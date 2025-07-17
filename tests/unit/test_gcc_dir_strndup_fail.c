#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include "include_path_cache.h"

static int failures = 0;
#define ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "Assertion failed: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
        failures++; \
    } \
} while (0)

static char *test_vc_strndup(const char *s, size_t n) { (void)s; (void)n; return NULL; }
#define vc_strndup test_vc_strndup
#include "../../src/include_path_cache.c"

int main(void)
{
    include_path_cache_init();
    ASSERT(gcc_query_failed);
    const char *dir = include_path_cache_gcc_dir();
    ASSERT(dir != NULL);
    include_path_cache_cleanup();
    ASSERT(gcc_query_failed == 0);

    if (failures == 0)
        printf("All gcc_dir_strndup_fail tests passed\n");
    else
        printf("%d gcc_dir_strndup_fail test(s) failed\n", failures);
    return failures ? 1 : 0;
}
