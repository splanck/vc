#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
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
    unsetenv("VCPATH");
    unsetenv("VCINC");
    unsetenv("CPATH");
    unsetenv("C_INCLUDE_PATH");

    vector_t inc; vector_init(&inc, sizeof(char *));
    char *p = strdup("/uinc");
    vector_push(&inc, &p);

    vector_t sys; vector_init(&sys, sizeof(char *));
    char *s = strdup("/sysinc");
    vector_push(&sys, &s);

    vector_t out; ASSERT(collect_include_dirs(&out, &inc, &sys, NULL));
    ASSERT(out.count >= 2);
    if (out.count >= 2) {
        ASSERT(strcmp(((char **)out.data)[0], "/uinc") == 0);
        ASSERT(strcmp(((char **)out.data)[1], "/sysinc") == 0);
    }
    free_string_vector(&out);
    free(p); vector_free(&inc);
    free(s); vector_free(&sys);
    if (failures == 0)
        printf("All collect_include_isystem tests passed\n");
    else
        printf("%d collect_include_isystem test(s) failed\n", failures);
    return failures ? 1 : 0;
}
