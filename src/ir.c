/*
 * Intermediate representation builder.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include "ir.h"
#include "label.h"
#include "strbuf.h"
#include "util.h"


/*
 * Reset the builder so new instructions can be emitted.
 * The first generated value id will be 1.
 */
void ir_builder_init(ir_builder_t *b)
{
    b->head = b->tail = NULL;
    b->next_value_id = 1;
}

/* Free all instructions owned by the builder. */
void ir_builder_free(ir_builder_t *b)
{
    ir_instr_t *ins = b->head;
    while (ins) {
        ir_instr_t *next = ins->next;
        free(ins->name);
        free(ins->data);
        free(ins);
        ins = next;
    }
    b->head = b->tail = NULL;
    b->next_value_id = 0;
}

/* Allocate and append a blank instruction to the builder's list. */
static ir_instr_t *append_instr(ir_builder_t *b)
{
    ir_instr_t *ins = calloc(1, sizeof(*ins));
    if (!ins)
        return NULL;
    ins->dest = -1;
    ins->name = NULL;
    ins->data = NULL;
    if (!b->head)
        b->head = ins;
    else
        b->tail->next = ins;
    b->tail = ins;
    return ins;
}

/*
 * Emit IR_CONST. dest gets a fresh id and imm stores the constant
 * value.
 */
ir_value_t ir_build_const(ir_builder_t *b, int value)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return (ir_value_t){0};
    ins->op = IR_CONST;
    ins->dest = b->next_value_id++;
    ins->imm = value;
    return (ir_value_t){ins->dest};
}

/*
 * Emit IR_GLOB_STRING defining a global string literal. A unique
 * label is stored in `name` and the literal text in `data`.
 */
ir_value_t ir_build_string(ir_builder_t *b, const char *str)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return (ir_value_t){0};
    ins->op = IR_GLOB_STRING;
    ins->dest = b->next_value_id++;
    char label[32];
    ins->name = vc_strdup(label_format("Lstr", ins->dest, label));
    ins->data = vc_strdup(str ? str : "");
    return (ir_value_t){ins->dest};
}

/*
 * Emit IR_LOAD for variable `name`. The loaded value id is returned
 * as the destination of the instruction.
 */
ir_value_t ir_build_load(ir_builder_t *b, const char *name)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return (ir_value_t){0};
    ins->op = IR_LOAD;
    ins->dest = b->next_value_id++;
    ins->name = vc_strdup(name ? name : "");
    return (ir_value_t){ins->dest};
}

/*
 * Emit IR_STORE to assign `val` to variable `name`. src1 holds the
 * value identifier.
 */
void ir_build_store(ir_builder_t *b, const char *name, ir_value_t val)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return;
    ins->op = IR_STORE;
    ins->src1 = val.id;
    ins->name = vc_strdup(name ? name : "");
}

/*
 * Emit IR_LOAD_PARAM reading parameter `index` into a new value.
 */
ir_value_t ir_build_load_param(ir_builder_t *b, int index)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return (ir_value_t){0};
    ins->op = IR_LOAD_PARAM;
    ins->dest = b->next_value_id++;
    ins->imm = index;
    return (ir_value_t){ins->dest};
}

/*
 * Emit IR_STORE_PARAM storing `val` into parameter `index`.
 */
void ir_build_store_param(ir_builder_t *b, int index, ir_value_t val)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return;
    ins->op = IR_STORE_PARAM;
    ins->imm = index;
    ins->src1 = val.id;
}

/*
 * Emit IR_ADDR producing the address of variable `name`.
 */
ir_value_t ir_build_addr(ir_builder_t *b, const char *name)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return (ir_value_t){0};
    ins->op = IR_ADDR;
    ins->dest = b->next_value_id++;
    ins->name = vc_strdup(name ? name : "");
    return (ir_value_t){ins->dest};
}

/*
 * Emit IR_LOAD_PTR loading from the pointer address `addr`.
 */
ir_value_t ir_build_load_ptr(ir_builder_t *b, ir_value_t addr)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return (ir_value_t){0};
    ins->op = IR_LOAD_PTR;
    ins->dest = b->next_value_id++;
    ins->src1 = addr.id;
    return (ir_value_t){ins->dest};
}

/*
 * Emit IR_STORE_PTR storing `val` through pointer `addr`.
 */
void ir_build_store_ptr(ir_builder_t *b, ir_value_t addr, ir_value_t val)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return;
    ins->op = IR_STORE_PTR;
    ins->src1 = addr.id;
    ins->src2 = val.id;
}

/*
 * Emit IR_LOAD_IDX loading from array element `name[idx]`.
 */
