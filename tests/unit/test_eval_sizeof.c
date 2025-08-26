#include <stdio.h>
#include "ast_expr.h"
#include "consteval.h"

static int failures = 0;
#define ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "Assertion failed: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
        failures++; \
    } \
} while (0)

static void test_sizeof_ptr_32(void)
{
    expr_t *e = ast_make_sizeof_type(TYPE_PTR, 0, 0, 1, 1);
    long long val = 0;
    ASSERT(eval_const_expr(e, NULL, 0, &val));
    ASSERT(val == 4);
    ast_free_expr(e);
}

static void test_sizeof_ptr_64(void)
{
    expr_t *e = ast_make_sizeof_type(TYPE_PTR, 0, 0, 1, 1);
    long long val = 0;
    ASSERT(eval_const_expr(e, NULL, 1, &val));
    ASSERT(val == 8);
    ast_free_expr(e);
}

static void test_sizeof_int(void)
{
    expr_t *e = ast_make_sizeof_type(TYPE_INT, 0, 0, 1, 1);
    long long val = 0;
    ASSERT(eval_const_expr(e, NULL, 0, &val));
    ASSERT(val == 4);
    ast_free_expr(e);
}

int main(void)
{
    test_sizeof_ptr_32();
    test_sizeof_ptr_64();
    test_sizeof_int();
    if (failures == 0)
        printf("All eval_sizeof tests passed\n");
    else
        printf("%d eval_sizeof test(s) failed\n", failures);
    return failures ? 1 : 0;
}
