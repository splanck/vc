#include <stdio.h>
#include <string.h>
#include "ir_core.h"
#include "opt.h"

static int failures = 0;
#define ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "Assertion failed: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
        failures++; \
    } \
} while (0)

static void test_hoist_simple(void)
{
    ir_builder_t ir;
    ir_builder_init(&ir);
    ir_build_func_begin(&ir, "f");
    ir_value_t p = ir_build_load_param(&ir, 0);
    ir_build_label(&ir, "L1");
    ir_value_t c = ir_build_const(&ir, 0);
    ir_build_bcond(&ir, c, "L2");
    ir_value_t mul = ir_build_binop(&ir, IR_MUL, p, p);
    ir_build_br(&ir, "L1");
    ir_build_label(&ir, "L2");
    ir_build_return(&ir, mul);
    ir_build_func_end(&ir);

    opt_run(&ir, NULL);

    /* expect MUL moved before L1 label */
    int seen_mul_before = 0;
    int reached_label = 0;
    for (ir_instr_t *i = ir.head; i; i = i->next) {
        if (i->op == IR_LABEL && strcmp(i->name, "L1") == 0)
            reached_label = 1;
        if (i->op == IR_MUL && !reached_label)
            seen_mul_before = 1;
    }
    ASSERT(seen_mul_before);

    ir_builder_free(&ir);
}

int main(void)
{
    test_hoist_simple();
    if (failures == 0)
        printf("All licm tests passed\n");
    else
        printf("%d licm test(s) failed\n", failures);
    return failures ? 1 : 0;
}
