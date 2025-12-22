#ifndef ARENA_H
#define ARENA_H

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#define ARENA_PUSH_STRUCT(arena, T) (T*)arena_push(arena, sizeof(T))
#define ARENA_PUSH_ARRAY(arena, T, n) (T*)arena_push(arena, sizeof(T) * n)

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
    size_t memory_used;
    size_t committed_size;
    size_t page_size;
    size_t reserved_size;
    char *ptr;
} arena;

#ifdef __cplusplus
extern "C" {
#endif // cplusplus


ARENADEF arena arena_init(size_t size);
ARENADEF void *arena_push(arena *arena, size_t size);
ARENADEF void arena_pop(arena *arena, size_t size);
ARENADEF void arena_pop_to(arena *arena, size_t pos);
ARENADEF void arena_clear(arena *arena);
ARENADEF void arena_destroy(arena *arena);
ARENADEF int arena_reset_region(const arena *arena, void *region_start, size_t region_size);

#ifdef __cplusplus
}
#endif // cplusplus

#endif // ARENA_H
#ifdef ARENA_IMPLEMENTATION

#define ARENA_ALIGNMENT 16
#define ALIGN_UP(x, a) (((x) + ((a) - 1)) & ~((a) - 1))

#define ARENA_MIN(a, b) (((a) < (b)) ? (a) : (b))
#define ARENA_MAX(a, b) (((a) > (b)) ? (a) : (b))

/* ===========================================================
   ===============   WINDOWS IMPLEMENTATION   =================
   =========================================================== */
#if defined(_WIN32) || defined(_WIN64)

#include <windows.h>

ARENADEF arena arena_init(size_t size) {
    arena arena = {0};
    SYSTEM_INFO sys_info;
    GetSystemInfo(&sys_info);
    arena.page_size = (size_t)sys_info.dwPageSize;
    arena.reserved_size = ALIGN_UP(size, arena.page_size);

    arena.ptr = (char*)VirtualAlloc(NULL, arena.reserved_size, MEM_RESERVE, PAGE_READWRITE);
    if (!arena.ptr) {
        fprintf(stderr, "VirtualAlloc reserve failed: %lu\n", GetLastError());
        exit(EXIT_FAILURE);
    }

    return arena;
}

ARENADEF void* arena_push(arena* arena, size_t size) {
    uintptr_t base = (uintptr_t)(arena->ptr + arena->memory_used);
    uintptr_t aligned = ALIGN_UP(base, ARENA_ALIGNMENT);
    size_t padding = aligned - base;
    size_t total_size = padding + size;
    size_t required = arena->memory_used + total_size;

    if (required > arena->reserved_size) {
        fprintf(stderr, "ERROR: Allocation exceeds arena reserved_size!\n");
        return NULL;
    }

    if (required > arena->committed_size) {
        size_t new_commit_end = ALIGN_UP(required, arena.page_size);
        size_t commit_amount = new_commit_end - arena->committed_size;

        void* result = VirtualAlloc(arena->ptr + arena->committed_size,
                                    commit_amount, MEM_COMMIT, PAGE_READWRITE);
        if (!result) {
            fprintf(stderr, "VirtualAlloc commit failed: %lu\n", GetLastError());
            exit(EXIT_FAILURE);
        }

        arena->committed_size = new_commit_end;
    }

    arena->memory_used += total_size;
    return (void*)aligned;
}

ARENADEF void arena_pop(arena *arena, size_t size) {
    size = ARENA_MIN(size, arena->memory_used);
    arena->memory_used -= size;
}

ARENADEF void arena_pop_to(arena *arena, size_t pos) {
    size_t size = pos < arena->memory_used ? arena->memory_used - pos : 0;
    arena_pop(arena, size);
}

ARENADEF void arena_clear(arena *arena) {
    if (arena->ptr) {
        VirtualAlloc(arena->ptr, arena->committed_size, MEM_RESET, PAGE_READWRITE);
        arena->memory_used = 0;
    }
}

