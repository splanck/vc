/*
 * Constant folding optimization pass.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include "opt.h"

/* Evaluate a binary integer op for constant folding */
static int eval_int_op(ir_op_t op, int a, int b)
{
    switch (op) {
    case IR_ADD: return a + b;
    case IR_SUB: return a - b;
    case IR_MUL: return a * b;
    case IR_DIV: return b != 0 ? a / b : 0;
    case IR_MOD: return b != 0 ? a % b : 0;
    case IR_SHL: return a << b;
    case IR_SHR: return a >> b;
    case IR_AND: return a & b;
    case IR_OR:  return a | b;
    case IR_XOR: return a ^ b;
    case IR_CMPEQ: return a == b;
    case IR_CMPNE: return a != b;
    case IR_CMPLT: return a < b;
    case IR_CMPGT: return a > b;
    case IR_CMPLE: return a <= b;
    case IR_CMPGE: return a >= b;
    case IR_LOGAND: return (a && b);
    case IR_LOGOR:  return (a || b);
    default: return 0;
    }
}

/* Evaluate a binary floating point op for constant folding */
static int eval_float_op(ir_op_t op, int a, int b)
{
    union { float f; int i; } fa = {.i = a}, fb = {.i = b}, res;
    switch (op) {
    case IR_FADD: res.f = fa.f + fb.f; break;
    case IR_FSUB: res.f = fa.f - fb.f; break;
    case IR_FMUL: res.f = fa.f * fb.f; break;
    case IR_FDIV: res.f = fb.f != 0.0f ? fa.f / fb.f : 0.0f; break;
    default: res.i = 0; break;
    }
    return res.i;
}

/* Update destination entry in constant tracking tables */
static void update_const(ir_instr_t *ins, int val, int cst,
                         int max_id, int *is_const, int *values)
{
    if (ins->dest >= 0 && ins->dest < max_id) {
        is_const[ins->dest] = cst;
        if (cst)
            values[ins->dest] = val;
    }
}

/* Try folding an integer binary operation */
static void fold_int_instr(ir_instr_t *ins, int max_id,
                           int *is_const, int *values)
{
    if (ins->src1 < max_id && ins->src2 < max_id &&
        is_const[ins->src1] && is_const[ins->src2]) {
        int a = values[ins->src1];
        int b = values[ins->src2];
        int result = eval_int_op(ins->op, a, b);
        ins->op = IR_CONST;
        ins->imm = result;
        ins->src1 = ins->src2 = 0;
        update_const(ins, result, 1, max_id, is_const, values);
    } else {
        update_const(ins, 0, 0, max_id, is_const, values);
    }
}

/* Try folding a floating point binary operation */
static void fold_float_instr(ir_instr_t *ins, int max_id,
                             int *is_const, int *values)
{
    if (ins->src1 < max_id && ins->src2 < max_id &&
        is_const[ins->src1] && is_const[ins->src2]) {
        int a = values[ins->src1];
        int b = values[ins->src2];
        int result = eval_float_op(ins->op, a, b);
        ins->op = IR_CONST;
        ins->imm = result;
        ins->src1 = ins->src2 = 0;
        update_const(ins, result, 1, max_id, is_const, values);
    } else {
        update_const(ins, 0, 0, max_id, is_const, values);
    }
}

/* Perform simple constant folding */
void fold_constants(ir_builder_t *ir)
{
    if (!ir)
        return;
    int max_id = ir->next_value_id;
    int *is_const = calloc((size_t)max_id, sizeof(int));
    int *values = calloc((size_t)max_id, sizeof(int));
    if (!is_const || !values) {
        opt_error("out of memory");
        free(is_const);
        free(values);
        return;
    }

    for (ir_instr_t *ins = ir->head; ins; ins = ins->next) {
        switch (ins->op) {
        case IR_CONST:
            update_const(ins, (int)ins->imm, 1, max_id, is_const, values);
            break;
        case IR_ADD: case IR_SUB: case IR_MUL: case IR_DIV: case IR_MOD:
        case IR_SHL: case IR_SHR: case IR_AND: case IR_OR: case IR_XOR:
        case IR_CMPEQ: case IR_CMPNE: case IR_CMPLT:
        case IR_CMPGT: case IR_CMPLE: case IR_CMPGE:
        case IR_LOGAND: case IR_LOGOR:
            fold_int_instr(ins, max_id, is_const, values);
            break;
        case IR_FADD: case IR_FSUB: case IR_FMUL: case IR_FDIV:
            fold_float_instr(ins, max_id, is_const, values);
            break;
        case IR_LFADD: case IR_LFSUB: case IR_LFMUL: case IR_LFDIV:
            update_const(ins, 0, 0, max_id, is_const, values);
            break;
        case IR_LOAD:
        case IR_LOAD_IDX:
            update_const(ins, 0, 0, max_id, is_const, values);
            break;
        case IR_STORE:
        case IR_LOAD_PARAM:
        case IR_STORE_IDX:
            update_const(ins, 0, 0, max_id, is_const, values);
            break;
        case IR_STORE_PARAM:
        case IR_ADDR:
        case IR_LOAD_PTR:
        case IR_STORE_PTR:
        case IR_PTR_ADD:
        case IR_PTR_DIFF:
        case IR_ALLOCA:
            update_const(ins, 0, 0, max_id, is_const, values);
            break;
        case IR_RETURN:
            /* nothing to do */
            break;
        case IR_GLOB_STRING:
        case IR_GLOB_VAR:
        case IR_GLOB_ARRAY:
        case IR_GLOB_UNION:
        case IR_GLOB_STRUCT:
            update_const(ins, 0, 0, max_id, is_const, values);
            break;
        case IR_CALL: case IR_FUNC_BEGIN: case IR_FUNC_END: case IR_ARG:
            update_const(ins, 0, 0, max_id, is_const, values);
            break;
        case IR_BCOND: case IR_LABEL: case IR_BR:
            break;
        }
    }

    free(is_const);
    free(values);
}

