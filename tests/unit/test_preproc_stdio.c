#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "preproc_file.h"

/* Stub packing helpers used by the preprocessor */
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
    const char *line = "#include <stdio.h>\n";
    if (fd >= 0) {
        ASSERT(write(fd, line, strlen(line)) == (ssize_t)strlen(line));
        close(fd);
    }

    vector_t dirs; vector_init(&dirs, sizeof(char *));
    preproc_context_t ctx;
    char *res = preproc_run(&ctx, tmpl, &dirs, NULL, NULL);
    ASSERT(res != NULL);
    free(res);
    preproc_context_free(&ctx);
    vector_free(&dirs);
    unlink(tmpl);

    if (failures == 0)
        printf("All preproc_stdio tests passed\n");
    else
        printf("%d preproc_stdio test(s) failed\n", failures);
    return failures ? 1 : 0;
}