ARENADEF void arena_destroy(arena *arena) {
    if (arena->ptr) {
        VirtualFree(arena->ptr, 0, MEM_RELEASE);
        arena->ptr = NULL;
        arena->reserved_size = 0;
        arena->committed_size = 0;
        arena->memory_used = 0;
    }
}

ARENADEF int arena_reset_region(const arena *arena, void *region_start, size_t region_size) {
    uintptr_t arena_start = (uintptr_t)arena->ptr;
    uintptr_t arena_end = arena_start + arena->reserved_size;
    uintptr_t region_addr = (uintptr_t)region_start;

    if (region_addr >= arena_start && region_addr + region_size <= arena_end) {
        memset(region_start, 0, region_size);
        return 0;
    }
    return -1;
}


/* ===========================================================
   ==================   POSIX IMPLEMENTATION   ================
   =========================================================== */
#elif defined(__linux__) || defined(__unix__) || defined(__APPLE__) || defined(__MACH__)

#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>

ARENADEF arena arena_init(size_t size) {
    arena arena = {0};
    arena.page_size = (size_t)sysconf(_SC_PAGESIZE);
    size = ALIGN_UP(size, arena.page_size);

    arena.ptr = (char*)mmap(NULL, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (arena.ptr == MAP_FAILED) {
        perror("mmap reserve failed");
        exit(EXIT_FAILURE);
    }

    arena.reserved_size = size;
    return arena;
}

ARENADEF void *arena_push(arena *arena, size_t size) {
    uintptr_t base = (uintptr_t)(arena->ptr + arena->memory_used);
    uintptr_t aligned = ALIGN_UP(base, ARENA_ALIGNMENT);
    size_t padding = aligned - base;
    size_t total_size = padding + size;
    size_t required = arena->memory_used + total_size;
    arena->memory_used += total_size;

    if (required > arena->reserved_size) {
        fprintf(stderr, "ERROR: Allocation exceeds arena reserved_size!\n");
        return NULL;
    }

    if (required > arena->committed_size) {
        size_t new_commit_end = ALIGN_UP(required, arena->page_size);
        size_t commit_amount = new_commit_end - arena->committed_size;
        void* commit_ptr = arena->ptr + arena->committed_size;

        if (mprotect(commit_ptr, commit_amount, PROT_READ | PROT_WRITE) != 0) {
            perror("mprotect commit failed");
            exit(EXIT_FAILURE);
        }

        arena->committed_size = new_commit_end;
    }

    return (void*)aligned;
}

ARENADEF void arena_pop(arena *arena, size_t size) {
    size = ARENA_MIN(size, arena->memory_used);
    arena->memory_used -= size;
}

ARENADEF void arena_pop_to(arena *arena, size_t pos) {
    size_t size = pos < arena->memory_used ? arena->memory_used - pos : 0;
    arena_pop(arena, size);
}

ARENADEF void arena_clear(arena *arena) {
    if (arena->ptr) {
        madvise(arena->ptr, arena->committed_size, MADV_DONTNEED);
        arena->memory_used = 0;
    }
}

ARENADEF void arena_destroy(arena *arena) {
    if (arena->ptr) {
        munmap(arena->ptr, arena->reserved_size);
        arena->ptr = NULL;
        arena->reserved_size = 0;
        arena->committed_size = 0;
        arena->memory_used = 0;
    }
}

ARENADEF int arena_reset_region(const arena *arena, void *region_start, size_t region_size) {
    uintptr_t arena_start = (uintptr_t)arena->ptr;
    uintptr_t arena_end = arena_start + arena->reserved_size;
    uintptr_t region_addr = (uintptr_t)region_start;

    if (region_addr >= arena_start && region_addr + region_size <= arena_end) {
        memset(region_start, 0, region_size);
        return 0;
    }
    return -1;
}

#else
#error "Unsupported platform: this arena allocator only supports Windows and POSIX (Linux/macOS/BSD)"
#endif // platform-specific code

#endif // ARENA_IMPLEMENTATION
