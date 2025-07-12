/*
 * Constant folding optimization pass.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
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
    float fa = 0.0f;
    float fb = 0.0f;
    float res;

    memcpy(&fa, &a, sizeof(fa));
    memcpy(&fb, &b, sizeof(fb));

    switch (op) {
    case IR_FADD: res = fa + fb; break;
    case IR_FSUB: res = fa - fb; break;
    case IR_FMUL: res = fa * fb; break;
    case IR_FDIV: res = fb != 0.0f ? fa / fb : 0.0f; break;
    default:      res = 0.0f; break;
    }

    int out = 0;
    memcpy(&out, &res, sizeof(out));
    return out;
}

/* Evaluate a binary long double op for constant folding */
static uint64_t eval_long_float_op(ir_op_t op, long double a, long double b)
{
    if (sizeof(long double) > sizeof(uint64_t))
        return 0;

    long double res;

    switch (op) {
    case IR_LFADD: res = a + b; break;
    case IR_LFSUB: res = a - b; break;
    case IR_LFMUL: res = a * b; break;
    case IR_LFDIV: res = b != 0.0L ? a / b : 0.0L; break;
    default:       res = 0.0L; break;
    }

    uint64_t out = 0;
    memcpy(&out, &res, sizeof(res) > sizeof(out) ? sizeof(out) : sizeof(res));
    return out;
}

/* Evaluate pointer addition for constant folding */
static int eval_ptr_add(int base, int idx, int esz)
{
    return base + idx * esz;
}

/* Evaluate pointer difference for constant folding */
static int eval_ptr_diff(int a, int b, int esz)
{
    return esz ? (a - b) / esz : 0;
}

/* Update destination entry in constant tracking tables */
static void update_const(ir_instr_t *ins, int val, int cst,
                         size_t max_id, int *is_const, int *values)
{
    if (ins->dest >= 0 && (size_t)ins->dest < max_id) {
        is_const[ins->dest] = cst;
        if (cst)
            values[ins->dest] = val;
    }
}

/* Try folding an integer binary operation */
static void fold_int_instr(ir_instr_t *ins, size_t max_id,
                           int *is_const, int *values)
{
    if ((size_t)ins->src1 < max_id && (size_t)ins->src2 < max_id &&
        is_const[ins->src1] && is_const[ins->src2]) {
        int a = values[ins->src1];
        int b = values[ins->src2];
        int result = eval_int_op(ins->op, a, b);
        ins->op = IR_CONST;
        ins->imm = (long long)result;
        ins->src1 = ins->src2 = 0;
        update_const(ins, result, 1, max_id, is_const, values);
    } else {
        update_const(ins, 0, 0, max_id, is_const, values);
    }
}

/* Try folding a floating point binary operation */
static void fold_float_instr(ir_instr_t *ins, size_t max_id,
                             int *is_const, int *values)
{
    if ((size_t)ins->src1 < max_id && (size_t)ins->src2 < max_id &&
        is_const[ins->src1] && is_const[ins->src2]) {
        int a = values[ins->src1];
        int b = values[ins->src2];
        int result = eval_float_op(ins->op, a, b);
        ins->op = IR_CONST;
        ins->imm = (long long)result;
        ins->src1 = ins->src2 = 0;
        update_const(ins, result, 1, max_id, is_const, values);
    } else {
        update_const(ins, 0, 0, max_id, is_const, values);
    }
}

/* Try folding a long double binary operation */
static void fold_long_float_instr(ir_instr_t *ins, size_t max_id,
                                  int *is_const, int *values)
{
    if (sizeof(long double) > sizeof(int)) {
        update_const(ins, 0, 0, max_id, is_const, values);
        return;
    }
    if ((size_t)ins->src1 < max_id && (size_t)ins->src2 < max_id &&
        is_const[ins->src1] && is_const[ins->src2]) {
        int ia = values[ins->src1];
        int ib = values[ins->src2];
        long double a = 0.0L;
        long double b = 0.0L;
        memcpy(&a, &ia, sizeof(a));
        memcpy(&b, &ib, sizeof(b));
        uint64_t result = eval_long_float_op(ins->op, a, b);
        ins->op = IR_CONST;
        ins->imm = (long long)result;
        ins->src1 = ins->src2 = 0;
        update_const(ins, (int)result, 1, max_id, is_const, values);
    } else {
        update_const(ins, 0, 0, max_id, is_const, values);
    }
}