ir_value_t ir_build_load_idx(ir_builder_t *b, const char *name, ir_value_t idx)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return (ir_value_t){0};
    ins->op = IR_LOAD_IDX;
    ins->dest = b->next_value_id++;
    ins->src1 = idx.id;
    ins->name = vc_strdup(name ? name : "");
    return (ir_value_t){ins->dest};
}

/*
 * Emit IR_STORE_IDX storing `val` into array element `name[idx]`.
 */
void ir_build_store_idx(ir_builder_t *b, const char *name, ir_value_t idx,
                        ir_value_t val)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return;
    ins->op = IR_STORE_IDX;
    ins->src1 = idx.id;
    ins->src2 = val.id;
    ins->name = vc_strdup(name ? name : "");
}

/*
 * Emit a binary arithmetic or comparison instruction. Operands are in
 * src1 and src2 and a new destination value id is returned.
 */
ir_value_t ir_build_binop(ir_builder_t *b, ir_op_t op, ir_value_t left, ir_value_t right)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return (ir_value_t){0};
    ins->op = op;
    ins->dest = b->next_value_id++;
    ins->src1 = left.id;
    ins->src2 = right.id;
    return (ir_value_t){ins->dest};
}

ir_value_t ir_build_logand(ir_builder_t *b, ir_value_t left, ir_value_t right)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return (ir_value_t){0};
    ins->op = IR_LOGAND;
    ins->dest = b->next_value_id++;
    ins->src1 = left.id;
    ins->src2 = right.id;
    return (ir_value_t){ins->dest};
}

ir_value_t ir_build_logor(ir_builder_t *b, ir_value_t left, ir_value_t right)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return (ir_value_t){0};
    ins->op = IR_LOGOR;
    ins->dest = b->next_value_id++;
    ins->src1 = left.id;
    ins->src2 = right.id;
    return (ir_value_t){ins->dest};
}

/* Emit IR_ARG to push an argument value for a call. */
void ir_build_arg(ir_builder_t *b, ir_value_t val)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return;
    ins->op = IR_ARG;
    ins->src1 = val.id;
}

/* Emit IR_RETURN using the supplied value id. */
void ir_build_return(ir_builder_t *b, ir_value_t val)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return;
    ins->op = IR_RETURN;
    ins->src1 = val.id;
}

/*
 * Emit IR_CALL to `name`. The number of arguments previously pushed by
 * IR_ARG instructions is stored in imm.
 */
ir_value_t ir_build_call(ir_builder_t *b, const char *name, size_t arg_count)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return (ir_value_t){0};
    ins->op = IR_CALL;
    ins->dest = b->next_value_id++;
    ins->name = vc_strdup(name ? name : "");
    ins->imm = (int)arg_count;
    return (ir_value_t){ins->dest};
}

/* Begin a function with the given name. */
void ir_build_func_begin(ir_builder_t *b, const char *name)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return;
    ins->op = IR_FUNC_BEGIN;
    ins->name = vc_strdup(name ? name : "");
}

/* End the current function. */
void ir_build_func_end(ir_builder_t *b)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return;
    ins->op = IR_FUNC_END;
}

/* Emit IR_BR jumping unconditionally to `label`. */
void ir_build_br(ir_builder_t *b, const char *label)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return;
    ins->op = IR_BR;
    ins->name = vc_strdup(label ? label : "");
}

/*
 * Emit IR_BCOND. The branch target is `label` and src1 holds the
 * condition value id.
 */
void ir_build_bcond(ir_builder_t *b, ir_value_t cond, const char *label)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return;
    ins->op = IR_BCOND;
    ins->src1 = cond.id;
    ins->name = vc_strdup(label ? label : "");
}

/* Emit IR_LABEL marking a location in the instruction stream. */
void ir_build_label(ir_builder_t *b, const char *label)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return;
    ins->op = IR_LABEL;
    ins->name = vc_strdup(label ? label : "");
}

/*
 * Emit IR_GLOB_VAR declaring global variable `name` with constant
 * initializer `value`.
 */
void ir_build_glob_var(ir_builder_t *b, const char *name, int value, int is_static)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return;
    ins->op = IR_GLOB_VAR;
    ins->name = vc_strdup(name ? name : "");
    ins->imm = value;
    ins->src1 = is_static;
}

void ir_build_glob_array(ir_builder_t *b, const char *name,
                         const int *values, size_t count, int is_static)
{
    /* Emit IR_GLOB_ARRAY storing an array of constants. `data` points
     * to a copy of the initializer values. */
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return;
    ins->op = IR_GLOB_ARRAY;
    ins->name = vc_strdup(name ? name : "");
    ins->imm = (int)count;
    ins->src1 = is_static;
    if (count) {
        int *vals = malloc(count * sizeof(int));
        if (!vals)
            return;
        for (size_t i = 0; i < count; i++)
            vals[i] = values[i];
        ins->data = (char *)vals;
    }
}

