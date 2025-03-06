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

Arena InitArena(size_t size) {
    Arena arena;
    arena.arenaSize = size;
    arena.arenaPtr = malloc(size);
    if (arena.arenaPtr == NULL) {
        fprintf(stderr, "ERROR: Failed to allocate memory for arena!\n");
        exit(1);
    }
    arena.previousAllocs = 0;
    return arena;
}

void* ArenaAlloc(Arena* arena, size_t allocSize) {
    if (arena->previousAllocs + allocSize > arena->arenaSize) {
        fprintf(stderr, "ERROR: Allocation exceeds the size of the arena!");
        exit(1);
    }
    void* alloc = (char*)arena->arenaPtr + arena->previousAllocs;
    arena->previousAllocs += allocSize;
    return alloc;
}

void ResetArena(Arena* arena) {
    memset(arena->arenaPtr, 0, arena->arenaSize);
    arena->previousAllocs = 0;
}

void FreeArena(Arena* arena) {
    free(arena->arenaPtr);
    arena->arenaPtr = NULL;
}