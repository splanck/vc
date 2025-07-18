#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "preproc_path.h"
#include "util.h"

/* stub popen that always fails */
FILE *test_popen(const char *cmd, const char *mode)
{
    (void)cmd; (void)mode;
    errno = ENOSYS;
    return NULL;
}

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

    vector_t empty; vector_init(&empty, sizeof(char *));
    FILE *tmp = tmpfile();
    if (!tmp) {
        perror("tmpfile");
        exit(1);
    }
    int saved = dup(fileno(stderr));
    dup2(fileno(tmp), fileno(stderr));

    vector_t dirs;
    ASSERT(collect_include_dirs(&dirs, &empty, "/tmp/sysroot", false));

    fflush(stderr);
    fseek(tmp, 0, SEEK_SET);
    char buf[256];
    size_t n = fread(buf, 1, sizeof(buf) - 1, tmp);
    buf[n] = '\0';

    dup2(saved, fileno(stderr));
    close(saved);
    fclose(tmp);

#if defined(__linux__)
#ifndef MULTIARCH_FALLBACK
#define MULTIARCH_FALLBACK "x86_64-linux-gnu"
#endif
#ifdef MULTIARCH
#define MARCH MULTIARCH
#else
#define MARCH MULTIARCH_FALLBACK
#endif
    char expect[256];
    snprintf(expect, sizeof(expect), "/tmp/sysroot/usr/lib/gcc/%s/include", MARCH);
    int found = 0;
    for (size_t i = 0; i < dirs.count; i++) {
        if (strcmp(((char **)dirs.data)[i], expect) == 0) {
            found = 1;
            break;
        }
    }
    ASSERT(found);
#else
    const char *expect = "/tmp/sysroot/usr/lib/gcc/include";
    int found = 0;
    for (size_t i = 0; i < dirs.count; i++) {
        if (strcmp(((char **)dirs.data)[i], expect) == 0) {
            found = 1;
            break;
        }
    }
    ASSERT(found);
#endif

    free_string_vector(&dirs);
    vector_free(&empty);
    preproc_path_cleanup();

    ASSERT(strstr(buf, "popen") != NULL);

    if (failures == 0)
        printf("All preproc_popen_fail tests passed\n");
    else
        printf("%d preproc_popen_fail test(s) failed\n", failures);
    return failures ? 1 : 0;
}
