#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "command.h"
#include "strbuf.h"

static int failures = 0;
#define ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "Assertion failed: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
        failures++; \
    } \
} while (0)

extern int strbuf_append(strbuf_t *sb, const char *text); /* real impl */
static int fail_at = 0;
static int call_count = 0;
int test_strbuf_append(strbuf_t *sb, const char *text)
{
    call_count++;
    if (fail_at && call_count == fail_at)
        return -1;
    return strbuf_append(sb, text);
}

static void test_alloc_failure(void)
{
    char *argv[] = {"cmd", "arg1", "arg2", NULL};
    call_count = 0;
    fail_at = 3; /* fail while processing second argument */
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
