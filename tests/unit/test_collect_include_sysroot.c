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
    vector_t empty; vector_init(&empty, sizeof(char *));
    vector_t base; ASSERT(collect_include_dirs(&base, &empty, NULL, false));
    size_t count = base.count;
    char **paths = calloc(count, sizeof(char*));
    for (size_t i = 0; i < count; i++)
        paths[i] = strdup(((char **)base.data)[i]);
    free_string_vector(&base);

    const char *sysroot = "/tmp/sysroot";
    vector_t pref; ASSERT(collect_include_dirs(&pref, &empty, sysroot, false));
    ASSERT(pref.count == count);
    for (size_t i = 0; i < count; i++) {
        char expect[4096];
        snprintf(expect, sizeof(expect), "%s%s", sysroot, paths[i]);
        ASSERT(strcmp(((char **)pref.data)[i], expect) == 0);
    }
    free_string_vector(&pref);

    vector_t pref_slash; ASSERT(collect_include_dirs(&pref_slash, &empty, "/tmp/sysroot/", false));
    ASSERT(pref_slash.count == count);
    for (size_t i = 0; i < count; i++) {
        char expect[4096];
        snprintf(expect, sizeof(expect), "%s%s", sysroot, paths[i]);
        ASSERT(strcmp(((char **)pref_slash.data)[i], expect) == 0);
    }
    free_string_vector(&pref_slash);

    for (size_t i = 0; i < count; i++)
        free(paths[i]);
    free(paths);
    vector_free(&empty);

    if (failures == 0)
        printf("All collect_include_sysroot tests passed\n");
    else
        printf("%d collect_include_sysroot test(s) failed\n", failures);
    return failures ? 1 : 0;
}
