/*
 * Dead code elimination pass.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include "opt.h"

/* Check whether an instruction produces a side effect */
static int has_side_effect(ir_instr_t *ins)
{
    switch (ins->op) {
    case IR_STORE:
    case IR_STORE_PTR:
    case IR_STORE_IDX:
    case IR_BFSTORE:
    case IR_STORE_PARAM:
    case IR_CALL:
    case IR_CALL_PTR:
    case IR_ARG:
    case IR_RETURN:
    case IR_RETURN_AGG:
    case IR_BR:
    case IR_BCOND:
    case IR_FUNC_BEGIN:
    case IR_FUNC_END:
    case IR_LABEL:
    case IR_GLOB_VAR:
    case IR_GLOB_ARRAY:
    case IR_GLOB_UNION:
    case IR_GLOB_STRUCT:
        return 1;
    case IR_LOAD:
    case IR_LOAD_IDX:
    case IR_BFLOAD:
    case IR_LOAD_PTR:
        return ins->is_volatile || ins->op == IR_LOAD_PTR;
    default:
        return 0;
    }
}

/* Remove instructions whose results are unused */
void dead_code_elim(ir_builder_t *ir)
{
    if (!ir)
        return;

    size_t max_id = ir->next_value_id;
    int count = 0;
    for (ir_instr_t *i = ir->head; i; i = i->next)
        count++;

    ir_instr_t **list = malloc((size_t)count * sizeof(*list));
    if (!list) {
        opt_error("out of memory");
        return;
    }

    int idx = 0;
    for (ir_instr_t *i = ir->head; i; i = i->next)
        list[idx++] = i;

    int *used = calloc(max_id, sizeof(int));
    if (!used) {
        opt_error("out of memory");
        free(list);
        return;
    }

    for (int i = count - 1; i >= 0; i--) {
        ir_instr_t *ins = list[i];
        int dest = ins->dest;
        int side = has_side_effect(ins);

        if (dest >= 0 && !side && !used[dest]) {
            if (i == 0)
                ir->head = ins->next;
            else
                list[i - 1]->next = ins->next;

            if (ins == ir->tail)
                ir->tail = (i == 0) ? NULL : list[i - 1];

            free(ins->name);
            free(ins->data);
            free(ins);
            continue;
        }

        if (ins->src1 >= 0 && (size_t)ins->src1 < max_id)
            used[ins->src1] = 1;
        if (ins->src2 >= 0 && (size_t)ins->src2 < max_id)
            used[ins->src2] = 1;
    }

    free(used);
    free(list);
}

