#include <stdio.h>
#include <string.h> // memset
#include <stdint.h>
#include <stdlib.h>

#define ARENA_ALIGNMENT 16
#define ALIGN_UP(x, a) (((x) + ((a) - 1)) & ~((a) - 1))

#if defined(_WIN32) || defined(_WIN64)
    #define ARENA_WINDOWS
    #include <windows.h>
#else
    #define ARENA_POSIX
    #include <sys/mman.h>
    #include <unistd.h>
    #include <errno.h>
#endif

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
#endif

typedef struct {
    size_t memory_used;
    size_t committed_size;
    size_t arena_size;
    char* arena_ptr;
} Arena;

#ifdef __cplusplus
extern "C" {
#endif

ARENADEF Arena init_arena(size_t size);
ARENADEF void* arena_alloc(Arena* arena, size_t alloc_size);
ARENADEF void reset_arena(Arena* arena);
ARENADEF void free_arena(Arena* arena);
ARENADEF int reset_region(const Arena* arena, void* region_start, size_t region_size);

#ifdef __cplusplus
}
#endif

#ifdef ARENA_IMPLEMENTATION

// Forward declarations
static Arena arena_init_platform(size_t size);
static void arena_reset_platform(Arena* arena);
static void arena_free_platform(Arena* arena);
static size_t arena_get_page_size();

ARENADEF Arena init_arena(size_t size) {
    return arena_init_platform(size);
}

ARENADEF void* arena_alloc(Arena* arena, size_t alloc_size) {
    uintptr_t base = (uintptr_t)(arena->arena_ptr + arena->memory_used);
    uintptr_t aligned = ALIGN_UP(base, ARENA_ALIGNMENT);
    size_t padding = aligned - base;
    size_t total_size = padding + alloc_size;
    size_t required = arena->memory_used + total_size;

    if (required > arena->arena_size) {
        fprintf(stderr, "ERROR: Allocation exceeds arena capacity!\n");
        exit(EXIT_FAILURE);
    }

    if (required > arena->committed_size) {
        size_t page_size = arena_get_page_size();
        size_t new_commit_end = ALIGN_UP(required, page_size);
        size_t commit_amount = new_commit_end - arena->committed_size;

#ifdef ARENA_WINDOWS
        void* result = VirtualAlloc(arena->arena_ptr + arena->committed_size,
                                    commit_amount, MEM_COMMIT, PAGE_READWRITE);
        if (!result) {
            fprintf(stderr, "VirtualAlloc commit failed: %lu\n", GetLastError());
            exit(EXIT_FAILURE);
        }
#else // POSIX
        if (mprotect(arena->arena_ptr + arena->committed_size,
                     commit_amount, PROT_READ | PROT_WRITE) != 0) {
            perror("mprotect");
            exit(EXIT_FAILURE);
        }
#endif
        arena->committed_size = new_commit_end;
    }

    arena->memory_used += total_size;
    return (void*)aligned;
}

ARENADEF void reset_arena(Arena* arena) {
    if (arena->arena_ptr) {
        arena_reset_platform(arena);
        arena->memory_used = 0;
    }
}

ARENADEF void free_arena(Arena* arena) {
    if (arena->arena_ptr) {
        arena_free_platform(arena);
        arena->arena_ptr = NULL;
        arena->arena_size = 0;
        arena->committed_size = 0;
        arena->memory_used = 0;
    }
}

ARENADEF int reset_region(const Arena* arena, void* region_start, size_t region_size) {
    uintptr_t arena_start = (uintptr_t)arena->arena_ptr;
    uintptr_t arena_end = arena_start + arena->arena_size;
    uintptr_t region_addr = (uintptr_t)region_start;

    if (region_addr >= arena_start && region_addr + region_size <= arena_end) {
        memset(region_start, 0, region_size);
        fprintf(stdout, "INFO: Arena Region reset successfully!\n");
        return 0;
    } else {
        fprintf(stderr, "ERROR: Arena Region is out of bounds of the arena!\n");
        return -1;
    }
}

// ------------------ PLATFORM-SPECIFIC ------------------

static size_t arena_get_page_size() {
#ifdef ARENA_WINDOWS
    SYSTEM_INFO sys_info;
    GetSystemInfo(&sys_info);
    return (size_t)sys_info.dwPageSize;
#else
    return (size_t)sysconf(_SC_PAGESIZE);
#endif
}

#ifdef ARENA_WINDOWS

static Arena arena_init_platform(size_t size) {
    Arena arena = {0};
    size_t page_size = arena_get_page_size();
    size = ALIGN_UP(size, page_size);

    arena.arena_ptr = (char*)VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_READWRITE);
    if (!arena.arena_ptr) {
        fprintf(stderr, "VirtualAlloc reserve failed with error: %lu\n", GetLastError());
        exit(EXIT_FAILURE);
    }

    arena.arena_size = size;
    arena.committed_size = 0;
    return arena;
}

static void arena_reset_platform(Arena* arena) {
    // Just zero committed memory
    memset(arena->arena_ptr, 0, arena->committed_size);
}

static void arena_free_platform(Arena* arena) {
    VirtualFree(arena->arena_ptr, 0, MEM_RELEASE);
}

#else // POSIX

static Arena arena_init_platform(size_t size) {
    Arena arena = {0};
    size_t page_size = arena_get_page_size();
    size = ALIGN_UP(size, page_size);

    arena.arena_ptr = (char*)mmap(NULL, size, PROT_NONE,
                                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (arena.arena_ptr == MAP_FAILED) {
        perror("mmap reserve failed");
        exit(EXIT_FAILURE);
    }

    arena.arena_size = size;
    arena.committed_size = 0;
    return arena;
}

static void arena_reset_platform(Arena* arena) {
    madvise(arena->arena_ptr, arena->committed_size, MADV_DONTNEED);
}

static void arena_free_platform(Arena* arena) {
    munmap(arena->arena_ptr, arena->arena_size);
}

#endif // Platform

#endif // ARENA_IMPLEMENTATION
