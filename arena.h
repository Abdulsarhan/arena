#ifndef ARENA_H
#define ARENA_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

typedef struct {
    uintptr_t previousAllocs;
    uintptr_t arenaSize;
    void* arenaPtr;
}Arena;

static inline Arena InitArena(size_t size) {
    Arena arena;
    arena.arenaSize = size;
    arena.arenaPtr = malloc(size);
    if (arena.arenaPtr == NULL) {
        fprintf(stderr, "ERROR: Failed to allocate memory for arena!\n");
        exit(-1);
    }
    arena.previousAllocs = 0;
    return arena;
}

static inline void* ArenaAlloc(Arena& arena, size_t allocSize) {
    if (arena.previousAllocs + allocSize > arena.arenaSize) {
        fprintf(stderr, "ERROR: Allocation exceeds the size of the arena!");
        exit(-1);
    }
    void* alloc = (char*)arena.arenaPtr + arena.previousAllocs;
    arena.previousAllocs += allocSize;
    return alloc;
}

static inline void ResetArena(Arena* arena) {
    memset(arena->arenaPtr, 0, arena->arenaSize);
    arena->previousAllocs = 0;
}

static inline void FreeArena(Arena* arena) {
    if (arena->arenaPtr != NULL) {
        free(arena->arenaPtr);
        arena->arenaPtr = NULL;
    }
    else {
        fprintf(stderr, "ERROR: Arena is null, not going to free arena.");
    }
}
static inline int ResetRegion(Arena arena, void* region_start, size_t region_end) {
    if (arena.arenaPtr) {
        memset(region_start, 0, region_end);
        fprintf(stdout, "INFO: region reset successfully!");
        return 0;
    }
    else {
        fprintf(stderr, "ERROR: Arena is null, not going to reset region.");
        return -1;
    }
}

#endif // ARENA_H
