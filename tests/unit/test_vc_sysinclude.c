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
    char dir_template[] = "/tmp/vc_sysXXXXXX";
    char *d = mkdtemp(dir_template);
    ASSERT(d != NULL);
    char path[4096];
    snprintf(path, sizeof(path), "%s/foo.h", d);
    FILE *fp = fopen(path, "w");
    ASSERT(fp != NULL);
    if (fp) {
        fputs("/* test */\n", fp);
        fclose(fp);
    }
    char envbuf[8192];
#ifdef _WIN32
    snprintf(envbuf, sizeof(envbuf), "%s;%s", d, d);
#else
    snprintf(envbuf, sizeof(envbuf), "%s:%s", d, d);
#endif
    setenv("VC_SYSINCLUDE", envbuf, 1);
    vector_t empty; vector_init(&empty, sizeof(char *));
    vector_t dirs; ASSERT(collect_include_dirs(&dirs, &empty, NULL, NULL, false));
    size_t idx = SIZE_MAX;
    char *res = find_include_path("foo.h", '<', NULL, &dirs, 0, &idx);
    ASSERT(res != NULL);
    if (res) {
        ASSERT(strcmp(res, path) == 0);
        free(res);
    }
    free_string_vector(&dirs);
    vector_free(&empty);
    unsetenv("VC_SYSINCLUDE");
    unlink(path);
    rmdir(d);
    preproc_path_cleanup();

    if (failures == 0)
        printf("All vc_sysinclude tests passed\n");
    else
        printf("%d vc_sysinclude test(s) failed\n", failures);
    return failures ? 1 : 0;
}
