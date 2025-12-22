#ifndef ARENA_H
#define ARENA_H

#include <stdio.h> // printf
#include <stdint.h> // uintptr_t
#include <string.h> // memset
#include <stdlib.h> // EXIT_FAILURE

#define ARENA_PUSH_STRUCT(arena, T) (T*)arena_push(arena, sizeof(T))
#define ARENA_PUSH_ARRAY(arena, T, n) (T*)arena_push(arena, sizeof(T) * n)

#ifdef ARENA_STATIC
#define ARENADEF static
#else
#define ARENADEF extern
#endif

typedef struct {
    size_t memory_used; // total memory allocated with arena_push
    size_t capacity;
    char* arena_ptr;
} arena;

#ifdef __cplusplus
extern "C" {
#endif

ARENADEF arena arena_init(size_t size);
ARENADEF void *arena_push(arena *arena, size_t alloc_size);
ARENADEF void arena_pop(arena *arena, size_t size);
ARENADEF void arena_pop_to(arena *arena, size_t pos);
ARENADEF void arena_clear(arena *arena);
ARENADEF void arena_destroy(arena *arena);
ARENADEF int arena_reset_region(const arena *arena, void* region_start, size_t region_size);

#ifdef __cplusplus
}
#endif

#ifdef ARENA_IMPLEMENTATION

#ifndef ARENA_ALIGNMENT
#define ARENA_ALIGNMENT 16
#endif

#define ALIGN_UP(x, a) (((x) + ((a) - 1)) & ~((a) - 1))

ARENADEF arena arena_init(size_t size) {
    arena arena = { 0 };
    arena.capacity = size;
    arena.arena_ptr = (char*)malloc(size);
    if (!arena.arena_ptr) {
        fprintf(stderr, "ERROR: Failed to allocate memory for arena!\n");
        exit(EXIT_FAILURE);
    }
    return arena;
}

ARENADEF void* arena_push(arena *arena, size_t alloc_size) {
    uintptr_t base = (uintptr_t)(arena->arena_ptr + arena->memory_used);
    uintptr_t aligned = ALIGN_UP(base, ARENA_ALIGNMENT);
    size_t padding = aligned - base;
    size_t total_size = padding + alloc_size;

    if (arena->memory_used + total_size > arena->capacity) {
        fprintf(stderr, "ERROR: Allocation exceeds the size of the arena!\n");
        exit(EXIT_FAILURE);
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
    if (arena->arena_ptr) {
        memset(arena->arena_ptr, 0, arena->capacity);
        arena->memory_used = 0;
    }
}

ARENADEF void arena_destroy(arena *arena) {
    if (arena->arena_ptr) {
        free(arena->arena_ptr);
        arena->arena_ptr = NULL;
    }
}

ARENADEF int arena_reset_region(const arena *arena, void *region_start, size_t region_size) {
    uintptr_t arena_start = (uintptr_t)arena->arena_ptr;
    uintptr_t arena_end = arena_start + arena->capacity;
    uintptr_t region_addr = (uintptr_t)region_start;

    if (region_addr >= arena_start && region_addr + region_size <= arena_end) {
        memset(region_start, 0, region_size);
        fprintf(stdout, "INFO: arena Region reset successfully!\n");
        return 0;
    }
    else {
        fprintf(stderr, "ERROR: arena Region is out of bounds of the arena!\n");
        return -1;
    }
}

#endif // Implementation Macro

#endif
