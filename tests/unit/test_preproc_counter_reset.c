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
    const char *src = "int v = __COUNTER__;\n";
    char tmpl[] = "/tmp/cntXXXXXX.c";
    int fd = mkstemp(tmpl); ASSERT(fd >= 0);
    if (fd >= 0) { write(fd, src, strlen(src)); close(fd); }

    vector_t dirs; vector_init(&dirs, sizeof(char *));
    preproc_context_t ctx = {0};

    char *r1 = preproc_run(&ctx, tmpl, &dirs, NULL, NULL); ASSERT(r1);
    if (r1) { ASSERT(strstr(r1, "int v = 0;") != NULL); free(r1); }
    preproc_context_free(&ctx);

    char *r2 = preproc_run(&ctx, tmpl, &dirs, NULL, NULL); ASSERT(r2);
    if (r2) { ASSERT(strstr(r2, "int v = 0;") != NULL); free(r2); }
    preproc_context_free(&ctx);

    vector_free(&dirs);
    unlink(tmpl);

    if (failures == 0)
        printf("All preproc counter reset tests passed\n");
    else
        printf("%d preproc counter reset test(s) failed\n", failures);
    return failures ? 1 : 0;
}
