#include <stdlib.h>
#include "opt.h"
#include <string.h>

typedef struct var_const {
    const char *name;
    int value;
    int known;
    struct var_const *next;
} var_const_t;

static void clear_var_list(var_const_t *head)
{
    for (var_const_t *v = head; v; v = v->next)
        v->known = 0;
}

/* Propagate constants through store/load pairs */
static void propagate_load_consts(ir_builder_t *ir)
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
                if (!v)
                    break;
                v->name = ins->name;
                v->next = vars;
                vars = v;
            }
            if (ins->src1 < max_id && is_const[ins->src1]) {
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
            if (v && v->known) {
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
        case IR_BR:
        case IR_BCOND:
        case IR_LABEL:
        case IR_ADD: case IR_SUB: case IR_MUL: case IR_DIV:
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
        case IR_CMPEQ: case IR_CMPNE: case IR_CMPLT:
        case IR_CMPGT: case IR_CMPLE: case IR_CMPGE:
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
                case IR_CMPEQ: result = a == b; break;
                case IR_CMPNE: result = a != b; break;
                case IR_CMPLT: result = a < b; break;
                case IR_CMPGT: result = a > b; break;
                case IR_CMPLE: result = a <= b; break;
                case IR_CMPGE: result = a >= b; break;
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
            if (ins->dest >= 0 && ins->dest < max_id)
                is_const[ins->dest] = 0;
            break;
        case IR_RETURN:
            /* nothing to do */
            break;
        case IR_GLOB_STRING:
        case IR_GLOB_VAR:
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


/* Instructions that must be preserved even if their result is unused */
static int has_side_effect(ir_instr_t *ins)
{
    switch (ins->op) {
    case IR_STORE:
    case IR_STORE_PTR:
    case IR_STORE_IDX:
    case IR_STORE_PARAM:
    case IR_CALL:
    case IR_ARG:
    case IR_RETURN:
    case IR_BR:
    case IR_BCOND:
    case IR_FUNC_BEGIN:
    case IR_FUNC_END:
    case IR_LABEL:
    case IR_GLOB_VAR:
        return 1;
    default:
        return 0;
    }
}

/* Remove instructions whose results are never used */
static void dead_code_elim(ir_builder_t *ir)
{
    if (!ir)
        return;

    int max_id = ir->next_value_id;
    int count = 0;
    for (ir_instr_t *i = ir->head; i; i = i->next)
        count++;

    ir_instr_t **list = malloc((size_t)count * sizeof(*list));
    if (!list)
        return;

    int idx = 0;
    for (ir_instr_t *i = ir->head; i; i = i->next)
        list[idx++] = i;

    int *used = calloc((size_t)max_id, sizeof(int));
    if (!used) {
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

        if (ins->src1 >= 0 && ins->src1 < max_id)
            used[ins->src1] = 1;
        if (ins->src2 >= 0 && ins->src2 < max_id)
            used[ins->src2] = 1;
    }

    free(used);
    free(list);
}

void opt_run(ir_builder_t *ir, const opt_config_t *cfg)
{
    opt_config_t def = {1, 1, 1};
    const opt_config_t *c = cfg ? cfg : &def;
    if (c->const_prop)
        propagate_load_consts(ir);
    if (c->fold_constants)
        fold_constants(ir);
    if (c->dead_code)
        dead_code_elim(ir);
}

