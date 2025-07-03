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

static void test_remove_blocks(void)
{
    ir_builder_t ir;
    ir_builder_init(&ir);
    ir_build_func_begin(&ir, "f");
    ir_value_t v = ir_build_const(&ir, 1);
    ir_build_br(&ir, "L1");
    ir_build_store(&ir, "x", v); /* unreachable */
    ir_build_label(&ir, "L1");
    ir_build_return(&ir, v);
    ir_build_store(&ir, "x", v); /* unreachable */
    ir_build_func_end(&ir);

    opt_run(&ir, NULL);

    int count = 0;
    for (ir_instr_t *i = ir.head; i; i = i->next)
        count++;
    ASSERT(count == 6);

    ir_instr_t *i = ir.head;
    ASSERT(i && i->op == IR_FUNC_BEGIN); i = i->next;
    ASSERT(i && i->op == IR_CONST); i = i->next;
    ASSERT(i && i->op == IR_BR); i = i->next;
    ASSERT(i && i->op == IR_LABEL && strcmp(i->name, "L1") == 0); i = i->next;
    ASSERT(i && i->op == IR_RETURN); i = i->next;
    ASSERT(i && i->op == IR_FUNC_END && i->next == NULL);

    ir_builder_free(&ir);
}

int main(void)
{
    test_remove_blocks();
    if (failures == 0)
        printf("All opt_unreachable tests passed\n");
    else
        printf("%d opt_unreachable test(s) failed\n", failures);
    return failures ? 1 : 0;
}
