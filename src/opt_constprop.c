/*
 * Constant propagation optimization pass.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include "opt.h"

typedef struct var_const {
    const char *name;
    int value;
    int known;
    struct var_const *next;
} var_const_t;

typedef struct {
    size_t max_id;
    int *is_const;
    int *values;
    var_const_t *vars;
} const_track_t;

/* Update destination entry in constant tracking tables */
static void update_const(const_track_t *ct, ir_instr_t *ins, int val, int cst)
{
    size_t max_id = ct->max_id;
    if (ins->dest >= 0 && (size_t)ins->dest < max_id) {
        ct->is_const[ins->dest] = cst;
        if (cst)
            ct->values[ins->dest] = val;
    }
}

/* Reset all entries in a variable constant list */
static void clear_var_list(var_const_t *head)
{
    for (var_const_t *v = head; v; v = v->next)
        v->known = 0;
}

/* Allocate arrays used for constant tracking */
static int init_const_tracking(const_track_t *ct, ir_builder_t *ir)
{
    ct->max_id = ir->next_value_id;
    ct->is_const = calloc(ct->max_id, sizeof(int));
    ct->values = calloc(ct->max_id, sizeof(int));
    ct->vars = NULL;
    if (!ct->is_const || !ct->values) {
        opt_error("out of memory");
        free(ct->is_const);
        free(ct->values);
        return 0;
    }
    return 1;
}

/* Free all memory owned by the constant tracker */
static void free_const_tracking(const_track_t *ct)
{
    free(ct->is_const);
    free(ct->values);
    while (ct->vars) {
        var_const_t *next = ct->vars->next;
        free(ct->vars);
        ct->vars = next;
    }
}

/* Evaluate a long double binary op */
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

/* Handle constant propagation through an IR_STORE instruction */
static void handle_store(const_track_t *ct, ir_instr_t *ins, int in_loop)
{
    var_const_t *v = ct->vars;
    size_t max_id = ct->max_id;
    while (v && strcmp(v->name, ins->name) != 0)
        v = v->next;
    if (!v) {
        v = calloc(1, sizeof(*v));
        if (!v) {
            opt_error("out of memory");
            return;
        }
        v->name = ins->name;
        v->next = ct->vars;
        ct->vars = v;
    }
    if (!in_loop && !ins->is_volatile &&
        (size_t)ins->src1 < max_id && ct->is_const[ins->src1]) {
        v->known = 1;
        v->value = ct->values[ins->src1];
    } else {
        v->known = 0;
    }
}

/* Handle constant propagation through an IR_LOAD instruction */
static void handle_load(const_track_t *ct, ir_instr_t *ins, int in_loop)
{
    var_const_t *v = ct->vars;
    size_t max_id = ct->max_id;
    while (v && strcmp(v->name, ins->name) != 0)
        v = v->next;
    if (!in_loop && !ins->is_volatile && v && v->known) {
        free(ins->name);
        ins->name = NULL;
        ins->op = IR_CONST;
        ins->imm = v->value;
        if (ins->dest >= 0 && (size_t)ins->dest < max_id) {
            ct->is_const[ins->dest] = 1;
            ct->values[ins->dest] = v->value;
        }
    } else if (ins->dest >= 0 && (size_t)ins->dest < max_id) {
        ct->is_const[ins->dest] = 0;
    }
}

