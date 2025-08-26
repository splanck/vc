#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "preproc_path.h"
#include "util.h"

static int failures = 0;
#define ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "Assertion failed: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
        failures++; \
    } \
} while (0)

int main(void)
{
    vector_t dirs; vector_init(&dirs, sizeof(char *));
    ASSERT(append_env_paths("foo:bar;baz", &dirs));
    ASSERT(dirs.count == 3);
    if (dirs.count == 3) {
        ASSERT(strcmp(((char **)dirs.data)[0], "foo") == 0);
        ASSERT(strcmp(((char **)dirs.data)[1], "bar") == 0);
        ASSERT(strcmp(((char **)dirs.data)[2], "baz") == 0);
    }
    free_string_vector(&dirs);
    if (failures == 0)
        printf("All append_env_paths_both tests passed\n");
    else
        printf("%d append_env_paths_both test(s) failed\n", failures);
    return failures ? 1 : 0;
}
