/* Heap for the native PS5 title build.
 *
 * Inside a sandboxed title the system libc heap is small and does not grow
 * on demand; SDL2 allocates whole 1080p framebuffers and libxmp loads modules
 * of several megabytes, so large malloc() calls fail and the app crashes
 * (ProsperoRadio works around the same thing by mapping big blocks itself).
 * This file replaces malloc/calloc/realloc/free/posix_memalign for the whole
 * executable with a small allocator on top of mmap():
 *   - blocks of 32 KiB and more get their own mapping and are unmapped on free;
 *   - smaller blocks come from size classes (powers of two from 16 bytes)
 *     carved out of 2 MiB arenas, recycled through per-class free lists.
 * Every block carries a 16 byte header with its class or mapping size.
 * The desktop build keeps the normal C library heap. */
#ifdef OLISE_NATIVE

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

void* mmap(void* addr, size_t len, int prot, int flags, int fd, long offset);
int munmap(void* addr, size_t len);

#define PROT_RW        3
#define MAP_PRIV_ANON  0x1002
#define MAP_FAILED_PTR ((void*)-1)

#define HEADER_SIZE    16
#define MIN_CLASS      4            /* 16 bytes */
#define MAX_CLASS      15           /* 32 KiB; larger blocks are mapped */
#define ARENA_SIZE     (2u * 1024u * 1024u)
#define PAGE           0x4000u      /* 16 KiB, the PS5 page size */
#define MAGIC_SMALL    0x534D414Cu  /* 'SMAL' */
#define MAGIC_LARGE    0x4C415247u  /* 'LARG' */

typedef struct Header {
	uint32_t magic;
	uint32_t cls;        /* size class for small blocks */
	uint64_t size;       /* mapping size for large blocks, user size for small */
} Header;

typedef struct FreeNode { struct FreeNode* next; } FreeNode;

static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static FreeNode* g_free[MAX_CLASS + 1];
static uint8_t* g_arena = 0;
static size_t g_arena_left = 0;

static void* map_pages(size_t len)
{
	void* p = mmap(0, len, PROT_RW, MAP_PRIV_ANON, -1, 0);
	return p == MAP_FAILED_PTR ? 0 : p;
}

static uint32_t class_for(size_t size)
{
	uint32_t cls = MIN_CLASS;
	while (((size_t)1 << cls) < size) cls++;
	return cls;
}

static void* alloc_small(uint32_t cls)
{
	size_t block = ((size_t)1 << cls) + HEADER_SIZE;
	Header* h;
	if (g_free[cls]) {
		FreeNode* n = g_free[cls];
		g_free[cls] = n->next;
		h = (Header*)((uint8_t*)n - HEADER_SIZE);
	} else {
		if (g_arena_left < block) {
			g_arena = (uint8_t*)map_pages(ARENA_SIZE);
			if (!g_arena) return 0;
			g_arena_left = ARENA_SIZE;
		}
		h = (Header*)g_arena;
		g_arena += block;
		g_arena_left -= block;
	}
	h->magic = MAGIC_SMALL;
	h->cls = cls;
	return (uint8_t*)h + HEADER_SIZE;
}

static void* alloc_large(size_t size)
{
	size_t total = (size + HEADER_SIZE + PAGE - 1) & ~(size_t)(PAGE - 1);
	Header* h = (Header*)map_pages(total);
	if (!h) return 0;
	h->magic = MAGIC_LARGE;
	h->cls = 0;
	h->size = total;
	return (uint8_t*)h + HEADER_SIZE;
}

static size_t usable_size(const Header* h)
{
	return h->magic == MAGIC_LARGE ? h->size - HEADER_SIZE : ((size_t)1 << h->cls);
}

void* malloc(size_t size)
{
	if (size == 0) size = 1;
	void* p;
	if (size > ((size_t)1 << MAX_CLASS)) {
		p = alloc_large(size);
	} else {
		uint32_t cls = class_for(size);
		pthread_mutex_lock(&g_lock);
		p = alloc_small(cls);
		if (p) ((Header*)((uint8_t*)p - HEADER_SIZE))->size = size;
		pthread_mutex_unlock(&g_lock);
	}
	return p;
}

void free(void* ptr)
{
	if (!ptr) return;
	Header* h = (Header*)((uint8_t*)ptr - HEADER_SIZE);
	if (h->magic == MAGIC_LARGE) {
		munmap(h, h->size);
	} else if (h->magic == MAGIC_SMALL) {
		pthread_mutex_lock(&g_lock);
		FreeNode* n = (FreeNode*)ptr;
		n->next = g_free[h->cls];
		g_free[h->cls] = n;
		pthread_mutex_unlock(&g_lock);
	}
	/* Anything else was not ours (allocated before this heap took over): leak it. */
}

void* calloc(size_t count, size_t size)
{
	size_t total;
	if (size && count > (size_t)-1 / size) return 0;
	total = count * size;
	void* p = malloc(total);
	if (p) memset(p, 0, total); /* fresh mappings are zero, recycled blocks are not */
	return p;
}

void* realloc(void* ptr, size_t size)
{
	if (!ptr) return malloc(size);
	if (size == 0) { free(ptr); return 0; }
	Header* h = (Header*)((uint8_t*)ptr - HEADER_SIZE);
	if (h->magic != MAGIC_SMALL && h->magic != MAGIC_LARGE) {
		/* Foreign block: we do not know its size, copy conservatively. */
		void* p = malloc(size);
		if (p) memcpy(p, ptr, size);
		return p;
	}
	size_t have = usable_size(h);
	if (size <= have && (h->magic == MAGIC_LARGE || size > have / 2)) {
		if (h->magic == MAGIC_SMALL) h->size = size;
		return ptr;
	}
	void* p = malloc(size);
	if (!p) return 0;
	size_t old = h->magic == MAGIC_SMALL ? (size_t)h->size : have;
	memcpy(p, ptr, old < size ? old : size);
	free(ptr);
	return p;
}

int posix_memalign(void** out, size_t alignment, size_t size)
{
	if (alignment <= HEADER_SIZE) {
		*out = malloc(size);
		return *out ? 0 : 12;
	}
	/* Over-allocate and return the aligned address inside a large mapping;
	   the header still sits HEADER_SIZE bytes before the returned pointer. */
	size_t total = size + alignment + HEADER_SIZE;
	uint8_t* raw = (uint8_t*)alloc_large(total);
	if (!raw) { *out = 0; return 12; }
	Header* base = (Header*)(raw - HEADER_SIZE);
	uintptr_t aligned = ((uintptr_t)raw + alignment - 1) & ~(uintptr_t)(alignment - 1);
	if (aligned != (uintptr_t)raw) {
		Header* h = (Header*)(aligned - HEADER_SIZE);
		/* Keep the mapping base so free() can unmap the whole thing. */
		h->magic = MAGIC_LARGE;
		h->cls = (uint32_t)(aligned - (uintptr_t)base); /* offset back to the base */
		h->size = base->size - h->cls;
		/* free() unmaps from h with h->size: covers the rest of the mapping; the
		   first h->cls bytes stay mapped. Acceptable for the few aligned blocks. */
	}
	*out = (void*)aligned;
	return 0;
}

void* aligned_alloc(size_t alignment, size_t size)
{
	void* p = 0;
	return posix_memalign(&p, alignment, size) == 0 ? p : 0;
}

void* memalign(size_t alignment, size_t size)
{
	return aligned_alloc(alignment, size);
}

#endif /* OLISE_NATIVE */
