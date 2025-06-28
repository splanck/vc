/*
 * Intermediate representation builder.
 *
 * The routines in this file append instructions to an ir_builder_t and
 * generate value identifiers used as operands.  Each helper corresponds
 * to a single IR opcode and may allocate strings or other data owned by
 * the builder.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "ir_core.h"
#include "label.h"
#include "strbuf.h"
#include "util.h"
#include "ast.h"


/*
 * Reset the builder so new instructions can be emitted.
 * The first generated value id will be 1.
 */
void ir_builder_init(ir_builder_t *b)
{
    b->head = b->tail = NULL;
    b->next_value_id = 1;
    b->cur_file = "";
    b->cur_line = 0;
    b->cur_column = 0;
}

void ir_builder_set_loc(ir_builder_t *b, const char *file, size_t line, size_t column)
{
    b->cur_file = file ? file : "";
    b->cur_line = line;
    b->cur_column = column;
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
    ins->is_volatile = 0;
    ins->file = b->cur_file;
    ins->line = b->cur_line;
    ins->column = b->cur_column;
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
ir_value_t ir_build_const(ir_builder_t *b, long long value)
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
 * Emit IR_GLOB_WSTRING defining a global wide string literal stored as
 * an array of integers. A unique label is stored in `name`.
 */
ir_value_t ir_build_wstring(ir_builder_t *b, const char *str)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return (ir_value_t){0};
    ins->op = IR_GLOB_WSTRING;
    ins->dest = b->next_value_id++;
    char label[32];
    ins->name = vc_strdup(label_format("LWstr", ins->dest, label));
    size_t len = strlen(str ? str : "");
    long long *vals = malloc((len + 1) * sizeof(long long));
    if (!vals)
        return (ir_value_t){0};
    for (size_t i = 0; i < len; i++)
        vals[i] = (unsigned char)str[i];
    vals[len] = 0;
    ins->imm = (long long)(len + 1);
    ins->data = (char *)vals;
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

ir_value_t ir_build_load_vol(ir_builder_t *b, const char *name)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return (ir_value_t){0};
    ins->op = IR_LOAD;
    ins->dest = b->next_value_id++;
    ins->name = vc_strdup(name ? name : "");
    ins->is_volatile = 1;
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

void ir_build_store_vol(ir_builder_t *b, const char *name, ir_value_t val)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return;
    ins->op = IR_STORE;
    ins->src1 = val.id;
    ins->name = vc_strdup(name ? name : "");
    ins->is_volatile = 1;
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

/* Emit IR_PTR_ADD adding `idx` (scaled by element size) to pointer `ptr`. */
ir_value_t ir_build_ptr_add(ir_builder_t *b, ir_value_t ptr, ir_value_t idx,
                            int elem_size)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return (ir_value_t){0};
    ins->op = IR_PTR_ADD;
    ins->dest = b->next_value_id++;
    ins->src1 = ptr.id;
    ins->src2 = idx.id;
    ins->imm = elem_size;
    return (ir_value_t){ins->dest};
}

/* Emit IR_PTR_DIFF computing the difference between two pointers. */
ir_value_t ir_build_ptr_diff(ir_builder_t *b, ir_value_t a, ir_value_t bptr,
                             int elem_size)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return (ir_value_t){0};
    ins->op = IR_PTR_DIFF;
    ins->dest = b->next_value_id++;
    ins->src1 = a.id;
    ins->src2 = bptr.id;
    ins->imm = elem_size;
    return (ir_value_t){ins->dest};
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

/* Volatile variant of IR_LOAD_IDX for element `name[idx]`. */
ir_value_t ir_build_load_idx_vol(ir_builder_t *b, const char *name, ir_value_t idx)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return (ir_value_t){0};
    ins->op = IR_LOAD_IDX;
    ins->dest = b->next_value_id++;
    ins->src1 = idx.id;
    ins->name = vc_strdup(name ? name : "");
    ins->is_volatile = 1;
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

/* Volatile variant of IR_STORE_IDX assigning `val` to `name[idx]`. */
void ir_build_store_idx_vol(ir_builder_t *b, const char *name, ir_value_t idx,
                            ir_value_t val)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return;
    ins->op = IR_STORE_IDX;
    ins->src1 = idx.id;
    ins->src2 = val.id;
    ins->name = vc_strdup(name ? name : "");
    ins->is_volatile = 1;
}

/* Load a bit-field from `name` shifted by `shift` and masked by `width`. */
ir_value_t ir_build_bfload(ir_builder_t *b, const char *name,
                           int shift, int width)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return (ir_value_t){0};
    ins->op = IR_BFLOAD;
    ins->dest = b->next_value_id++;
    ins->name = vc_strdup(name ? name : "");
    ins->imm = ((long long)shift << 32) | (unsigned)width;
    return (ir_value_t){ins->dest};
}

/* Store `val` into a bit-field at `name`. */
void ir_build_bfstore(ir_builder_t *b, const char *name, int shift,
                      int width, ir_value_t val)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return;
    ins->op = IR_BFSTORE;
    ins->src1 = val.id;
    ins->name = vc_strdup(name ? name : "");
    ins->imm = ((long long)shift << 32) | (unsigned)width;
}

/* Emit IR_ALLOCA reserving stack space of the given size. */
ir_value_t ir_build_alloca(ir_builder_t *b, ir_value_t size)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return (ir_value_t){0};
    ins->op = IR_ALLOCA;
    ins->dest = b->next_value_id++;
    ins->src1 = size.id;
    return (ir_value_t){ins->dest};
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

/* Emit IR_LOGAND performing logical AND. */
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

/* Emit IR_LOGOR performing logical OR. */
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

/* Emit IR_ARG to push an argument value for a call. The argument's
 * type kind is stored in the instruction's imm field for later
 * optimisations or code generation. */
void ir_build_arg(ir_builder_t *b, ir_value_t val, type_kind_t type)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return;
    ins->op = IR_ARG;
    ins->src1 = val.id;
    ins->imm = (long long)type;
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
    ins->imm = (long long)arg_count;
    return (ir_value_t){ins->dest};
}

/* Emit IR_CALL_PTR using function address in `func`. */
ir_value_t ir_build_call_ptr(ir_builder_t *b, ir_value_t func, size_t arg_count)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return (ir_value_t){0};
    ins->op = IR_CALL_PTR;
    ins->dest = b->next_value_id++;
    ins->src1 = func.id;
    ins->imm = (long long)arg_count;
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


