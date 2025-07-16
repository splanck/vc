#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include "include_path_cache.h"

static int failures = 0;
#define ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "Assertion failed: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
        failures++; \
    } \
} while (0)

static int fail_alloc = 0;
void *test_alloc_or_exit(size_t size)
{
    if (fail_alloc)
        return NULL;
    void *p = malloc(size);
    if (!p) {
        perror("malloc");
        exit(1);
    }
    return p;
}

int main(void)
{
    fail_alloc = 1;
    include_path_cache_init();
    include_path_cache_cleanup();

    fail_alloc = 0;
    include_path_cache_init();
    const char * const *dirs = include_path_cache_std_dirs();
    ASSERT(dirs[0] != NULL);
    include_path_cache_cleanup();

    if (failures == 0)
        printf("All std_dirs_alloc_fail tests passed\n");
    else
        printf("%d std_dirs_alloc_fail test(s) failed\n", failures);
    return failures ? 1 : 0;
}
