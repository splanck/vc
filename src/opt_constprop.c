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

/* Reset all entries in a variable constant list */
static void clear_var_list(var_const_t *head)
{
    for (var_const_t *v = head; v; v = v->next)
        v->known = 0;
}

/* Propagate constants from stores to subsequent loads */
void propagate_load_consts(ir_builder_t *ir)
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

    var_const_t *vars = NULL;

    for (ir_instr_t *ins = ir->head; ins; ins = ins->next) {
        switch (ins->op) {
        case IR_CONST:
            if (ins->dest >= 0 && ins->dest < max_id) {
                is_const[ins->dest] = 1;
                values[ins->dest] = ins->imm;
            }
            break;
        case IR_STORE: {
            var_const_t *v = vars;
            while (v && strcmp(v->name, ins->name) != 0)
                v = v->next;
            if (!v) {
                v = calloc(1, sizeof(*v));
                if (!v) {
                    opt_error("out of memory");
                    break;
                }
                v->name = ins->name;
                v->next = vars;
                vars = v;
            }
            if (!ins->is_volatile && ins->src1 < max_id && is_const[ins->src1]) {
                v->known = 1;
                v->value = values[ins->src1];
            } else {
                v->known = 0;
            }
            break;
        }
        case IR_LOAD: {
            var_const_t *v = vars;
            while (v && strcmp(v->name, ins->name) != 0)
                v = v->next;
            if (!ins->is_volatile && v && v->known) {
                free(ins->name);
                ins->name = NULL;
                ins->op = IR_CONST;
                ins->imm = v->value;
                if (ins->dest >= 0 && ins->dest < max_id) {
                    is_const[ins->dest] = 1;
                    values[ins->dest] = v->value;
                }
            } else if (ins->dest >= 0 && ins->dest < max_id) {
                is_const[ins->dest] = 0;
            }
            break;
        }
        case IR_STORE_PTR:
        case IR_STORE_IDX:
        case IR_CALL:
        case IR_ARG:
            clear_var_list(vars);
            if (ins->dest >= 0 && ins->dest < max_id)
                is_const[ins->dest] = 0;
            break;
        case IR_LOGAND: case IR_LOGOR:
        case IR_LOAD_PARAM:
        case IR_ADDR:
        case IR_LOAD_PTR:
        case IR_LOAD_IDX:
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
        case IR_ADD: case IR_SUB: case IR_MUL: case IR_DIV: case IR_MOD:
        case IR_SHL: case IR_SHR: case IR_AND: case IR_OR: case IR_XOR:
        case IR_FADD: case IR_FSUB: case IR_FMUL: case IR_FDIV:
        case IR_PTR_ADD:
        case IR_PTR_DIFF:
        case IR_CMPEQ: case IR_CMPNE: case IR_CMPLT:
        case IR_CMPGT: case IR_CMPLE: case IR_CMPGE:
            if (ins->dest >= 0 && ins->dest < max_id)
                is_const[ins->dest] = 0;
            if (ins->op == IR_FUNC_BEGIN)
                clear_var_list(vars);
            break;
        }
    }

    free(is_const);
    free(values);
    while (vars) {
        var_const_t *next = vars->next;
        free(vars);
        vars = next;
    }
}

