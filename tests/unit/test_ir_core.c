#include <stdio.h>
#include <stdlib.h>
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

int main(void)
{
    test_wstring_alloc_fail();
    if (failures == 0)
        printf("All ir_core tests passed\n");
    else
        printf("%d ir_core test(s) failed\n", failures);
    return failures ? 1 : 0;
}
