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
    char tmpl[] = "/tmp/tpasteXXXXXX";
    int fd = mkstemp(tmpl);
    ASSERT(fd >= 0);
    const char *src =
        "#define PREFIX(name) prefix##name\n"
        "#define SUFFIX(name) name##suffix\n"
        "#define BEGIN(name) ##name\n"
        "#define END(name) name##\n"
        "int PREFIX(foo) = 1;\n"
        "int SUFFIX(bar) = 2;\n"
        "int BEGIN(baz) = 3;\n"
        "int END(qux) = 4;\n";
    if (fd >= 0) {
        ASSERT(write(fd, src, strlen(src)) == (ssize_t)strlen(src));
        close(fd);
    }

    vector_t dirs; vector_init(&dirs, sizeof(char *));
    preproc_context_t ctx = {0};
    char *res = preproc_run(&ctx, tmpl, &dirs, NULL, NULL, NULL, NULL, NULL, false);
    ASSERT(res != NULL);
    if (res) {
        ASSERT(strstr(res, "int prefixfoo = 1;") != NULL);
        ASSERT(strstr(res, "int barsuffix = 2;") != NULL);
        ASSERT(strstr(res, "int baz = 3;") != NULL);
        ASSERT(strstr(res, "int qux = 4;") != NULL);
    }
    free(res);
    preproc_context_free(&ctx);
    vector_free(&dirs);
    unlink(tmpl);

    if (failures == 0)
        printf("All preproc_token_paste tests passed\n");
    else
        printf("%d preproc_token_paste test(s) failed\n", failures);
    return failures ? 1 : 0;
}
