#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "command.h"

static int failures = 0;
#define ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "Assertion failed: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
        failures++; \
    } \
} while (0)

static void test_long_path(void)
{
    size_t len = 8000; /* deliberately longer than typical PATH_MAX */
    char *path = malloc(len + 1);
    if (!path) {
        perror("malloc");
        exit(1);
    }
    memset(path, 'a', len);
    path[len] = '\0';

    char *argv[] = {"cmd", path, NULL};
    char *s = command_to_string(argv);
    ASSERT(s != NULL);
    ASSERT(strstr(s, path) != NULL);
    free(s);
    free(path);
}

int main(void)
{
    test_long_path();
    if (failures == 0)
        printf("All command_long_path tests passed\n");
    else
        printf("%d command_long_path test(s) failed\n", failures);
    return failures ? 1 : 0;
}
