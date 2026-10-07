#ifdef OLISE_GL
/* OpenGL display backend for the native PS5 title.
 *
 * The software renderer keeps drawing the FastTracker II interface into the
 * 640x360 framebuffer; this backend uploads that buffer as a texture and
 * draws it over the whole screen through EGL and OpenGL 4.6 Core, using the
 * PS5 OpenGL SDK (Copyright (C) 2026 BlackBearReloaded, GPL-3.0-or-later).
 *
 * Going through the GPU costs nothing here and buys the visualiser: once the
 * frame is a texture, a shader can warp it, feed it back into itself and mix
 * in the waveform. This file only does the plain presentation; the visualiser
 * builds on the same texture.
 *
 * Shaders are compiled once at startup and checked there. The SDK calls
 * _Exit() on a GPU submission error without running exit handlers, so a
 * failure must be caught while the app can still report it, not mid-playback. */
#include "display.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Without this <GL/gl.h> declares only the OpenGL 1.1 entry points, and
 * everything from shaders onwards is missing. The SDK's own examples set it
 * on the command line; keeping it here makes the file build on its own. */
#define GL_GLEXT_PROTOTYPES 1

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>

#include "dbg.h"
#include "gfx.h"
#include "visualiser.h"
#ifdef OLISE_PROJECTM
#include "milkdrop.h"
#endif

extern "C" {
/* The splash screen holds the video output until it is dismissed; EGL cannot
 * take over while it does. The VideoOut backend calls this for the same
 * reason. The SDK's own examples skip it because they run without a splash. */
int sceSystemServiceHideSplashScreen(void);
}

/* libxmp keeps whole modules in memory and the Mod Archive cap is 16 MiB, so
 * ask the SDK allocator for more than its 128 MiB default. */
extern "C" const size_t ps5_opengl_heap_size = 256u * 1024u * 1024u;

namespace {

EGLDisplay g_display = EGL_NO_DISPLAY;
EGLSurface g_surface = EGL_NO_SURFACE;
EGLContext g_context = EGL_NO_CONTEXT;
GLuint g_program = 0;
GLuint g_vao = 0;
GLuint g_texture = 0;
GLint g_loc_crt = -1;
GLint g_loc_opacity = -1;
int g_screen_w = 0, g_screen_h = 0;
bool g_ready = false;

/* One triangle covering the screen: cheaper than two and avoids the seam along
 * the diagonal. gl_VertexID picks the corners, so there is no vertex buffer. */
const char* const VERTEX_SOURCE =
	"#version 460 core\n"
	"out vec2 uv;\n"
	"void main() {\n"
	"  vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);\n"
	"  uv = vec2(p.x, 1.0 - p.y);\n"
	"  gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);\n"
	"}\n";

/* Nearest sampling keeps the pixels crisp, the way the VideoOut backend scaled
 * them. The scanlines darken every second output row, as the CRT option did. */
const char* const FRAGMENT_SOURCE =
	"#version 460 core\n"
	"in vec2 uv;\n"
	"layout(location=0) out vec4 colour;\n"
	"uniform sampler2D frame;\n"
	"uniform int crt;\n"
	"uniform float opacity;\n"
	"void main() {\n"
	"  vec3 c = texture(frame, uv).bgr;\n"
	"  if (crt != 0 && (int(gl_FragCoord.y) & 1) == 1) c *= 0.75;\n"
	   /* Over the visualiser the interface is drawn semi-transparent, and its
	      black background drops out entirely so the effect shows through. */
	"  float a = opacity;\n"
	"  if (opacity < 1.0) a *= clamp(max(c.r, max(c.g, c.b)) * 3.0, 0.0, 1.0);\n"
	"  colour = vec4(c, a);\n"
	"}\n";

bool fail(char* err, size_t err_len, const char* what)
{
	snprintf(err, err_len, "opengl: %s", what);
	return false;
}

/* Returns 0 and writes the compiler log into err on failure. */
GLuint compile(GLenum type, const char* source, char* err, size_t err_len)
{
	GLuint shader = glCreateShader(type);
	GLint ok = GL_FALSE;

	glShaderSource(shader, 1, &source, nullptr);
	glCompileShader(shader);
	glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		char log[512] = {0};
		GLsizei len = 0;
		glGetShaderInfoLog(shader, sizeof(log) - 1, &len, log);
		snprintf(err, err_len, "opengl: %s shader: %.*s",
		         type == GL_VERTEX_SHADER ? "vertex" : "fragment", (int)len, log);
		glDeleteShader(shader);
		return 0;
	}
	return shader;
}

