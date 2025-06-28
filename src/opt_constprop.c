/*
 * Constant propagation optimization pass.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "opt.h"

typedef struct var_const {
    const char *name;
    int value;
    int known;
    struct var_const *next;
} var_const_t;

typedef struct {
    int max_id;
    int *is_const;
    int *values;
    var_const_t *vars;
} const_track_t;

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
    ct->is_const = calloc((size_t)ct->max_id, sizeof(int));
    ct->values = calloc((size_t)ct->max_id, sizeof(int));
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
static int eval_long_float_op(ir_op_t op, int a, int b)
{
    long double fa = (long double)a;
    long double fb = (long double)b;
    long double res;
    switch (op) {
    case IR_LFADD: res = fa + fb; break;
    case IR_LFSUB: res = fa - fb; break;
    case IR_LFMUL: res = fa * fb; break;
    case IR_LFDIV: res = fb != 0.0L ? fa / fb : 0.0L; break;
    default:       res = 0.0L; break;
    }
    return (int)res;
}

/* Handle constant propagation through an IR_STORE instruction */
static void handle_store(const_track_t *ct, ir_instr_t *ins)
{
    var_const_t *v = ct->vars;
    int max_id = ct->max_id;
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
    if (!ins->is_volatile && ins->src1 < max_id && ct->is_const[ins->src1]) {
        v->known = 1;
        v->value = ct->values[ins->src1];
    } else {
        v->known = 0;
    }
}

/* Handle constant propagation through an IR_LOAD instruction */
static void handle_load(const_track_t *ct, ir_instr_t *ins)
{
    var_const_t *v = ct->vars;
    int max_id = ct->max_id;
    while (v && strcmp(v->name, ins->name) != 0)
        v = v->next;
    if (!ins->is_volatile && v && v->known) {
        free(ins->name);
        ins->name = NULL;
        ins->op = IR_CONST;
        ins->imm = v->value;
        if (ins->dest >= 0 && ins->dest < max_id) {
            ct->is_const[ins->dest] = 1;
            ct->values[ins->dest] = v->value;
        }
    } else if (ins->dest >= 0 && ins->dest < max_id) {
        ct->is_const[ins->dest] = 0;
    }
}

/* Update constant tracking information for a single instruction */
static void propagate_through_instruction(const_track_t *ct, ir_instr_t *ins)
{
    int max_id = ct->max_id;
    switch (ins->op) {
    case IR_CONST:
        if (ins->dest >= 0 && ins->dest < max_id) {
            ct->is_const[ins->dest] = 1;
            ct->values[ins->dest] = ins->imm;
        }
        break;
    case IR_STORE:
    case IR_BFSTORE:
        handle_store(ct, ins);
        break;
    case IR_LOAD:
    case IR_BFLOAD:
        handle_load(ct, ins);
        break;
    case IR_STORE_PTR:
    case IR_STORE_IDX:
    case IR_CALL:
    case IR_CALL_PTR:
    case IR_ARG:
        clear_var_list(ct->vars);
        if (ins->dest >= 0 && ins->dest < max_id)
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
    case IR_FUNC_BEGIN:
    case IR_FUNC_END:
    case IR_GLOB_STRING:
    case IR_GLOB_VAR:
    case IR_GLOB_ARRAY:
    case IR_GLOB_UNION:
    case IR_GLOB_STRUCT:
    case IR_BR:
    case IR_BCOND:
    case IR_LABEL:
    case IR_LFADD: case IR_LFSUB: case IR_LFMUL: case IR_LFDIV:
        if (ins->dest >= 0 && ins->dest < max_id &&
            ins->src1 < max_id && ins->src2 < max_id &&
            ct->is_const[ins->src1] && ct->is_const[ins->src2]) {
            int a = ct->values[ins->src1];
            int b = ct->values[ins->src2];
            int r = eval_long_float_op(ins->op, a, b);
            ct->is_const[ins->dest] = 1;
            ct->values[ins->dest] = r;
        } else if (ins->dest >= 0 && ins->dest < max_id) {
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
        if (ins->dest >= 0 && ins->dest < max_id)
            ct->is_const[ins->dest] = 0;
        if (ins->op == IR_FUNC_BEGIN)
            clear_var_list(ct->vars);
        break;
    }
}

/* Traverse all instructions applying store/load propagation */
static void process_instructions(ir_builder_t *ir, const_track_t *ct)
{
    for (ir_instr_t *ins = ir->head; ins; ins = ins->next)
        propagate_through_instruction(ct, ins);
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

