#include <stdio.h>
#include <string.h>
#include "ast_expr.h"
#include "semantic_expr.h"
#include "symtable.h"
#include "ir_core.h"

static int failures;
#define ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "Assertion failed: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
        failures++; \
    } \
} while (0)

static void check_flags(const char *lit, int u, int lcnt, type_kind_t expect)
{
    expr_t *e = ast_make_number(lit, 1, 1);
    ASSERT(e);
    ASSERT(e->number.is_unsigned == u);
    ASSERT(e->number.long_count == lcnt);
    ir_builder_t ir; ir_builder_init(&ir);
    symtable_t vars, funcs; symtable_init(&vars); symtable_init(&funcs);
    type_kind_t t = check_expr(e, &vars, &funcs, &ir, NULL);
    ASSERT(t == expect);
    ir_builder_free(&ir);
    symtable_free(&vars); symtable_free(&funcs);
    ast_free_expr(e);
}

int main(void)
{
    check_flags("1u", 1, 0, TYPE_UINT);
    check_flags("2ul", 1, 1, TYPE_ULONG);
    check_flags("3llu", 1, 2, TYPE_ULLONG);
    check_flags("4ll", 0, 2, TYPE_LLONG);
    if (failures == 0)
        printf("All number_suffix tests passed\n");
    else
        printf("%d number_suffix test(s) failed\n", failures);
    return failures ? 1 : 0;
}