bool build_program(char* err, size_t err_len)
{
	GLuint vs = compile(GL_VERTEX_SHADER, VERTEX_SOURCE, err, err_len);
	if (!vs) return false;

	GLuint fs = compile(GL_FRAGMENT_SHADER, FRAGMENT_SOURCE, err, err_len);
	if (!fs) { glDeleteShader(vs); return false; }

	g_program = glCreateProgram();
	glAttachShader(g_program, vs);
	glAttachShader(g_program, fs);
	glLinkProgram(g_program);
	glDeleteShader(vs);
	glDeleteShader(fs);

	GLint linked = GL_FALSE;
	glGetProgramiv(g_program, GL_LINK_STATUS, &linked);
	if (!linked) {
		char log[512] = {0};
		GLsizei len = 0;
		glGetProgramInfoLog(g_program, sizeof(log) - 1, &len, log);
		snprintf(err, err_len, "opengl: link: %.*s", (int)len, log);
		glDeleteProgram(g_program);
		g_program = 0;
		return false;
	}

	glUseProgram(g_program);
	glUniform1i(glGetUniformLocation(g_program, "frame"), 0);
	g_loc_crt = glGetUniformLocation(g_program, "crt");
	g_loc_opacity = glGetUniformLocation(g_program, "opacity");
	return true;
}

} /* namespace */

bool display_init(char* err, size_t err_len)
{
	static const EGLint config_attributes[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 8,
		EGL_NONE,
	};
	static const EGLint context_attributes[] = {
		EGL_CONTEXT_MAJOR_VERSION_KHR, 4,
		EGL_CONTEXT_MINOR_VERSION_KHR, 6,
		EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR,
		EGL_NONE,
	};
	EGLConfig config = nullptr;
	EGLint major = 0, minor = 0, count = 0;

	(void)sceSystemServiceHideSplashScreen();

	g_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if (g_display == EGL_NO_DISPLAY) return fail(err, err_len, "no display");
	if (!eglInitialize(g_display, &major, &minor)) return fail(err, err_len, "eglInitialize");
	if (!eglBindAPI(EGL_OPENGL_API)) return fail(err, err_len, "eglBindAPI");
	if (!eglChooseConfig(g_display, config_attributes, &config, 1, &count) || count < 1)
		return fail(err, err_len, "eglChooseConfig");

	g_surface = eglCreateWindowSurface(g_display, config, 0, nullptr);
	if (g_surface == EGL_NO_SURFACE) return fail(err, err_len, "eglCreateWindowSurface");

	g_context = eglCreateContext(g_display, config, EGL_NO_CONTEXT, context_attributes);
	if (g_context == EGL_NO_CONTEXT) return fail(err, err_len, "eglCreateContext");
	if (!eglMakeCurrent(g_display, g_surface, g_surface, g_context))
		return fail(err, err_len, "eglMakeCurrent");

	if (!build_program(err, err_len)) return false;

	/* Core profile draws nothing without a bound vertex array, even when the
	   shader builds its own vertices. */
	glGenVertexArrays(1, &g_vao);
	glBindVertexArray(g_vao);

	glGenTextures(1, &g_texture);
	glBindTexture(GL_TEXTURE_2D, g_texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, gfx::W, gfx::H, 0, GL_RGBA,
	             GL_UNSIGNED_BYTE, nullptr);

	EGLint width = 0, height = 0;
	eglQuerySurface(g_display, g_surface, EGL_WIDTH, &width);
	eglQuerySurface(g_display, g_surface, EGL_HEIGHT, &height);
	g_screen_w = width;
	g_screen_h = height;
	glViewport(0, 0, width, height);

	/* eglSwapBuffers already waits for vblank, so the frame pacing in main
	   must not sleep on top of it. */
	eglSwapInterval(g_display, 1);

	dbg_log("display: OpenGL %s, %dx%d",
	        (const char*)glGetString(GL_VERSION), width, height);
	g_ready = true;

	/* projectM is started separately by display_start_visualiser(): it reads
	   the preset folder, which only becomes listable after the app has asked
	   for elevation, and that happens later in main. */
	return true;
}

