#ifndef ARENA_H
#define ARENA_H
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#define ARENA_ALIGNMENT 16
#define ALIGN_UP(x, a) (((x) + ((a) - 1)) & ~((a) - 1))

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
    size_t size;
    char* ptr;
} Arena;

#ifdef __cplusplus
extern "C" {
#endif // cplusplus


ARENADEF Arena init_arena(size_t size);
ARENADEF void* arena_alloc(Arena* arena, size_t alloc_size);
ARENADEF void reset_arena(Arena* arena);
ARENADEF void free_arena(Arena* arena);
ARENADEF int reset_region(const Arena* arena, void* region_start, size_t region_size);

#ifdef __cplusplus
}
#endif // cplusplus

#endif // ARENA_H
#ifdef ARENA_IMPLEMENTATION

/* ===========================================================
   ===============   WINDOWS IMPLEMENTATION   =================
   =========================================================== */
#if defined(_WIN32) || defined(_WIN64)

#include <windows.h>

ARENADEF Arena init_arena(size_t size) {
    Arena arena = {0};
    SYSTEM_INFO sys_info;
    GetSystemInfo(&sys_info);
    size_t page_size = (size_t)sys_info.dwPageSize;
    size = ALIGN_UP(size, page_size);

    arena.ptr = (char*)VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_READWRITE);
    if (!arena.ptr) {
        fprintf(stderr, "VirtualAlloc reserve failed: %lu\n", GetLastError());
        exit(EXIT_FAILURE);
    }

    arena.size = size;
    return arena;
}

ARENADEF void* arena_alloc(Arena* arena, size_t alloc_size) {
    uintptr_t base = (uintptr_t)(arena->ptr + arena->memory_used);
    uintptr_t aligned = ALIGN_UP(base, ARENA_ALIGNMENT);
    size_t padding = aligned - base;
    size_t total_size = padding + alloc_size;
    size_t required = arena->memory_used + total_size;

    if (required > arena->size) {
        fprintf(stderr, "ERROR: Allocation exceeds arena capacity!\n");
        return NULL;
    }

    SYSTEM_INFO sys_info;
    GetSystemInfo(&sys_info);
    size_t page_size = (size_t)sys_info.dwPageSize;

    if (required > arena->committed_size) {
        size_t new_commit_end = ALIGN_UP(required, page_size);
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

ARENADEF void reset_arena(Arena* arena) {
    if (arena->ptr) {
        VirtualAlloc(arena->ptr, arena->committed_size, MEM_RESET, PAGE_READWRITE);
        arena->memory_used = 0;
    }
}

ARENADEF void free_arena(Arena* arena) {
    if (arena->ptr) {
        VirtualFree(arena->ptr, 0, MEM_RELEASE);
        arena->ptr = NULL;
        arena->size = 0;
        arena->committed_size = 0;
        arena->memory_used = 0;
    }
}

ARENADEF int reset_region(const Arena* arena, void* region_start, size_t region_size) {
    uintptr_t arena_start = (uintptr_t)arena->ptr;
    uintptr_t arena_end = arena_start + arena->size;
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

ARENADEF Arena init_arena(size_t size) {
    Arena arena = {0};
    size_t page_size = (size_t)sysconf(_SC_PAGESIZE);
    size = ALIGN_UP(size, page_size);

    arena.ptr = (char*)mmap(NULL, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (arena.ptr == MAP_FAILED) {
        perror("mmap reserve failed");
        exit(EXIT_FAILURE);
    }

    arena.size = size;
    return arena;
}

ARENADEF void* arena_alloc(Arena* arena, size_t alloc_size) {
    uintptr_t base = (uintptr_t)(arena->ptr + arena->memory_used);
    uintptr_t aligned = ALIGN_UP(base, ARENA_ALIGNMENT);
    size_t padding = aligned - base;
    size_t total_size = padding + alloc_size;
    size_t required = arena->memory_used + total_size;

    if (required > arena->size) {
        fprintf(stderr, "ERROR: Allocation exceeds arena capacity!\n");
        return NULL;
    }

    size_t page_size = (size_t)sysconf(_SC_PAGESIZE);
    if (required > arena->committed_size) {
        size_t new_commit_end = ALIGN_UP(required, page_size);
        size_t commit_amount = new_commit_end - arena->committed_size;
        void* commit_ptr = arena->ptr + arena->committed_size;

        if (mprotect(commit_ptr, commit_amount, PROT_READ | PROT_WRITE) != 0) {
            perror("mprotect commit failed");
            exit(EXIT_FAILURE);
        }

        arena->committed_size = new_commit_end;
    }

    arena->memory_used += total_size;
    return (void*)aligned;
}

ARENADEF void reset_arena(Arena* arena) {
    if (arena->ptr) {
        madvise(arena->ptr, arena->committed_size, MADV_DONTNEED);
        arena->memory_used = 0;
    }
}

ARENADEF void free_arena(Arena* arena) {
    if (arena->ptr) {
        munmap(arena->ptr, arena->size);
        arena->ptr = NULL;
        arena->size = 0;
        arena->committed_size = 0;
        arena->memory_used = 0;
    }
}

ARENADEF int reset_region(const Arena* arena, void* region_start, size_t region_size) {
    uintptr_t arena_start = (uintptr_t)arena->ptr;
    uintptr_t arena_end = arena_start + arena->size;
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
