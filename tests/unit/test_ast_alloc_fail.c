#include <stdio.h>
#include <stdlib.h>
#include "ast_expr.h"
#include "ast_stmt.h"

static int failures = 0;
#define ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "Assertion failed: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
        failures++; \
    } \
} while (0)

extern void *malloc(size_t size); /* real malloc */
static int fail_malloc = 0;
static int fail_after = 0;
void *test_malloc(size_t size)
{
    if (fail_malloc) {
        if (fail_after-- <= 0)
            return NULL;
    }
    return malloc(size);
}

static void test_number_expr_alloc(void)
{
    fail_malloc = 1; fail_after = 0; /* fail in vc_strndup */
    expr_t *e = ast_make_number("123", 1, 1);
    ASSERT(e == NULL);
    fail_malloc = 0;
}

static void test_new_expr_alloc(void)
{
    fail_malloc = 1; fail_after = 1; /* fail in new_expr after strip_suffix */
    expr_t *e = ast_make_number("123", 1, 1);
    ASSERT(e == NULL);
    fail_malloc = 0;
}

int main(void)
{
    test_number_expr_alloc();
    test_new_expr_alloc();
    if (failures == 0)
        printf("All ast_alloc_fail tests passed\n");
    else
        printf("%d ast_alloc_fail test(s) failed\n", failures);
    return failures ? 1 : 0;
}
