#include <stdio.h>
#include <string.h>
#include "ast_expr.h"
#include "symtable.h"
#include "semantic_expr.h"
#include "ir_core.h"

static int failures = 0;
#define ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "Assertion failed: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
        failures++; \
    } \
} while (0)

static void test_complex_literal(void)
{
    expr_t *e = ast_make_complex_literal(1.0, 2.0, 1, 1);
    ir_builder_t ir; ir_builder_init(&ir);
    symtable_t vars, funcs; symtable_init(&vars); symtable_init(&funcs);
    ir_value_t val; type_kind_t t = check_expr(e, &vars, &funcs, &ir, &val);
    ASSERT(t == TYPE_DOUBLE_COMPLEX);
    ir_instr_t *ins = ir.head;
    ASSERT(ins && ins->op == IR_CPLX_CONST);
    ASSERT(ins->next == NULL);
    if (ins && ins->data) {
        double *vals = (double *)ins->data;
        ASSERT(vals[0] == 1.0 && vals[1] == 2.0);
    } else {
        ASSERT(0);
    }
    ir_builder_free(&ir);
    ast_free_expr(e);
    symtable_free(&vars); symtable_free(&funcs);
}

static void test_complex_assign(void)
{
    expr_t *const1i = ast_make_complex_literal(0.0, 1.0, 1, 1);
    expr_t *add = ast_make_binary(BINOP_ADD,
                                  ast_make_ident("c", 1, 1),
                                  const1i,
                                  1, 1);
    expr_t *assign = ast_make_assign("c", add, 1, 1);

    ir_builder_t ir; ir_builder_init(&ir);
    symtable_t vars, funcs; symtable_init(&vars); symtable_init(&funcs);
    symtable_add(&vars, "c", "c", TYPE_DOUBLE_COMPLEX, 0, 0, 0, 0, 0, 0, 0);

    ir_value_t val; type_kind_t t = check_expr(assign, &vars, &funcs, &ir, &val);
    ASSERT(t == TYPE_DOUBLE_COMPLEX);

    ir_instr_t *ins = ir.head;
    ASSERT(ins && ins->op == IR_LOAD && strcmp(ins->name, "c") == 0); ins = ins->next;
    ASSERT(ins && ins->op == IR_CPLX_CONST); ins = ins->next;
    ASSERT(ins && ins->op == IR_CPLX_ADD); ins = ins->next;
    ASSERT(ins && ins->op == IR_STORE && strcmp(ins->name, "c") == 0); ins = ins->next;
    ASSERT(ins == NULL);

    ir_builder_free(&ir);
    ast_free_expr(assign);
    symtable_free(&vars); symtable_free(&funcs);
}

int main(void)
{
    test_complex_literal();
    test_complex_assign();
    if (failures == 0)
        printf("All complex_expr tests passed\n");
    else
        printf("%d complex_expr test(s) failed\n", failures);
    return failures ? 1 : 0;
}
