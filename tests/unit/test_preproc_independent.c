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
    char tmpl1[] = "/tmp/ctx1XXXXXX.c";
    int fd1 = mkstemp(tmpl1); ASSERT(fd1 >= 0);
    if (fd1 >= 0) { write(fd1, src, strlen(src)); close(fd1); }

    char tmpl2[] = "/tmp/ctx2XXXXXX.c";
    int fd2 = mkstemp(tmpl2); ASSERT(fd2 >= 0);
    if (fd2 >= 0) { write(fd2, src, strlen(src)); close(fd2); }

    vector_t dirs; vector_init(&dirs, sizeof(char *));
    preproc_context_t c1, c2;
    char *r1 = preproc_run(&c1, tmpl1, &dirs, NULL, NULL, NULL, NULL, false); ASSERT(r1);
    char *r2 = preproc_run(&c2, tmpl2, &dirs, NULL, NULL, NULL, NULL, false); ASSERT(r2);
    if (r1) { ASSERT(strstr(r1, "int v = 0;") != NULL); }
    if (r2) { ASSERT(strstr(r2, "int v = 0;") != NULL); }
    free(r1); preproc_context_free(&c1); unlink(tmpl1);
    free(r2); preproc_context_free(&c2); unlink(tmpl2);
    vector_free(&dirs);

    if (failures == 0)
        printf("All preproc_independent tests passed\n");
    else
        printf("%d preproc_independent test(s) failed\n", failures);
    return failures ? 1 : 0;
}
