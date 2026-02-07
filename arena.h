#ifndef ARENA_H
#define ARENA_H

#include <stdio.h>
#include <string.h> /* not necessary on windows. see if you can remove this on linux */
#include <stdint.h>
#include <stdlib.h> /* not necessary on windows. see if you can remove this on linux */

#define ARENA_PUSH_STRUCT(arena, T) (T*)arena_push(arena, sizeof(T), 0)
#define ARENA_PUSH_ARRAY(arena, T, n) (T*)arena_push(arena, sizeof(T) * n, 0)
#define ARENA_PUSH_STRUCT_ZERO(arena, T) (T*)arena_push(arena, sizeof(T), 1)
#define ARENA_PUSH_ARRAY_ZERO(arena, T, n) (T*)arena_push(arena, sizeof(T) * n, 1)

#if defined(ARENA_STATIC)
    #define ARENADEF static
#elif defined(_WIN32) || defined(_WIN64)
    #if defined(ARENA_BUILD_DLL)
        #define ARENADEF __declspec(dllexport)
    #elif defined(ARENA_USE_DLL)
        #define ARENADEF __declspec(dllimport)
    #else
        #define ARENADEF extern
    #endif
#else
    #define ARENADEF extern
#endif // ARENADEF

typedef struct {
    size_t pos;
    size_t committed_size;
    size_t page_size;
    size_t reserved_size;
} mem_arena;

typedef struct {
    mem_arena *arena;
    size_t start_pos;
}arena_temp;

typedef int arena_bool;
#ifdef __cplusplus
extern "C" {
#endif // cplusplus


ARENADEF mem_arena *arena_init(size_t size);
ARENADEF void *arena_push(mem_arena *arena, size_t size, arena_bool zero_out_the_memory);
ARENADEF void arena_pop(mem_arena *arena, size_t size);
ARENADEF void arena_pop_to(mem_arena *arena, size_t pos);
ARENADEF void arena_clear(mem_arena *arena);
ARENADEF void arena_destroy(mem_arena *arena);
ARENADEF int arena_reset_region(const mem_arena *arena, void *region_start, size_t region_size);

#ifdef __cplusplus
}
#endif // cplusplus

#endif // ARENA_H
#define ARENA_IMPLEMENTATION
#ifdef ARENA_IMPLEMENTATION

#ifndef ARENA_ALIGNMENT
#define ARENA_ALIGNMENT 16
#endif

#define ALIGN_UP(x, a) (((x) + ((a) - 1)) & ~((a) - 1))

#define ARENA_MIN(a, b) (((a) < (b)) ? (a) : (b))
#define ARENA_MAX(a, b) (((a) > (b)) ? (a) : (b))

#define ARENA_BASE_POS sizeof(mem_arena)

static void *arena_memset(void *buf, int value, size_t count) {
    unsigned char *p = buf;
    unsigned char v = (unsigned char)value;
	size_t i = 0;

    for (; i < count; i++) {
        p[i] = v;
    }

    return buf;
}
/* ===========================================================
   ===============   WINDOWS IMPLEMENTATION   =================
   =========================================================== */
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>

ARENADEF mem_arena *arena_init(size_t size) {
    SYSTEM_INFO sys_info;
    GetSystemInfo(&sys_info);
    size_t page_size = sys_info.dwPageSize;
    size = ALIGN_UP(size, page_size);

    mem_arena *arena = (mem_arena*)VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_READWRITE);
    if (!arena) {
        fprintf(stderr, "VirtualAlloc reserve failed: %lu\n", GetLastError());
        exit(EXIT_FAILURE);
    }

    size_t initial_commit = ARENA_MAX(page_size, ARENA_BASE_POS);
    initial_commit = ALIGN_UP(initial_commit, page_size);
    
    void *commit = VirtualAlloc(arena, initial_commit, MEM_COMMIT, PAGE_READWRITE);
    if (!commit) {
        fprintf(stderr, "VirtualAlloc initial commit failed: %lu\n", GetLastError());
        VirtualFree(arena, 0, MEM_RELEASE);
        exit(EXIT_FAILURE);
    }

    arena->page_size = page_size;
    arena->reserved_size = size;
    arena->pos = ARENA_BASE_POS;
    arena->committed_size = initial_commit;

    return arena;
}

ARENADEF void* arena_push(mem_arena* arena, size_t size, arena_bool zero_out_the_memory) {
    size_t base = (size_t)arena + arena->pos;
    uintptr_t aligned = ALIGN_UP(base, ARENA_ALIGNMENT);
    size_t padding = aligned - base;
    size_t total_size = padding + size;
    size_t required = arena->pos + total_size;

    if (required > arena->reserved_size) {
        fprintf(stderr, "ERROR: Allocation exceeds arena reserved_size!\n");
        return NULL;
    }

    if (required > arena->committed_size) {
        size_t new_commit_end = ALIGN_UP(required, arena->page_size);
        size_t commit_amount = new_commit_end - arena->committed_size;

        void* result = VirtualAlloc((char*)arena + arena->committed_size,
                                    commit_amount, MEM_COMMIT, PAGE_READWRITE);
        if (!result) {
            fprintf(stderr, "VirtualAlloc commit failed: %lu\n", GetLastError());
            exit(EXIT_FAILURE);
        }

        arena->committed_size = new_commit_end;
    }

    arena->pos += total_size;
    if(zero_out_the_memory) {
        arena_memset((void*)aligned, 0, total_size);
    }
    return (void*)aligned;
}

