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
    char dir1[] = "/tmp/hi1XXXXXX";
    char dir2[] = "/tmp/hi2XXXXXX";
    ASSERT(mkdtemp(dir1) != NULL);
    ASSERT(mkdtemp(dir2) != NULL);

    char bar[256];
    snprintf(bar, sizeof(bar), "%s/bar.h", dir2);
    FILE *f = fopen(bar, "w");
    ASSERT(f != NULL);
    if (f) {
        fputs("/*bar*/\n", f);
        fclose(f);
    }

    char hdr[256];
    snprintf(hdr, sizeof(hdr), "%s/has.h", dir1);
    f = fopen(hdr, "w");
    ASSERT(f != NULL);
    if (f) {
        fputs("#if __has_include(\"stdio.h\")\nint ok1;\n#endif\n", f);
        fputs("#if __has_include(\"nosuch.h\")\nint fail1;\n#endif\n", f);
        fputs("#if __has_include_next(\"bar.h\")\nint ok2;\n#endif\n", f);
        fputs("#if __has_include_next(\"missing.h\")\nint fail2;\n#endif\n", f);
        fclose(f);
    }

    char src[] = "/tmp/hsXXXXXX";
    int fd = mkstemp(src);
    ASSERT(fd >= 0);
    if (fd >= 0) {
        dprintf(fd, "#include \"has.h\"\n");
        close(fd);
    }

    vector_t dirs; vector_init(&dirs, sizeof(char *));
    char *d1 = strdup(dir1);
    char *d2 = strdup(dir2);
    vector_push(&dirs, &d1);
    vector_push(&dirs, &d2);

    preproc_context_t ctx = {0};
    char *res = preproc_run(&ctx, src, &dirs, NULL, NULL, NULL, NULL, false);
    ASSERT(res != NULL);
    if (res) {
        ASSERT(strstr(res, "int ok1;") != NULL);
        ASSERT(strstr(res, "int ok2;") != NULL);
        ASSERT(strstr(res, "fail1") == NULL);
        ASSERT(strstr(res, "fail2") == NULL);
    }
    free(res);
    preproc_context_free(&ctx);
    free(d1);
    free(d2);
    vector_free(&dirs);
    unlink(src);
    unlink(hdr);
    unlink(bar);
    rmdir(dir1);
    rmdir(dir2);

    if (failures == 0)
        printf("All preproc_has_include tests passed\n");
    else
        printf("%d preproc_has_include test(s) failed\n", failures);
    return failures ? 1 : 0;
}
