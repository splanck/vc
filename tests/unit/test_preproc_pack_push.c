#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "preproc_file.h"

size_t semantic_pack_alignment = 0;
static size_t pack_history[4];
static size_t pack_count = 0;
void semantic_set_pack(size_t align) { pack_history[pack_count++] = align; semantic_pack_alignment = align; }

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
    const char *src = "#pragma pack(push, 2)\n"
                     "#pragma pack(push)\n"
                     "#pragma pack(pop)\n"
                     "#pragma pack(pop)\n";
    if (fd >= 0) {
        ASSERT(write(fd, src, strlen(src)) == (ssize_t)strlen(src));
        close(fd);
    }

    vector_t dirs; vector_init(&dirs, sizeof(char *));
    preproc_context_t ctx;
    char *res = preproc_run(&ctx, tmpl, &dirs, NULL, NULL, NULL);
    ASSERT(res != NULL);
    free(res);
    preproc_context_free(&ctx);
    vector_free(&dirs);
    unlink(tmpl);

    ASSERT(pack_count >= 3);
    ASSERT(pack_history[0] == 2); /* push with value */
    ASSERT(pack_history[1] == 2); /* pop after push() should keep 2 */
    ASSERT(pack_history[2] == 0); /* final pop restores 0 */

    if (failures == 0)
        printf("All preproc_pack_push tests passed\n");
    else
        printf("%d preproc_pack_push test(s) failed\n", failures);
    return failures ? 1 : 0;
}
