#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <sys/wait.h>
#include "ir_core.h"

static int failures = 0;
#define ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "Assertion failed: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
        failures++; \
    } \
} while (0)

extern void *malloc(size_t size); /* real malloc */
static int fail_malloc = 0;
void *test_malloc(size_t size)
{
    if (fail_malloc)
        return NULL;
    return malloc(size);
}

static void test_wstring_alloc_fail(void)
{
    ir_builder_t b;
    ir_builder_init(&b);
    fail_malloc = 1;
    ir_value_t v = ir_build_wstring(&b, "abc");
    ASSERT(v.id == 0);
    ASSERT(b.head == NULL && b.tail == NULL);
    fail_malloc = 0;
    ir_builder_free(&b);
}

static void test_many_ids(void)
{
    ir_builder_t b;
    ir_builder_init(&b);
    const int count = 10000;
    for (int i = 0; i < count; i++) {
        ir_value_t v = ir_build_const(&b, i);
        ASSERT(v.id == i + 1);
    }
    ASSERT(b.next_value_id == (size_t)count + 1);
    ir_builder_free(&b);
}

static void test_id_overflow(void)
{
    ir_builder_t b;
    ir_builder_init(&b);
    b.next_value_id = (size_t)INT_MAX - 1;
    ir_value_t v = ir_build_const(&b, 0);
    ASSERT(v.id == INT_MAX - 1);
    ASSERT(b.next_value_id == (size_t)INT_MAX);

    pid_t pid = fork();
    ASSERT(pid >= 0);
    if (pid == 0) {
        b.next_value_id = (size_t)INT_MAX;
        ir_build_const(&b, 0);
        _exit(0);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    ASSERT(WIFEXITED(status) && WEXITSTATUS(status) != 0);
    ir_builder_free(&b);
}

int main(void)
{
    test_wstring_alloc_fail();
    test_many_ids();
    test_id_overflow();
    if (failures == 0)
        printf("All ir_core tests passed\n");
    else
        printf("%d ir_core test(s) failed\n", failures);
    return failures ? 1 : 0;
}
