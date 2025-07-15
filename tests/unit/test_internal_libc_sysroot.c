#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
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
    char sysroot[] = "/tmp/vc_sysrootXXXXXX";
    char *root = mkdtemp(sysroot);
    ASSERT(root != NULL);
    char path[4096];
    snprintf(path, sizeof(path), "%s/usr/include", sysroot);
    ASSERT(mkdir(sysroot, 0700) == 0 || errno == EEXIST);
    ASSERT(mkdir(path, 0700) == 0);
    snprintf(path, sizeof(path), "%s/usr/include/stdio.h", sysroot);
    FILE *fp = fopen(path, "w");
    ASSERT(fp != NULL);
    if (fp) {
        fputs("/* sysroot stdio */\n", fp);
        fclose(fp);
    }

    vector_t empty; vector_init(&empty, sizeof(char *));
    vector_t dirs; ASSERT(collect_include_dirs(&dirs, &empty, sysroot, NULL, true));
    size_t idx = SIZE_MAX;
    char *res = find_include_path("stdio.h", '<', NULL, &dirs, 0, &idx);
    ASSERT(res != NULL);
    if (res) {
        ASSERT(strstr(res, "libc/include/stdio.h") != NULL);
        free(res);
    }
    free_string_vector(&dirs);
    vector_free(&empty);
    unlink(path);
    snprintf(path, sizeof(path), "%s/usr/include", sysroot);
    rmdir(path);
    snprintf(path, sizeof(path), "%s/usr", sysroot);
    rmdir(path);
    rmdir(sysroot);
    preproc_path_cleanup();

    if (failures == 0)
        printf("All internal_libc_sysroot tests passed\n");
    else
        printf("%d internal_libc_sysroot test(s) failed\n", failures);
    return failures ? 1 : 0;
}