/* Update constant tracking information for a single instruction */
static void propagate_through_instruction(const_track_t *ct, ir_instr_t *ins,
                                          int in_loop)
{
    size_t max_id = ct->max_id;
    switch (ins->op) {
    case IR_CONST:
        if (ins->dest >= 0 && (size_t)ins->dest < max_id) {
            ct->is_const[ins->dest] = 1;
            ct->values[ins->dest] = (int)ins->imm;
        }
        break;
    case IR_STORE:
    case IR_BFSTORE:
        handle_store(ct, ins, in_loop);
        break;
    case IR_LOAD:
    case IR_BFLOAD:
        handle_load(ct, ins, in_loop);
        break;
    case IR_STORE_PTR:
    case IR_STORE_IDX:
    case IR_CALL:
    case IR_CALL_PTR:
    case IR_CALL_NR:
    case IR_CALL_PTR_NR:
    case IR_ARG:
        clear_var_list(ct->vars);
        if (ins->dest >= 0 && (size_t)ins->dest < max_id)
            ct->is_const[ins->dest] = 0;
        break;
    case IR_LOGAND: case IR_LOGOR:
    case IR_LOAD_PARAM:
    case IR_ADDR:
    case IR_LOAD_PTR:
    case IR_LOAD_IDX:
    case IR_ALLOCA:
    case IR_STORE_PARAM:
    case IR_RETURN:
    case IR_RETURN_AGG:
    case IR_FUNC_BEGIN:
    case IR_FUNC_END:
    case IR_GLOB_STRING:
    case IR_GLOB_WSTRING:
    case IR_GLOB_VAR:
    case IR_GLOB_ARRAY:
    case IR_GLOB_UNION:
    case IR_GLOB_STRUCT:
    case IR_GLOB_ADDR:
    case IR_BR:
    case IR_BCOND:
    case IR_LABEL:
    case IR_LFADD: case IR_LFSUB: case IR_LFMUL: case IR_LFDIV:
        if (sizeof(long double) <= sizeof(int) &&
            ins->dest >= 0 && (size_t)ins->dest < max_id &&
            (size_t)ins->src1 < max_id && (size_t)ins->src2 < max_id &&
            ct->is_const[ins->src1] && ct->is_const[ins->src2]) {
            int ia = ct->values[ins->src1];
            int ib = ct->values[ins->src2];
            long double a = 0.0L;
            long double b = 0.0L;
            memcpy(&a, &ia, sizeof(a));
            memcpy(&b, &ib, sizeof(b));
            uint64_t r = eval_long_float_op(ins->op, a, b);
            ct->is_const[ins->dest] = 1;
            ct->values[ins->dest] = (int)r;
        } else if (ins->dest >= 0 && (size_t)ins->dest < max_id) {
            ct->is_const[ins->dest] = 0;
        }
        break;
    case IR_ADD: case IR_SUB: case IR_MUL: case IR_DIV: case IR_MOD:
    case IR_SHL: case IR_SHR: case IR_AND: case IR_OR: case IR_XOR:
    case IR_FADD: case IR_FSUB: case IR_FMUL: case IR_FDIV:
    case IR_PTR_ADD:
    case IR_PTR_DIFF:
    case IR_CMPEQ: case IR_CMPNE: case IR_CMPLT:
    case IR_CMPGT: case IR_CMPLE: case IR_CMPGE:
        if (ins->dest >= 0 && (size_t)ins->dest < max_id)
            ct->is_const[ins->dest] = 0;
        if (ins->op == IR_FUNC_BEGIN)
            clear_var_list(ct->vars);
        break;
    case IR_CAST:
    case IR_CPLX_CONST:
    case IR_CPLX_ADD: case IR_CPLX_SUB:
    case IR_CPLX_MUL: case IR_CPLX_DIV:
        update_const(ct, ins, 0, 0);
        break;
    }
}

/* Traverse all instructions applying store/load propagation */
static int loop_start(ir_instr_t *lbl, ir_instr_t **end_out)
{
    if (!lbl || lbl->op != IR_LABEL)
        return 0;
    ir_instr_t *bcond = lbl->next;
    if (!bcond || bcond->op != IR_BCOND)
        return 0;
    ir_instr_t *br = bcond->next;
    while (br && !(br->op == IR_BR && br->name && strcmp(br->name, lbl->name) == 0))
        br = br->next;
    if (!br)
        return 0;
    for (ir_instr_t *i = bcond->next; i && i != br; i = i->next)
        if (i->op == IR_LABEL)
            return 0;
    *end_out = br;
    return 1;
}

static void process_instructions(ir_builder_t *ir, const_track_t *ct)
{
    int in_loop = 0;
    ir_instr_t *loop_end = NULL;
    for (ir_instr_t *ins = ir->head; ins; ins = ins->next) {
        if (!in_loop && ins->op == IR_LABEL && loop_start(ins, &loop_end))
            in_loop = 1;

        propagate_through_instruction(ct, ins, in_loop);

        if (in_loop && ins == loop_end)
            in_loop = 0;
    }
}

/* Top level constant propagation pass */
/*
 * Step 1: initialize constant tracking tables
 * Step 2: process store/load instructions
 * Step 3: free tracking tables
 */
void propagate_load_consts(ir_builder_t *ir)
{
    if (!ir)
        return;

    const_track_t ct;
    if (!init_const_tracking(&ct, ir))
        return;

    process_instructions(ir, &ct);

    free_const_tracking(&ct);
}

