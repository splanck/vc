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
    const char *src = "#if 'A' == 65\n"
                     "int ok;\n"
                     "#else\n"
                     "int bad;\n"
                     "#endif\n";
    if (fd >= 0) {
        ASSERT(write(fd, src, strlen(src)) == (ssize_t)strlen(src));
        close(fd);
    }

    vector_t dirs; vector_init(&dirs, sizeof(char *));
    preproc_context_t ctx;
    char *res = preproc_run(&ctx, tmpl, &dirs, NULL, NULL);
    ASSERT(res != NULL);
    if (res) {
        ASSERT(strstr(res, "int ok;") != NULL);
        ASSERT(strstr(res, "int bad;") == NULL);
    }
    free(res);
    preproc_context_free(&ctx);
    vector_free(&dirs);
    unlink(tmpl);

    if (failures == 0)
        printf("All preproc_charlit tests passed\n");
    else
        printf("%d preproc_charlit test(s) failed\n", failures);
    return failures ? 1 : 0;
}
