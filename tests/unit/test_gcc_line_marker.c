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
    char tmpl[] = "/tmp/ppXXXXXX.c";
    int fd = mkstemp(tmpl);
    ASSERT(fd >= 0);
    const char *src = "# 42 \"foo.c\" 1\n"
                     "int a = __LINE__;\n"
                     "const char *f = __FILE__;\n"
                     "# 100 \"bar.c\" 2\n"
                     "int b = __LINE__;\n";
    if (fd >= 0) {
        ASSERT(write(fd, src, strlen(src)) == (ssize_t)strlen(src));
        close(fd);
    }

    vector_t dirs; vector_init(&dirs, sizeof(char *));
    preproc_context_t ctx;
    char *res = preproc_run(&ctx, tmpl, &dirs, NULL, NULL, NULL);
    ASSERT(res != NULL);
    if (res) {
        ASSERT(strstr(res, "int a = 42;") != NULL);
        ASSERT(strstr(res, "\"foo.c\"") != NULL);
        ASSERT(strstr(res, "int b = 100;") != NULL);
        ASSERT(strstr(res, "\"bar.c\"") != NULL);
    }
    free(res);
    preproc_context_free(&ctx);
    vector_free(&dirs);
    unlink(tmpl);

    if (failures == 0)
        printf("All gcc_line_marker tests passed\n");
    else
        printf("%d gcc_line_marker test(s) failed\n", failures);
    return failures ? 1 : 0;
}