/* Set before display_present() when the visualiser should run underneath.
   Kept here rather than passed through display_present() so the interface
   stays the same for the software backends. */
static const vis::Frame* g_vis_frame = nullptr;
static float g_vis_opacity = 1.0f;

void display_set_visualiser(const vis::Frame* frame, float interface_opacity)
{
	g_vis_frame = frame;
	g_vis_opacity = interface_opacity;
}

void display_present(const uint32_t* fb, bool crt)
{
	if (!g_ready) return;

	bool visualised = false;
	if (g_vis_frame) {
#ifdef OLISE_PROJECTM
		/* Real MilkDrop presets when projectM started; the built-in effect
		   otherwise. One flag decides, so a missing preset folder or a
		   refused library simply falls back instead of failing. */
		if (milkdrop::available()) {
			milkdrop::add_audio(g_vis_frame->samples, g_vis_frame->sample_count);
			milkdrop::render();
			visualised = true;
		}
		if (!visualised)
#endif
		{
		/* The caller does not know the output size, so fill it in here. */
		vis::Frame frame = *g_vis_frame;
		frame.screen_w = g_screen_w;
		frame.screen_h = g_screen_h;
		visualised = vis::draw(frame);
		}
		/* draw() returning false means the GPU resources failed; fall back to
		   the plain interface rather than showing nothing. */
		if (!visualised) g_vis_frame = nullptr;
		/* The visualiser renders its feedback field at a quarter size and
		   leaves the viewport that small. Without this the interface would be
		   drawn into the bottom left corner of the screen. */
		glViewport(0, 0, g_screen_w, g_screen_h);
	}

	glBindTexture(GL_TEXTURE_2D, g_texture);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, gfx::W, gfx::H, GL_RGBA,
	                GL_UNSIGNED_BYTE, fb);

	glUseProgram(g_program);
	glUniform1i(g_loc_crt, crt ? 1 : 0);
	glUniform1f(g_loc_opacity, visualised ? g_vis_opacity : 1.0f);
	if (visualised) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	glBindVertexArray(g_vao);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	if (visualised) glDisable(GL_BLEND);

	eglSwapBuffers(g_display, g_surface);
}

bool display_has_gpu()
{
	return true;
}

void display_start_visualiser()
{
	if (!g_ready) return;
#ifdef OLISE_PROJECTM
	/* Needs the OpenGL context current, which it is here, and the preset
	   folder readable, which is why main calls this after elevation. */
	milkdrop::init(g_screen_w, g_screen_h);
#endif
}

void display_shutdown()
{
	if (g_display == EGL_NO_DISPLAY) return;
#ifdef OLISE_PROJECTM
	milkdrop::shutdown();
#endif
	eglMakeCurrent(g_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	if (g_context != EGL_NO_CONTEXT) eglDestroyContext(g_display, g_context);
	if (g_surface != EGL_NO_SURFACE) eglDestroySurface(g_display, g_surface);
	eglTerminate(g_display);
	g_display = EGL_NO_DISPLAY;
	g_ready = false;
}

bool display_screenshot(const uint32_t*, const char*)
{
	return false; /* desktop build only */
}

#endif /* OLISE_GL */
