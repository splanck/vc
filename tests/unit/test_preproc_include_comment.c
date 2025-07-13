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
    char dir[] = "/tmp/incXXXXXX";
    ASSERT(mkdtemp(dir) != NULL);

    char hdr[256];
    snprintf(hdr, sizeof(hdr), "%s/foo.h", dir);
    FILE *f = fopen(hdr, "w");
    ASSERT(f != NULL);
    if (f) {
        fputs("int foo = 42;\n", f);
        fclose(f);
    }

    char src[] = "/tmp/srcXXXXXX.c";
    int fd = mkstemp(src);
    ASSERT(fd >= 0);
    if (fd >= 0) {
        dprintf(fd, "#include <foo.h> /*comment*/\n");
        close(fd);
    }

    vector_t dirs; vector_init(&dirs, sizeof(char *));
    char *d = strdup(dir);
    vector_push(&dirs, &d);

    preproc_context_t ctx;
    char *res = preproc_run(&ctx, src, &dirs, NULL, NULL, NULL);
    ASSERT(res != NULL);
    if (res) {
        ASSERT(strstr(res, "int foo = 42;") != NULL);
    }
    free(res);
    preproc_context_free(&ctx);
    free(d);
    vector_free(&dirs);
    unlink(src);
    unlink(hdr);
    rmdir(dir);

    if (failures == 0)
        printf("All preproc_include_comment tests passed\n");
    else
        printf("%d preproc_include_comment test(s) failed\n", failures);
    return failures ? 1 : 0;
}
