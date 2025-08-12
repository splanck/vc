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
#include <limits.h>
#include <stdint.h>
#include "ir_core.h"
#include "ir_builder.h"
#include "label.h"
#include "strbuf.h"
#include "util.h"
#include "ast.h"
#include "error.h"

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
    b->aliases = NULL;
    b->next_alias_id = 1;
    vector_init(&b->locals, sizeof(ir_local_t));
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
    while (b->aliases) {
        alias_ent_t *n = b->aliases->next;
        free((char *)b->aliases->name);
        free(b->aliases);
        b->aliases = n;
    }
    b->next_alias_id = 0;
    for (size_t i = 0; i < b->locals.count; i++)
        free(((ir_local_t *)b->locals.data)[i].name);
    vector_free(&b->locals);
}

void ir_builder_add_local(ir_builder_t *b, const char *name, size_t size)
{
    if (!b || !name || !size)
        return;
    for (size_t i = 0; i < b->locals.count; i++) {
        ir_local_t *loc = &((ir_local_t *)b->locals.data)[i];
        if (strcmp(loc->name, name) == 0) {
            loc->size = size;
            return;
        }
    }
    ir_local_t loc;
    loc.name = vc_strdup(name);
    if (!loc.name)
        return;
    loc.size = size;
    vector_push(&b->locals, &loc);
}
/*
 * Emit a binary arithmetic or comparison instruction. Operands are in
 * src1 and src2 and a new destination value id is returned.
 */
ir_value_t ir_build_binop(ir_builder_t *b, ir_op_t op, ir_value_t left, ir_value_t right,
                          type_kind_t type)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return (ir_value_t){0};
    ins->op = op;
    ins->dest = alloc_value_id(b);
    ins->src1 = left.id;
    ins->src2 = right.id;
    ins->type = type;
    return (ir_value_t){ins->dest};
}

ir_value_t ir_build_cplx_add(ir_builder_t *b, ir_value_t left, ir_value_t right,
                             type_kind_t type)
{
    return ir_build_binop(b, IR_CPLX_ADD, left, right, type);
}

ir_value_t ir_build_cplx_sub(ir_builder_t *b, ir_value_t left, ir_value_t right,
                             type_kind_t type)
{
    return ir_build_binop(b, IR_CPLX_SUB, left, right, type);
}

ir_value_t ir_build_cplx_mul(ir_builder_t *b, ir_value_t left, ir_value_t right,
                             type_kind_t type)
{
    return ir_build_binop(b, IR_CPLX_MUL, left, right, type);
}

ir_value_t ir_build_cplx_div(ir_builder_t *b, ir_value_t left, ir_value_t right,
                             type_kind_t type)
{
    return ir_build_binop(b, IR_CPLX_DIV, left, right, type);
}

/* Emit IR_LOGAND performing logical AND. */
ir_value_t ir_build_logand(ir_builder_t *b, ir_value_t left, ir_value_t right)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return (ir_value_t){0};
    ins->op = IR_LOGAND;
    ins->dest = alloc_value_id(b);
    ins->src1 = left.id;
    ins->src2 = right.id;
    ins->type = TYPE_INT;
    return (ir_value_t){ins->dest};
}

/* Emit IR_LOGOR performing logical OR. */
ir_value_t ir_build_logor(ir_builder_t *b, ir_value_t left, ir_value_t right)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return (ir_value_t){0};
    ins->op = IR_LOGOR;
    ins->dest = alloc_value_id(b);
    ins->src1 = left.id;
    ins->src2 = right.id;
    ins->type = TYPE_INT;
    return (ir_value_t){ins->dest};
}

/* Emit IR_CAST converting between primitive types. */
ir_value_t ir_build_cast(ir_builder_t *b, ir_value_t val,
                         type_kind_t src_type, type_kind_t dst_type)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return (ir_value_t){0};
    ins->op = IR_CAST;
    ins->dest = alloc_value_id(b);
    ins->src1 = val.id;
    ins->imm = ((long long)src_type << 32) | (unsigned long long)(unsigned)dst_type;
    return (ir_value_t){ins->dest};
}

