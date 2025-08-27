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

static int fail_at = 0;
static int call_count = 0;
void *test_malloc(size_t size)
{
    call_count++;
    if (fail_at && call_count == fail_at)
        return NULL;
    return malloc(size);
}
void *test_realloc(void *ptr, size_t size)
{
    call_count++;
    if (fail_at && call_count == fail_at)
        return NULL;
    return realloc(ptr, size);
}

static void test_alloc_failure(void)
{
    char longarg[5000];
    memset(longarg, 'a', sizeof(longarg) - 1);
    longarg[sizeof(longarg) - 1] = '\0';
    char *argv[] = {"cmd", longarg, "arg2", NULL};
    call_count = 0;
    fail_at = 2; /* fail during first reallocation */
    char *s = command_to_string(argv);
    ASSERT(s == NULL);
}

int main(void)
{
    test_alloc_failure();
    if (failures == 0)
        printf("All command_alloc_fail tests passed\n");
    else
        printf("%d command_alloc_fail test(s) failed\n", failures);
    return failures ? 1 : 0;
}