ARENADEF void arena_pop(mem_arena *arena, size_t size) {
    if (arena->pos - ARENA_BASE_POS < size) {
        arena->pos = ARENA_BASE_POS;
    } else {
        arena->pos -= size;
    }
}

ARENADEF void arena_pop_to(mem_arena *arena, size_t pos) {
    pos = ARENA_MAX(pos, ARENA_BASE_POS);
    if (pos < arena->pos) {
        arena->pos = pos;
    }
}

ARENADEF void arena_clear(mem_arena *arena) {
    if (arena) {
        size_t reset_size = arena->committed_size - ARENA_BASE_POS;
        if (reset_size > 0) {
            VirtualAlloc((char*)arena + ARENA_BASE_POS, reset_size, MEM_RESET, PAGE_READWRITE);
        }
        arena->pos = ARENA_BASE_POS;
    }
}

ARENADEF void arena_destroy(mem_arena *arena) {
    if (arena) {
        arena->pos = 0;
        arena->committed_size = 0;
        arena->reserved_size = 0;
        arena->page_size = 0;
        VirtualFree(arena, 0, MEM_RELEASE);
    }
}

ARENADEF int arena_reset_region(const mem_arena *arena, void *region_start, size_t region_size) {
    uintptr_t arena_start = (uintptr_t)arena + ARENA_BASE_POS;
    uintptr_t arena_end = (uintptr_t)arena + arena->reserved_size;
    uintptr_t region_addr = (uintptr_t)region_start;

    if (region_addr >= arena_start && region_addr + region_size <= arena_end) {
        arena_memset(region_start, 0, region_size);
        return 0;
    }
    return -1;
}

#else
/* ===========================================================
   ===============   POSIX IMPLEMENTATION   ===================
   =========================================================== */

#include <sys/mman.h>
#include <unistd.h>

ARENADEF mem_arena *arena_init(size_t size) {
    size_t page_size = (size_t)sysconf(_SC_PAGESIZE);
    size = ALIGN_UP(size, page_size);

    mem_arena *arena = (mem_arena*)mmap(NULL, size, PROT_READ | PROT_WRITE,
                                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (arena == MAP_FAILED) {
        perror("mmap failed");
        exit(EXIT_FAILURE);
    }

    arena->page_size = page_size;
    arena->reserved_size = size;
    arena->pos = ARENA_BASE_POS;
    arena->committed_size = size; // mmap commits all at once on most systems

    return arena;
}

ARENADEF void* arena_push(mem_arena* arena, size_t size, arena_bool zero_out_the_memory) {
    size_t base = (size_t)arena + arena->pos;
    uintptr_t aligned = ALIGN_UP(base, ARENA_ALIGNMENT);
    size_t padding = aligned - base;
    size_t total_size = padding + size;
    size_t required = arena->pos + total_size;

    if (required > arena->reserved_size) {
        fprintf(stderr, "ERROR: Allocation exceeds arena reserved_size!\n");
        return NULL;
    }

    arena->pos += total_size;

    if(zero_out_the_memory) {
        arena_memset((void*)aligned, 0, total_size);
    }
    return (void*)aligned;
}

ARENADEF void arena_pop(mem_arena *arena, size_t size) {
    if (arena->pos - ARENA_BASE_POS < size) {
        arena->pos = ARENA_BASE_POS;
    } else {
        arena->pos -= size;
    }
}

ARENADEF void arena_pop_to(mem_arena *arena, size_t pos) {
    pos = ARENA_MAX(pos, ARENA_BASE_POS);
    if (pos < arena->pos) {
        arena->pos = pos;
    }
}

ARENADEF void arena_clear(mem_arena *arena) {
    if (arena) {
        size_t reset_size = arena->pos - ARENA_BASE_POS;
        if (reset_size > 0) {
            madvise((char*)arena + ARENA_BASE_POS, reset_size, MADV_DONTNEED);
        }
        arena->pos = ARENA_BASE_POS;
    }
}

ARENADEF void arena_destroy(mem_arena *arena) {
    if (arena) {
        munmap(arena, arena->reserved_size);
    }
}

ARENADEF int arena_reset_region(const mem_arena *arena, void *region_start, size_t region_size) {
    uintptr_t arena_start = (uintptr_t)arena + ARENA_BASE_POS;
    uintptr_t arena_end = (uintptr_t)arena + arena->reserved_size;
    uintptr_t region_addr = (uintptr_t)region_start;

    if (region_addr >= arena_start && region_addr + region_size <= arena_end) {
        arena_memset(region_start, 0, region_size);
        return 0;
    }
    return -1;
}

#endif // _WIN32 || _WIN64

#endif // ARENA_IMPLEMENTATION
