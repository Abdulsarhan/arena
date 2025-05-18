#ifndef ARENA_H
#define ARENA_H

#include <stdio.h> // printf
#include <stdint.h> // uintptr_t
#include <string.h> // memset
#include <stdlib.h> // EXIT_FAILURE

#define ARENA_ALIGNMENT 16
#define ALIGN_UP(x, a) (((x) + ((a) - 1)) & ~((a) - 1))

#ifdef ARENA_STATIC
#define ARENADEF static
#else
#define ARENADEF extern
#endif

typedef struct {
    size_t memory_used; // total memory allocated with arena_alloc
    size_t arena_size; // total capacity of the arena.
    char* arena_ptr; // pointer to the next region of unused memory in the arena.
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

ARENADEF Arena init_arena(size_t size) {
    Arena arena = { 0 };
    arena.arena_size = size;
    arena.arena_ptr = (char*)malloc(size);
    if (!arena.arena_ptr) {
        fprintf(stderr, "ERROR: Failed to allocate memory for arena!\n");
        exit(EXIT_FAILURE);
    }
    return arena;
}

ARENADEF void* arena_alloc(Arena* arena, size_t alloc_size) {
    uintptr_t base = (uintptr_t)(arena->arena_ptr + arena->memory_used);
    uintptr_t aligned = ALIGN_UP(base, ARENA_ALIGNMENT);
    size_t padding = aligned - base;
    size_t total_size = padding + alloc_size;

    if (arena->memory_used + total_size > arena->arena_size) {
        fprintf(stderr, "ERROR: Allocation exceeds the size of the arena!\n");
        exit(EXIT_FAILURE);
    }

    arena->memory_used += total_size;
    return (void*)aligned;
}

ARENADEF void reset_arena(Arena* arena) {
    if (arena->arena_ptr) {
        memset(arena->arena_ptr, 0, arena->arena_size);
        arena->memory_used = 0;
    }
}

ARENADEF void free_arena(Arena* arena) {
    if (arena->arena_ptr) {
        free(arena->arena_ptr);
        arena->arena_ptr = NULL;
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
    }
    else {
        fprintf(stderr, "ERROR: Arena Region is out of bounds of the arena!\n");
        return -1;
    }
}

#endif // Implementation Macro

#endif
