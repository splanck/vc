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
    const char *hdrsrc = "#define O once\n"
                         "#pragma O\n"
                         "int a;\n";
    if (fd >= 0) {
        ASSERT(write(fd, hdrsrc, strlen(hdrsrc)) == (ssize_t)strlen(hdrsrc));
        close(fd);
    }

    char maintmpl[] = "/tmp/mainXXXXXX.c";
    fd = mkstemp(maintmpl);
    ASSERT(fd >= 0);
    char buf[512];
    snprintf(buf, sizeof(buf), "#include \"%s\"\n#include \"%s\"\n", hdrtmpl, hdrtmpl);
    if (fd >= 0) {
        ASSERT(write(fd, buf, strlen(buf)) == (ssize_t)strlen(buf));
        close(fd);
    }

    vector_t dirs; vector_init(&dirs, sizeof(char *));
    preproc_context_t ctx = {0};
    char *res = preproc_run(&ctx, maintmpl, &dirs, NULL, NULL, NULL, NULL, false);
    ASSERT(res != NULL);
    if (res) {
        char *first = strstr(res, "int a;");
        ASSERT(first != NULL);
        if (first)
            ASSERT(strstr(first + 1, "int a;") == NULL);
    }
    free(res);
    preproc_context_free(&ctx);
    vector_free(&dirs);
    unlink(hdrtmpl);
    unlink(maintmpl);

    if (failures == 0)
        printf("All preproc_pragma_macro tests passed\n");
    else
        printf("%d preproc_pragma_macro test(s) failed\n", failures);
    return failures ? 1 : 0;
}