/* Try folding a cast operation */
static void fold_cast_instr(ir_instr_t *ins, size_t max_id,
                            int *is_const, int *values)
{
    if ((size_t)ins->src1 < max_id && is_const[ins->src1]) {
        int val = values[ins->src1];
        ins->op = IR_CONST;
        ins->imm = val;
        ins->src1 = ins->src2 = 0;
        update_const(ins, val, 1, max_id, is_const, values);
    } else {
        update_const(ins, 0, 0, max_id, is_const, values);
    }
}

/* Try folding pointer addition */
static void fold_ptr_add_instr(ir_instr_t *ins, size_t max_id,
                               int *is_const, int *values)
{
    if ((size_t)ins->src1 < max_id && (size_t)ins->src2 < max_id &&
        is_const[ins->src1] && is_const[ins->src2]) {
        int base = values[ins->src1];
        int idx = values[ins->src2];
        int result = eval_ptr_add(base, idx, (int)ins->imm);
        ins->op = IR_CONST;
        ins->imm = result;
        ins->src1 = ins->src2 = 0;
        update_const(ins, result, 1, max_id, is_const, values);
    } else {
        update_const(ins, 0, 0, max_id, is_const, values);
    }
}

/* Try folding pointer difference */
static void fold_ptr_diff_instr(ir_instr_t *ins, size_t max_id,
                                int *is_const, int *values)
{
    if ((size_t)ins->src1 < max_id && (size_t)ins->src2 < max_id &&
        is_const[ins->src1] && is_const[ins->src2]) {
        int a = values[ins->src1];
        int b = values[ins->src2];
        int result = eval_ptr_diff(a, b, (int)ins->imm);
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
    size_t max_id = ir->next_value_id;
    int *is_const = calloc(max_id, sizeof(int));
    int *values = calloc(max_id, sizeof(int));
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
        case IR_CAST:
            fold_cast_instr(ins, max_id, is_const, values);
            break;
        case IR_FADD: case IR_FSUB: case IR_FMUL: case IR_FDIV:
            fold_float_instr(ins, max_id, is_const, values);
            break;
        case IR_LFADD: case IR_LFSUB: case IR_LFMUL: case IR_LFDIV:
            fold_long_float_instr(ins, max_id, is_const, values);
            break;
        case IR_CPLX_CONST:
        case IR_CPLX_ADD: case IR_CPLX_SUB:
        case IR_CPLX_MUL: case IR_CPLX_DIV:
            update_const(ins, 0, 0, max_id, is_const, values);
            break;
        case IR_LOAD:
        case IR_LOAD_IDX:
        case IR_BFLOAD:
            update_const(ins, 0, 0, max_id, is_const, values);
            break;
        case IR_STORE:
        case IR_LOAD_PARAM:
        case IR_STORE_IDX:
        case IR_BFSTORE:
            update_const(ins, 0, 0, max_id, is_const, values);
            break;
        case IR_STORE_PARAM:
        case IR_ADDR:
        case IR_LOAD_PTR:
        case IR_STORE_PTR:
            update_const(ins, 0, 0, max_id, is_const, values);
            break;
        case IR_PTR_ADD:
            fold_ptr_add_instr(ins, max_id, is_const, values);
            break;
        case IR_PTR_DIFF:
            fold_ptr_diff_instr(ins, max_id, is_const, values);
            break;
        case IR_ALLOCA:
            update_const(ins, 0, 0, max_id, is_const, values);
            break;
        case IR_RETURN:
        case IR_RETURN_AGG:
            /* nothing to do */
            break;
        case IR_GLOB_STRING:
        case IR_GLOB_WSTRING:
        case IR_GLOB_VAR:
        case IR_GLOB_ARRAY:
        case IR_GLOB_UNION:
        case IR_GLOB_STRUCT:
        case IR_GLOB_ADDR:
            update_const(ins, 0, 0, max_id, is_const, values);
            break;
        case IR_CALL: case IR_CALL_PTR:
        case IR_CALL_NR: case IR_CALL_PTR_NR:
        case IR_FUNC_BEGIN: case IR_FUNC_END: case IR_ARG:
            update_const(ins, 0, 0, max_id, is_const, values);
            break;
        case IR_BCOND: case IR_LABEL: case IR_BR:
            break;
        }
    }

    free(is_const);
    free(values);
}

