#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "preproc_file_io.h"

static int failures = 0;
#define ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "Assertion failed: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
        failures++; \
    } \
} while (0)

int main(void)
{
    char tmpl[] = "/tmp/rflXXXXXX";
    int fd = mkstemp(tmpl);
    ASSERT(fd >= 0);
    FILE *f = fd >= 0 ? fdopen(fd, "w") : NULL;
    ASSERT(f != NULL);
    if (f) {
        for (int i = 0; i < 10000; i++) {
            fprintf(f, "LINE%d \\\n", i);
            fprintf(f, "CONT%d\n", i);
        }
        fclose(f);
    }

    char **lines = NULL;
    char *text = read_file_lines(tmpl, &lines);
    ASSERT(text != NULL);
    if (text) {
        for (int i = 0; i < 10000; i++) {
            char expect[32];
            snprintf(expect, sizeof(expect), "LINE%d CONT%d", i, i);
            ASSERT(strcmp(lines[i], expect) == 0);
        }
        ASSERT(lines[10000] == NULL);
    }
    free(lines);
    free(text);
    unlink(tmpl);

    if (failures == 0)
        printf("All read_file_lines_large tests passed\n");
    else
        printf("%d read_file_lines_large test(s) failed\n", failures);
    return failures ? 1 : 0;
}

