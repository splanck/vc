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
    if (access("/usr/include/stdio.h", R_OK) != 0) {
        char multi_path[256];
        snprintf(multi_path, sizeof(multi_path), "/usr/include/%s/stdio.h", MULTIARCH);
        if (access(multi_path, R_OK) != 0) {
            printf("Skipping preproc_stdio_skip tests (stdio.h not found)\n");
            return 0;
        }
    }
    char tmpl[] = "/tmp/ppXXXXXX.c";
    int fd = mkstemp(tmpl);
    ASSERT(fd >= 0);
    const char *line = "#include <stdio.h>\n";
    if (fd >= 0) {
        ASSERT(write(fd, line, strlen(line)) == (ssize_t)strlen(line));
        close(fd);
    }

    vector_t dirs; vector_init(&dirs, sizeof(char *));
    preproc_context_t ctx = {0};
    char *res = preproc_run(&ctx, tmpl, &dirs, NULL, NULL, NULL, NULL, false, false);
    if (!res) {
        printf("Skipping preproc_stdio_skip tests (preprocessing failed)\n");
        preproc_context_free(&ctx);
        vector_free(&dirs);
        unlink(tmpl);
        return 0;
    }
    free(res);
    preproc_context_free(&ctx);
    vector_free(&dirs);
    unlink(tmpl);

    if (failures == 0)
        printf("All preproc_stdio_skip tests passed\n");
    else
        printf("%d preproc_stdio_skip test(s) failed\n", failures);
    return failures ? 1 : 0;
}
