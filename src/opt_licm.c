/*
 * Loop invariant code motion pass.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include <string.h>
#include "opt.h"

static int is_pure_op(ir_op_t op)
{
    switch (op) {
    case IR_ADD: case IR_SUB: case IR_MUL: case IR_DIV: case IR_MOD:
    case IR_SHL: case IR_SHR: case IR_AND: case IR_OR: case IR_XOR:
    case IR_FADD: case IR_FSUB: case IR_FMUL: case IR_FDIV:
    case IR_LFADD: case IR_LFSUB: case IR_LFMUL: case IR_LFDIV:
    case IR_PTR_ADD: case IR_PTR_DIFF:
    case IR_CMPEQ: case IR_CMPNE: case IR_CMPLT: case IR_CMPGT:
    case IR_CMPLE: case IR_CMPGE:
    case IR_LOGAND: case IR_LOGOR:
    case IR_CONST:
        return 1;
    default:
        return 0;
    }
}

static void hoist(ir_instr_t *ins, ir_instr_t *before,
                  ir_builder_t *ir)
{
    if (before) {
        ins->next = before->next;
        before->next = ins;
    } else {
        ins->next = ir->head;
        ir->head = ins;
    }
}

void opt_licm(ir_builder_t *ir)
{
    if (!ir)
        return;

    ir_instr_t *prev = NULL;
    for (ir_instr_t *lbl = ir->head; lbl; prev = lbl, lbl = lbl->next) {
        if (lbl->op != IR_LABEL)
            continue;
        ir_instr_t *bcond = lbl->next;
        if (!bcond || bcond->op != IR_BCOND)
            continue;
        ir_instr_t *br = bcond->next;
        while (br && !(br->op == IR_BR && br->name &&
                       strcmp(br->name, lbl->name) == 0))
            br = br->next;
        if (!br)
            continue;
        /* ensure no other labels inside */
        int has_label = 0;
        for (ir_instr_t *i = bcond->next; i && i != br; i = i->next)
            if (i->op == IR_LABEL)
                has_label = 1;
        if (has_label)
            continue;

        size_t max_id = ir->next_value_id;
        int *defined = calloc(max_id, sizeof(int));
        if (!defined)
            return;
        for (ir_instr_t *i = bcond->next; i && i != br; i = i->next) {
            int invariant = is_pure_op(i->op);
            if (invariant) {
                if (i->src1 > 0 && (size_t)i->src1 < max_id && defined[i->src1])
                    invariant = 0;
                if (i->src2 > 0 && (size_t)i->src2 < max_id && defined[i->src2])
                    invariant = 0;
            }
            if (invariant) {
                /* remove from list */
                ir_instr_t *next = i->next;
                bcond->next = next;
                if (br == i)
                    break;
                hoist(i, prev, ir);
                i = bcond; /* restart from bcond */
            } else {
                if (i->dest > 0 && (size_t)i->dest < max_id)
                    defined[i->dest] = 1;
                bcond = i;
            }
        }
        free(defined);
    }
}
