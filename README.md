# Arena

Basic arena arena allocator implementation.

## Basic Usage
```C
#define MEGABYTES(x) ((x) * 1024 * 1024)

#include <stdio.h>

#define ARENA_IMPLEMENTATION
#include "arena.h"

int main() {

Arena arena = init_arena(MEGABYTES(1)); // you can pass in whatever size you want, in bytes.

char* hello_world = arena_alloc(&arena, sizeof(char) * 13);

hello_world = "Hello World!"

printf(hello_world);

free_arena(&arena);

return 0;

}
```
