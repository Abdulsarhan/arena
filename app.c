#define ARENA_IMPLEMENTATION
#include "arena.h"
#include <stdio.h>

#define Kib(x)((x) * 1024)
#define MiB(x)((x) * 1024 * 1024)
#define GiB(x)((x) * 1024 * 1024 * 1024)

int main() {
    mem_arena *arena = arena_init(MiB(10));

    int *nums = arena_push(arena, 100 * sizeof(int), 1);

    for(int i = 0; i < 100; i++) {
        nums[i] = i;
        printf("%d\n", nums[i]);
    }
    return 0;
}
