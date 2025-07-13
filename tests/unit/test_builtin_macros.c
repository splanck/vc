#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "preproc_file.h"

size_t semantic_pack_alignment = 0;
void semantic_set_pack(size_t align) { (void)align; }

static int failures = 0;
#define ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "Assertion failed: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
        failures++; \
    } \
} while (0)

int main(void)
{
    char hdrtmpl[] = "/tmp/hdrXXXXXX.h";
    int fd = mkstemp(hdrtmpl);
    ASSERT(fd >= 0);
    const char *hdrsrc = "int lvl = __INCLUDE_LEVEL__;\n"
                         "const char *b = __BASE_FILE__;\n"
                         "int cnt1 = __COUNTER__;\n";
    if (fd >= 0) {
        ASSERT(write(fd, hdrsrc, strlen(hdrsrc)) == (ssize_t)strlen(hdrsrc));
        close(fd);
    }

    char maintmpl[] = "/tmp/mainXXXXXX.c";
    fd = mkstemp(maintmpl);
    ASSERT(fd >= 0);
    char buf[512];
    snprintf(buf, sizeof(buf),
             "int cnt0 = __COUNTER__;\n#include \"%s\"\nint cnt2 = __COUNTER__;\nint lvl0 = __INCLUDE_LEVEL__;\nconst char *b0 = __BASE_FILE__;\n",
             hdrtmpl);
    if (fd >= 0) {
        ASSERT(write(fd, buf, strlen(buf)) == (ssize_t)strlen(buf));
        close(fd);
    }

    vector_t dirs; vector_init(&dirs, sizeof(char *));
    preproc_context_t ctx = {0};
    char *res = preproc_run(&ctx, maintmpl, &dirs, NULL, NULL);
    ASSERT(res != NULL);
    if (res) {
        char exp0[64]; snprintf(exp0, sizeof(exp0), "int cnt0 = 0;");
        char exp1[64]; snprintf(exp1, sizeof(exp1), "int cnt1 = 1;");
        char exp2[64]; snprintf(exp2, sizeof(exp2), "int cnt2 = 2;");
        char lvl_inc[] = "int lvl = 1;";
        char lvl_main[] = "int lvl0 = 0;";
        ASSERT(strstr(res, exp0) != NULL);
        ASSERT(strstr(res, exp1) != NULL);
        ASSERT(strstr(res, exp2) != NULL);
        ASSERT(strstr(res, lvl_inc) != NULL);
        ASSERT(strstr(res, lvl_main) != NULL);
        char base_q[512]; snprintf(base_q, sizeof(base_q), "\"%s\"", maintmpl);
        ASSERT(strstr(res, base_q) != NULL);
        ASSERT(strstr(res, hdrtmpl) == NULL);
    }
    free(res);
    preproc_context_free(&ctx);
    vector_free(&dirs);
    unlink(hdrtmpl);
    unlink(maintmpl);

    if (failures == 0)
        printf("All builtin macro tests passed\n");
    else
        printf("%d builtin macro test(s) failed\n", failures);
    return failures ? 1 : 0;
}
