#include <stdio.h>
#include "ast_expr.h"
#include "consteval.h"

int main(void)
{
    /* LLONG_MAX + 1 */
    expr_t *lhs = ast_make_number("9223372036854775807", 1, 1);
    expr_t *rhs = ast_make_number("1", 1, 1);
    expr_t *add = ast_make_binary(BINOP_ADD, lhs, rhs, 1, 1);
    long long val = 0;
    if (eval_const_expr(add, NULL, 0, &val)) {
        printf("add overflow not detected\n");
        ast_free_expr(add);
        return 1;
    }
    ast_free_expr(add);

    /* -LLONG_MIN */
    expr_t *min = ast_make_number("-9223372036854775808", 1, 1);
    expr_t *neg = ast_make_unary(UNOP_NEG, min, 1, 1);
    if (eval_const_expr(neg, NULL, 0, &val)) {
        printf("neg overflow not detected\n");
        ast_free_expr(neg);
        return 1;
    }
    ast_free_expr(neg);

    printf("All consteval_overflow tests passed\n");
    return 0;
}
