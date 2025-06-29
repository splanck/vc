#include <stdio.h>
#include "ast_expr.h"
#include "semantic_expr.h"
#include "symtable.h"
#include "ir_core.h"
#include "error.h"

static int failures = 0;
#define ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "Assertion failed: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
        failures++; \
    } \
} while (0)

static void test_malformed_conditional(void)
{
    symtable_t vars, funcs;
    symtable_init(&vars);
    symtable_init(&funcs);
    ir_builder_t ir;
    ir_builder_init(&ir);

    /* 1 ? x : 0 where x is undefined */
    expr_t *cond = ast_make_number("1", 1, 1);
    expr_t *then_expr = ast_make_ident("x", 1, 1);
    expr_t *else_expr = ast_make_number("0", 1, 1);
    expr_t *ce = ast_make_cond(cond, then_expr, else_expr, 1, 1);

    type_kind_t t = check_expr(ce, &vars, &funcs, &ir, NULL);
    ASSERT(t == TYPE_UNKNOWN);
    /* ensure no IR was produced for an invalid expression */
    ASSERT(ir.head == NULL);

    ir_builder_free(&ir);
    ast_free_expr(ce);
    symtable_free(&vars);
    symtable_free(&funcs);
}

int main(void)
{
    test_malformed_conditional();
    if (failures == 0)
        printf("All cond_expr tests passed\n");
    else
        printf("%d cond_expr test(s) failed\n", failures);
    return failures ? 1 : 0;
}
