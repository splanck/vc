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
    const char *src = "#define X 1\n"
                     "#define Y 1\n"
                     "#if defined(X) && Y\n"
                     "int yes;\n"
                     "#endif\n"
                     "#undef X\n"
                     "#if defined(X) && Y\n"
                     "int no;\n"
                     "#endif\n";
    if (fd >= 0) {
        ASSERT(write(fd, src, strlen(src)) == (ssize_t)strlen(src));
        close(fd);
    }

    vector_t dirs; vector_init(&dirs, sizeof(char *));
    preproc_context_t ctx = {0};
    char *res = preproc_run(&ctx, tmpl, &dirs, NULL, NULL, NULL, NULL, false, false);
    ASSERT(res != NULL);
    if (res) {
        ASSERT(strstr(res, "int yes;") != NULL);
        ASSERT(strstr(res, "int no;") == NULL);
    }
    free(res);
    preproc_context_free(&ctx);
    vector_free(&dirs);
    unlink(tmpl);

    if (failures == 0)
        printf("All preproc_defined_macro tests passed\n");
    else
        printf("%d preproc_defined_macro test(s) failed\n", failures);
    return failures ? 1 : 0;
}
