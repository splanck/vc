/*
 * Common subexpression elimination pass.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include "opt.h"

typedef struct expr_entry {
    ir_op_t op;
    int src1;
    int src2;
    long long imm;
    int dest;
    struct expr_entry *next;
} expr_entry_t;

static int is_commutative(ir_op_t op)
{
    switch (op) {
    case IR_ADD: case IR_MUL: case IR_AND: case IR_OR: case IR_XOR:
    case IR_FADD: case IR_FMUL:
    case IR_LFADD: case IR_LFMUL:
    case IR_CMPEQ: case IR_CMPNE: case IR_LOGAND: case IR_LOGOR:
        return 1;
    default:
        return 0;
    }
}

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
        return 1;
    default:
        return 0;
    }
}

void common_subexpr_elim(ir_builder_t *ir)
{
    if (!ir)
        return;

    expr_entry_t *list = NULL;

    for (ir_instr_t *ins = ir->head; ins; ins = ins->next) {
        if (!is_pure_op(ins->op))
            continue;

        int a = ins->src1;
        int b = ins->src2;
        if (is_commutative(ins->op) && a > b) {
            int t = a; a = b; b = t;
        }

        expr_entry_t *e;
        for (e = list; e; e = e->next) {
            if (e->op == ins->op && e->src1 == a && e->src2 == b &&
                e->imm == ins->imm) {
                int old = e->dest;
                for (ir_instr_t *u = ins->next; u; u = u->next) {
                    if (u->src1 == ins->dest)
                        u->src1 = old;
                    if (u->src2 == ins->dest)
                        u->src2 = old;
                }
                break;
            }
        }
        if (!e) {
            e = malloc(sizeof(*e));
            if (!e) {
                opt_error("out of memory");
                break;
            }
            e->op = ins->op;
            e->src1 = a;
            e->src2 = b;
            e->imm = ins->imm;
            e->dest = ins->dest;
            e->next = list;
            list = e;
        }
    }

    while (list) {
        expr_entry_t *n = list->next;
        free(list);
        list = n;
    }
}

