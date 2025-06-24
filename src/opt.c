#include <stdlib.h>
#include "opt.h"

/* Simple constant folding optimization pass */
static void fold_constants(ir_builder_t *ir)
{
    if (!ir)
        return;
    int max_id = ir->next_value_id;
    int *is_const = calloc((size_t)max_id, sizeof(int));
    int *values = calloc((size_t)max_id, sizeof(int));
    if (!is_const || !values) {
        free(is_const);
        free(values);
        return;
    }

    for (ir_instr_t *ins = ir->head; ins; ins = ins->next) {
        switch (ins->op) {
        case IR_CONST:
            if (ins->dest >= 0 && ins->dest < max_id) {
                is_const[ins->dest] = 1;
                values[ins->dest] = ins->imm;
            }
            break;
        case IR_ADD: case IR_SUB: case IR_MUL: case IR_DIV:
            if (ins->src1 < max_id && ins->src2 < max_id &&
                is_const[ins->src1] && is_const[ins->src2]) {
                int a = values[ins->src1];
                int b = values[ins->src2];
                int result = 0;
                switch (ins->op) {
                case IR_ADD: result = a + b; break;
                case IR_SUB: result = a - b; break;
                case IR_MUL: result = a * b; break;
                case IR_DIV: result = b != 0 ? a / b : 0; break;
                default: break;
                }
                ins->op = IR_CONST;
                ins->imm = result;
                ins->src1 = ins->src2 = 0;
                if (ins->dest >= 0 && ins->dest < max_id) {
                    is_const[ins->dest] = 1;
                    values[ins->dest] = result;
                }
            } else if (ins->dest >= 0 && ins->dest < max_id) {
                is_const[ins->dest] = 0;
            }
            break;
        case IR_LOAD:
            if (ins->dest >= 0 && ins->dest < max_id)
                is_const[ins->dest] = 0;
            break;
        case IR_STORE:
            break;
        case IR_RETURN:
            /* nothing to do */
            break;
        case IR_CALL: case IR_FUNC_BEGIN: case IR_FUNC_END:
            if (ins->dest >= 0 && ins->dest < max_id)
                is_const[ins->dest] = 0;
            break;
        case IR_BCOND: case IR_LABEL: case IR_BR:
            break;
        }
    }

    free(is_const);
    free(values);
}

void opt_run(ir_builder_t *ir)
{
    fold_constants(ir);
}

