# Arena

Simple Arena Allocator Implementations.
The ``arena.h`` implementation works by reserving memory without commiting it.
And pages are commited on demand. This makes memory management a lot easier.
Since you don't have to predict how much memory your arena needs up front.
You can just allocate an absurdly large size that you know you will never reach.

while the ``arena_malloc.h`` implementation simply allocates memory the old fashioned way.
So, you should only allocate as much memory as your arena will ever use.

## Basic Usage
```C
#define GIGABYTES(x) ((x) * 1024 * 1024 * 1024)

#include <stdio.h>

#define ARENA_IMPLEMENTATION
#include "arena.h"

int main() {
    Arena arena = init_arena(GIGABYTES(1)); 

    char* hello_world = arena_alloc(&arena, sizeof(char) * 13);

    strcpy(hello_world, "Hello World!");

    printf("%s\n", hello_world);

    free_arena(&arena);

    return 0;

}
```
