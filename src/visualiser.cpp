#ifdef OLISE_GL
/* MilkDrop style visualiser.
 *
 * MilkDrop's whole look comes from one idea: keep the previous frame, warp it
 * a little, dim it, and draw fresh shapes on top. Repeat sixty times a second
 * and the warp turns into tunnels, spirals and smoke. This file does exactly
 * that, with two textures that swap roles every frame:
 *
 *   previous frame --> warp shader --> current frame --> screen
 *                           ^                |
 *                           +----- swap -----+
 *
 * The warp runs at a quarter of the output size. The feedback hides the lower
 * resolution (every pixel is smeared over the next frames anyway) and it keeps
 * the fragment cost low enough to stay at 60 fps next to the music.
 *
 * Presets differ only in the uniforms they feed the same shader, so switching
 * costs nothing and cannot fail at runtime: every shader is compiled once at
 * startup, where an error can still be reported. */
#include "visualiser.h"

#define GL_GLEXT_PROTOTYPES 1

#include <math.h>
#include <stddef.h>
#include <stdio.h>

#include <GL/gl.h>

#include "dbg.h"

namespace vis {
namespace {

/* Quarter of 1080p. Large enough that the warp reads smooth, small enough to
   leave the GPU idle most of the frame. */
constexpr int FIELD_W = 480;
constexpr int FIELD_H = 270;
constexpr int WAVE_POINTS = 512;

struct Preset {
	const char* name;
	float zoom;        /* >1 pulls the image inward, <1 pushes it out */
	float rotate;      /* radians per second */
	float decay;       /* how much of the previous frame survives */
	float warp;        /* strength of the sine distortion */
	float wave_scale;  /* height of the waveform */
	float hue;         /* 0..1, base colour of the wave */
	int   mode;        /* 0 tunnel, 1 spiral, 2 ripple */
};

/* The preset list. Each one is a set of uniforms over the seven warp modes,
   so adding one costs nothing at runtime. They are ordered roughly from calm
   to wild, and the names are what the toast shows.

   zoom >1 pulls inward (a tunnel going away), <1 pushes outward (an explosion)
   decay near 1.0 leaves long trails, lower wipes the screen faster
   warp is the strength of the extra distortion on top of zoom and rotation  */
const Preset PRESETS[] = {
	/*  name          zoom    rotate  decay   warp    wave   hue   mode */
	{ "TUNNEL",      1.022f,  0.10f, 0.955f, 0.012f, 0.42f, 0.55f, 0 },
	{ "SLOW BURN",   1.006f,  0.03f, 0.975f, 0.008f, 0.55f, 0.08f, 0 },
	{ "DEEP SPACE",  1.040f,  0.05f, 0.965f, 0.006f, 0.26f, 0.62f, 0 },
	{ "HEARTBEAT",   1.012f, -0.04f, 0.958f, 0.016f, 0.62f, 0.00f, 0 },
	{ "SPIRAL",      1.014f,  0.55f, 0.962f, 0.020f, 0.34f, 0.80f, 1 },
	{ "VORTEX",      1.030f,  0.95f, 0.945f, 0.028f, 0.30f, 0.95f, 1 },
	{ "WHIRLPOOL",   1.008f,  1.60f, 0.968f, 0.014f, 0.38f, 0.48f, 1 },
	{ "UNWIND",      0.990f, -1.10f, 0.952f, 0.022f, 0.44f, 0.15f, 1 },
	{ "RIPPLE",      0.996f, -0.08f, 0.950f, 0.040f, 0.50f, 0.30f, 2 },
	{ "EXPLODE",     0.980f, -0.40f, 0.940f, 0.035f, 0.46f, 0.68f, 2 },
	{ "SONAR",       1.002f,  0.02f, 0.972f, 0.055f, 0.52f, 0.42f, 2 },
	{ "SHOCKWAVE",   0.972f,  0.25f, 0.936f, 0.048f, 0.40f, 0.88f, 2 },
	{ "KALEIDO",     1.016f,  0.20f, 0.960f, 0.018f, 0.36f, 0.72f, 3 },
	{ "STAINED",     1.004f,  0.08f, 0.970f, 0.012f, 0.48f, 0.22f, 3 },
	{ "MANDALA",     1.026f,  0.45f, 0.950f, 0.024f, 0.32f, 0.92f, 3 },
	{ "SWIRL",       1.010f,  0.70f, 0.964f, 0.020f, 0.40f, 0.58f, 4 },
	{ "CHURN",       1.018f,  1.30f, 0.948f, 0.032f, 0.34f, 0.05f, 4 },
	{ "BREATHE",     0.998f,  0.30f, 0.974f, 0.026f, 0.50f, 0.35f, 4 },
	{ "FOLD",        1.006f,  0.06f, 0.958f, 0.030f, 0.44f, 0.78f, 5 },
	{ "LATTICE",     1.000f,  0.00f, 0.968f, 0.042f, 0.38f, 0.50f, 5 },
	{ "ORIGAMI",     0.994f, -0.12f, 0.946f, 0.038f, 0.46f, 0.12f, 5 },
	{ "INTERFERE",   1.012f,  0.35f, 0.962f, 0.022f, 0.42f, 0.65f, 6 },
	{ "PULSAR",      1.024f, -0.60f, 0.942f, 0.030f, 0.36f, 0.85f, 6 },
	{ "RIPTIDE",     1.002f,  0.90f, 0.966f, 0.026f, 0.48f, 0.28f, 6 },
};
constexpr int PRESET_COUNT = (int)(sizeof(PRESETS) / sizeof(PRESETS[0]));

const char* const WARP_VERTEX =
	"#version 460 core\n"
	"out vec2 uv;\n"
	"void main() {\n"
	"  vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);\n"
	"  uv = p;\n"
	"  gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);\n"
	"}\n";

/* The warp itself. Reads the previous frame at a moved position and dims it.
   Everything that makes a preset is a uniform, so one shader covers them all. */
const char* const WARP_FRAGMENT =
	"#version 460 core\n"
	"in vec2 uv;\n"
	"layout(location=0) out vec4 colour;\n"
	"uniform sampler2D previous;\n"
	"uniform float zoom;\n"
	"uniform float angle;\n"
	"uniform float decay;\n"
	"uniform float warp;\n"
	"uniform float time;\n"
	"uniform int mode;\n"
	"void main() {\n"
	"  vec2 c = uv - 0.5;\n"
	"  float r = length(c);\n"
	"  if (mode == 1) {\n"
	       /* Spiral: rotate more the further out, so the image winds up. */
	"    float a = angle * (1.0 + r * 2.0);\n"
	"    float s = sin(a), k = cos(a);\n"
	"    c = mat2(k, -s, s, k) * c;\n"
	"  } else if (mode == 2) {\n"
	       /* Ripple: push along the radius with a travelling wave. */
	"    c += normalize(c + 1e-6) * sin(r * 24.0 - time * 3.0) * warp;\n"
	"  } else if (mode == 3) {\n"
	       /* Kaleidoscope: fold the plane into a wedge before warping, which
	          turns any motion into sixfold symmetry. */
	"    float a = atan(c.y, c.x), seg = 3.14159265 / 3.0;\n"
	"    a = abs(mod(a + angle, seg * 2.0) - seg);\n"
	"    c = vec2(cos(a), sin(a)) * r;\n"
	"  } else if (mode == 4) {\n"
	       /* Swirl: rotation that falls off towards the edge, so the middle
	          churns while the border stays put. */
	"    float a = angle * 3.0 / (1.0 + r * 6.0);\n"
	"    float s = sin(a), k = cos(a);\n"
	"    c = mat2(k, -s, s, k) * c;\n"
	"    c *= 1.0 + sin(time * 0.7) * warp;\n"
	"  } else if (mode == 5) {\n"
	       /* Grid: warp along the axes instead of the radius, which keeps
	          rectangles and reads as folding paper. */
	"    c += vec2(sin(c.y * 18.0 + time * 1.7), sin(c.x * 18.0 - time * 1.3)) * warp * 1.6;\n"
	"  } else if (mode == 6) {\n"
	       /* Dual: two counter rotating pulls that beat against each other. */
	"    float a = angle + sin(r * 9.0 - time) * 0.5;\n"
	"    float s = sin(a), k = cos(a);\n"
	"    c = mat2(k, -s, s, k) * c;\n"
	"  } else {\n"
	       /* Tunnel: a gentle twist that grows towards the edges. */
	"    float a = angle + r * 0.6;\n"
	"    float s = sin(a), k = cos(a);\n"
	"    c = mat2(k, -s, s, k) * c;\n"
	"  }\n"
	"  c *= zoom;\n"
	       /* A little sideways shear keeps straight lines from forming. */
	"  c += vec2(sin(uv.y * 8.0 + time), cos(uv.x * 8.0 - time)) * warp * 0.35;\n"
	"  vec3 prev = texture(previous, c + 0.5).rgb;\n"
	       /* Dim the blue a touch slower: the trail cools towards blue
	          instead of going grey, which reads better on a TV. */
	"  colour = vec4(prev * vec3(decay, decay, decay * 1.006), 1.0);\n"
	"}\n";

const char* const WAVE_VERTEX =
	"#version 460 core\n"
	"layout(location=0) in float sample_value;\n"
	"uniform float scale;\n"
	"uniform float spread;\n"
	"void main() {\n"
	"  float x = float(gl_VertexID) / float(gl_BaseVertex + 511);\n"
	"  gl_Position = vec4(x * 2.0 - 1.0, sample_value * scale * spread, 0.0, 1.0);\n"
	"}\n";

const char* const WAVE_FRAGMENT =
	"#version 460 core\n"
	"layout(location=0) out vec4 colour;\n"
	"uniform vec3 tint;\n"
	"void main() { colour = vec4(tint, 1.0); }\n";

/* Draws the field to the screen, brightening with the music. */
const char* const PRESENT_FRAGMENT =
	"#version 460 core\n"
	"in vec2 uv;\n"
	"layout(location=0) out vec4 colour;\n"
	"uniform sampler2D field;\n"
	"uniform float gain;\n"
	"void main() {\n"
	"  vec3 c = texture(field, uv).rgb * gain;\n"
	       /* A soft vignette keeps the eye on the middle. */
	"  float v = 1.0 - length(uv - 0.5) * 0.65;\n"
	"  colour = vec4(c * v, 1.0);\n"
	"}\n";

GLuint g_field[2] = {0, 0};
GLuint g_fbo[2] = {0, 0};
GLuint g_warp = 0, g_wave = 0, g_present = 0;
GLuint g_vao = 0, g_wave_vao = 0, g_wave_vbo = 0;
int g_front = 0;        /* index of the texture holding the newest frame */
bool g_ready = false;
bool g_failed = false;  /* set once, so a failure is not retried every frame */

GLint g_warp_zoom, g_warp_angle, g_warp_decay, g_warp_warp, g_warp_time, g_warp_mode;
GLint g_wave_scale, g_wave_spread, g_wave_tint;
GLint g_present_gain;

GLuint compile(GLenum type, const char* source)
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
		dbg_log("visualiser: shader: %.*s", (int)len, log);
		glDeleteShader(shader);
		return 0;
	}
	return shader;
}

