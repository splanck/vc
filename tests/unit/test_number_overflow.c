#include <stdio.h>
#include "ast_expr.h"
#include "consteval.h"

int main(void)
{
    /* LLONG_MAX + 1 */
    expr_t *e = ast_make_number("9223372036854775808", 1, 1);
    long long val = 0;
    if (eval_const_expr(e, NULL, 0, &val)) {
        printf("overflow not detected\n");
        ast_free_expr(e);
        return 1;
    }
    ast_free_expr(e);
    printf("All number_overflow tests passed\n");
    return 0;
}
