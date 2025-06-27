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

/* Evaluate a binary long double op for constant folding */
static long long eval_long_float_op(ir_op_t op, long long a, long long b)
{
    union { long double f; long long i; } fa = {.i = a}, fb = {.i = b}, res;
    switch (op) {
    case IR_LFADD: res.f = fa.f + fb.f; break;
    case IR_LFSUB: res.f = fa.f - fb.f; break;
    case IR_LFMUL: res.f = fa.f * fb.f; break;
    case IR_LFDIV: res.f = fb.f != 0.0L ? fa.f / fb.f : 0.0L; break;
    default: res.i = 0; break;
    }
    return res.i;
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
            if (ins->dest >= 0 && ins->dest < max_id) {
                is_const[ins->dest] = 1;
                values[ins->dest] = ins->imm;
            }
            break;
        case IR_ADD: case IR_SUB: case IR_MUL: case IR_DIV: case IR_MOD:
        case IR_SHL: case IR_SHR: case IR_AND: case IR_OR: case IR_XOR:
        case IR_CMPEQ: case IR_CMPNE: case IR_CMPLT:
        case IR_CMPGT: case IR_CMPLE: case IR_CMPGE:
        case IR_LOGAND: case IR_LOGOR:
            if (ins->src1 < max_id && ins->src2 < max_id &&
                is_const[ins->src1] && is_const[ins->src2]) {
                int a = values[ins->src1];
                int b = values[ins->src2];
                int result = eval_int_op(ins->op, a, b);
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
        case IR_FADD: case IR_FSUB: case IR_FMUL: case IR_FDIV:
        case IR_LFADD: case IR_LFSUB: case IR_LFMUL: case IR_LFDIV:
            if (ins->src1 < max_id && ins->src2 < max_id &&
                is_const[ins->src1] && is_const[ins->src2]) {
                long long a = values[ins->src1];
                long long b = values[ins->src2];
                long long result;
                if (ins->op == IR_LFADD || ins->op == IR_LFSUB ||
                    ins->op == IR_LFMUL || ins->op == IR_LFDIV)
                    result = eval_long_float_op(ins->op, a, b);
                else
                    result = eval_float_op(ins->op, (int)a, (int)b);
                ins->op = IR_CONST;
                ins->imm = result;
                ins->src1 = ins->src2 = 0;
                if (ins->dest >= 0 && ins->dest < max_id) {
                    is_const[ins->dest] = 1;
                    values[ins->dest] = (int)result;
                }
            } else if (ins->dest >= 0 && ins->dest < max_id) {
                is_const[ins->dest] = 0;
            }
            break;
        case IR_LOAD:
        case IR_LOAD_IDX:
            if (ins->dest >= 0 && ins->dest < max_id)
                is_const[ins->dest] = 0;
            break;
        case IR_STORE:
        case IR_LOAD_PARAM:
        case IR_STORE_IDX:
            if (ins->dest >= 0 && ins->dest < max_id)
                is_const[ins->dest] = 0;
            break;
        case IR_STORE_PARAM:
        case IR_ADDR:
        case IR_LOAD_PTR:
        case IR_STORE_PTR:
        case IR_PTR_ADD:
        case IR_PTR_DIFF:
        case IR_ALLOCA:
            if (ins->dest >= 0 && ins->dest < max_id)
                is_const[ins->dest] = 0;
            break;
        case IR_RETURN:
            /* nothing to do */
            break;
        case IR_GLOB_STRING:
        case IR_GLOB_VAR:
        case IR_GLOB_ARRAY:
        case IR_GLOB_UNION:
        case IR_GLOB_STRUCT:
            if (ins->dest >= 0 && ins->dest < max_id)
                is_const[ins->dest] = 0;
            break;
        case IR_CALL: case IR_FUNC_BEGIN: case IR_FUNC_END: case IR_ARG:
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