GLuint link_program(const char* vertex_source, const char* fragment_source)
{
	GLuint vs = compile(GL_VERTEX_SHADER, vertex_source);
	if (!vs) return 0;
	GLuint fs = compile(GL_FRAGMENT_SHADER, fragment_source);
	if (!fs) { glDeleteShader(vs); return 0; }

	GLuint program = glCreateProgram();
	glAttachShader(program, vs);
	glAttachShader(program, fs);
	glLinkProgram(program);
	glDeleteShader(vs);
	glDeleteShader(fs);

	GLint linked = GL_FALSE;
	glGetProgramiv(program, GL_LINK_STATUS, &linked);
	if (!linked) {
		char log[512] = {0};
		GLsizei len = 0;
		glGetProgramInfoLog(program, sizeof(log) - 1, &len, log);
		dbg_log("visualiser: link: %.*s", (int)len, log);
		glDeleteProgram(program);
		return 0;
	}
	return program;
}

bool init()
{
	glGenTextures(2, g_field);
	glGenFramebuffers(2, g_fbo);
	for (int i = 0; i < 2; i++) {
		glBindTexture(GL_TEXTURE_2D, g_field[i]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		/* Clamping stops the warp from wrapping the picture around the edge,
		   which would show as a hard seam once the zoom pulls inward. */
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, FIELD_W, FIELD_H, 0, GL_RGBA,
		             GL_UNSIGNED_BYTE, nullptr);
		glBindFramebuffer(GL_FRAMEBUFFER, g_fbo[i]);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
		                       g_field[i], 0);
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
			dbg_log("visualiser: framebuffer %d incomplete", i);
			return false;
		}
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	g_warp = link_program(WARP_VERTEX, WARP_FRAGMENT);
	g_wave = link_program(WAVE_VERTEX, WAVE_FRAGMENT);
	g_present = link_program(WARP_VERTEX, PRESENT_FRAGMENT);
	if (!g_warp || !g_wave || !g_present) return false;

	g_warp_zoom = glGetUniformLocation(g_warp, "zoom");
	g_warp_angle = glGetUniformLocation(g_warp, "angle");
	g_warp_decay = glGetUniformLocation(g_warp, "decay");
	g_warp_warp = glGetUniformLocation(g_warp, "warp");
	g_warp_time = glGetUniformLocation(g_warp, "time");
	g_warp_mode = glGetUniformLocation(g_warp, "mode");
	glUseProgram(g_warp);
	glUniform1i(glGetUniformLocation(g_warp, "previous"), 0);

	g_wave_scale = glGetUniformLocation(g_wave, "scale");
	g_wave_spread = glGetUniformLocation(g_wave, "spread");
	g_wave_tint = glGetUniformLocation(g_wave, "tint");

	g_present_gain = glGetUniformLocation(g_present, "gain");
	glUseProgram(g_present);
	glUniform1i(glGetUniformLocation(g_present, "field"), 0);

	glGenVertexArrays(1, &g_vao);

	glGenVertexArrays(1, &g_wave_vao);
	glBindVertexArray(g_wave_vao);
	glGenBuffers(1, &g_wave_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, g_wave_vbo);
	glBufferData(GL_ARRAY_BUFFER, WAVE_POINTS * sizeof(float), nullptr, GL_STREAM_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 1, GL_FLOAT, GL_FALSE, sizeof(float), nullptr);
	glBindVertexArray(0);

	dbg_log("visualiser: ready, %dx%d field, %d presets", FIELD_W, FIELD_H, PRESET_COUNT);
	return true;
}

