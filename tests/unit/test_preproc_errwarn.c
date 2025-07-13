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
    char tmpl[] = "/tmp/ppXXXXXX";
    int fd = mkstemp(tmpl);
    ASSERT(fd >= 0);
    const char *src = "#warning first\n"
                     "#error second\n";
    if (fd >= 0) {
        ASSERT(write(fd, src, strlen(src)) == (ssize_t)strlen(src));
        close(fd);
    }

    vector_t dirs; vector_init(&dirs, sizeof(char *));
    preproc_context_t ctx = {0};

    FILE *tmp = tmpfile();
    if (!tmp) {
        perror("tmpfile");
        exit(1);
    }
    int saved = dup(fileno(stderr));
    dup2(fileno(tmp), fileno(stderr));

    char *res = preproc_run(&ctx, tmpl, &dirs, NULL, NULL);

    fflush(stderr);
    fseek(tmp, 0, SEEK_SET);
    char buf[256];
    size_t n = fread(buf, 1, sizeof(buf) - 1, tmp);
    buf[n] = '\0';

    dup2(saved, fileno(stderr));
    close(saved);
    fclose(tmp);

    ASSERT(res == NULL);
    preproc_context_free(&ctx);
    vector_free(&dirs);
    unlink(tmpl);

    char exp1[256]; snprintf(exp1, sizeof(exp1), "%s:1: first", tmpl);
    char exp2[256]; snprintf(exp2, sizeof(exp2), "%s:2: second", tmpl);
    ASSERT(strstr(buf, exp1) != NULL);
    ASSERT(strstr(buf, exp2) != NULL);

    if (failures == 0)
        printf("All preproc_errwarn tests passed\n");
    else
        printf("%d preproc_errwarn test(s) failed\n", failures);
    return failures ? 1 : 0;
}
