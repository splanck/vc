#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "compile_helpers.h"

/* vc_dep_name is exposed when UNIT_TESTING is defined */
char *vc_dep_name(const char *target);

static int failures = 0;
#define ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "Assertion failed: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
        failures++; \
    } \
} while (0)

static void test_long_names(void)
{
    const char *prefix = "dir/subdir/";
    size_t base_len = 5000;
    size_t prefix_len = strlen(prefix);
    char *src = malloc(prefix_len + base_len + 3);
    memcpy(src, prefix, prefix_len);
    memset(src + prefix_len, 'x', base_len);
    memcpy(src + prefix_len + base_len, ".c", 3);

    char *obj = vc_obj_name(src);
    ASSERT(obj != NULL);
    if (obj) {
        ASSERT(strlen(obj) == base_len + 2);
        ASSERT(strncmp(obj, src + prefix_len, base_len) == 0);
        ASSERT(strcmp(obj + base_len, ".o") == 0);
        free(obj);
    }

    char *dep = vc_dep_name(src);
    ASSERT(dep != NULL);
    if (dep) {
        ASSERT(strlen(dep) == base_len + 2);
        ASSERT(strncmp(dep, src + prefix_len, base_len) == 0);
        ASSERT(strcmp(dep + base_len, ".d") == 0);
        free(dep);
    }

    free(src);
}

int main(void)
{
    test_long_names();
    if (failures == 0)
        printf("All vc_names tests passed\n");
    else
        printf("%d vc_names test(s) failed\n", failures);
    return failures ? 1 : 0;
}