/* A hue in 0..1 to an RGB triple. Keeps the wave colourful without a palette
   table, and the saturation stays high so it carries over the trail. */
void hue_to_rgb(float h, float out[3])
{
	h = h - floorf(h);
	const float r = fabsf(h * 6.0f - 3.0f) - 1.0f;
	const float g = 2.0f - fabsf(h * 6.0f - 2.0f);
	const float b = 2.0f - fabsf(h * 6.0f - 4.0f);
	out[0] = r < 0.0f ? 0.0f : (r > 1.0f ? 1.0f : r);
	out[1] = g < 0.0f ? 0.0f : (g > 1.0f ? 1.0f : g);
	out[2] = b < 0.0f ? 0.0f : (b > 1.0f ? 1.0f : b);
}

} /* namespace */

int preset_count() { return PRESET_COUNT; }

const char* preset_name(int index)
{
	if (index < 0 || index >= PRESET_COUNT) return "";
	return PRESETS[index].name;
}

bool draw(const Frame& f)
{
	if (g_failed) return false;
	if (!g_ready) {
		if (!init()) { g_failed = true; shutdown(); return false; }
		g_ready = true;
	}

	const Preset& p = PRESETS[(f.preset % PRESET_COUNT + PRESET_COUNT) % PRESET_COUNT];
	const float level = (float)f.level / 255.0f;
	const int back = g_front ^ 1;

	/* 1. Warp the previous frame into the other texture. A beat widens the
	      zoom for one frame, which reads as a push outward. */
	glBindFramebuffer(GL_FRAMEBUFFER, g_fbo[back]);
	glViewport(0, 0, FIELD_W, FIELD_H);
	glUseProgram(g_warp);
	glUniform1f(g_warp_zoom, p.zoom + (f.beat ? 0.02f : 0.0f) + level * 0.012f);
	glUniform1f(g_warp_angle, p.rotate * f.seconds * 0.25f);
	glUniform1f(g_warp_decay, p.decay);
	glUniform1f(g_warp_warp, p.warp * (0.6f + level));
	glUniform1f(g_warp_time, f.seconds);
	glUniform1i(g_warp_mode, p.mode);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, g_field[g_front]);
	glBindVertexArray(g_vao);
	glDrawArrays(GL_TRIANGLES, 0, 3);

	/* 2. Draw the waveform on top, in a colour that drifts over time. */
	if (f.samples && f.sample_count > 1) {
		float wave[WAVE_POINTS];
		const int n = f.sample_count < WAVE_POINTS ? f.sample_count : WAVE_POINTS;
		for (int i = 0; i < n; i++) wave[i] = (float)f.samples[i] / 32768.0f;
		for (int i = n; i < WAVE_POINTS; i++) wave[i] = 0.0f;

		glBindVertexArray(g_wave_vao);
		glBindBuffer(GL_ARRAY_BUFFER, g_wave_vbo);
		glBufferSubData(GL_ARRAY_BUFFER, 0, WAVE_POINTS * sizeof(float), wave);

		float tint[3];
		hue_to_rgb(p.hue + f.seconds * 0.03f, tint);
		glUseProgram(g_wave);
		glUniform1f(g_wave_scale, p.wave_scale);
		glUniform1f(g_wave_spread, 0.55f + level * 0.9f);
		glUniform3f(g_wave_tint, tint[0], tint[1], tint[2]);
		/* Additive so overlapping passes brighten instead of replacing, the
		   way the original does it. */
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		glDrawArrays(GL_LINE_STRIP, 0, WAVE_POINTS);
		glDisable(GL_BLEND);
	}

	/* 3. Show the result full screen. The field was rendered at a quarter
	      size, so the viewport has to grow back before this draw covers the
	      whole screen instead of one corner of it. */
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, f.screen_w, f.screen_h);
	glUseProgram(g_present);
	glUniform1f(g_present_gain, 1.0f + level * 0.45f);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, g_field[back]);
	glBindVertexArray(g_vao);
	glDrawArrays(GL_TRIANGLES, 0, 3);

	g_front = back;
	return true;
}

void shutdown()
{
	if (g_wave_vbo) { glDeleteBuffers(1, &g_wave_vbo); g_wave_vbo = 0; }
	if (g_wave_vao) { glDeleteVertexArrays(1, &g_wave_vao); g_wave_vao = 0; }
	if (g_vao) { glDeleteVertexArrays(1, &g_vao); g_vao = 0; }
	if (g_warp) { glDeleteProgram(g_warp); g_warp = 0; }
	if (g_wave) { glDeleteProgram(g_wave); g_wave = 0; }
	if (g_present) { glDeleteProgram(g_present); g_present = 0; }
	if (g_fbo[0] || g_fbo[1]) { glDeleteFramebuffers(2, g_fbo); g_fbo[0] = g_fbo[1] = 0; }
	if (g_field[0] || g_field[1]) { glDeleteTextures(2, g_field); g_field[0] = g_field[1] = 0; }
	g_ready = false;
}

} /* namespace vis */

#endif /* OLISE_GL */
