#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "ir.h"

static char *dup_string(const char *s)
{
    size_t len = strlen(s);
    char *out = malloc(len + 1);
    if (!out)
        return NULL;
    memcpy(out, s, len + 1);
    return out;
}

void ir_builder_init(ir_builder_t *b)
{
    b->head = b->tail = NULL;
    b->next_value_id = 1;
}

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

ir_value_t ir_build_string(ir_builder_t *b, const char *str)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return (ir_value_t){0};
    ins->op = IR_GLOB_STRING;
    ins->dest = b->next_value_id++;
    char label[32];
    snprintf(label, sizeof(label), "Lstr%d", ins->dest);
    ins->name = dup_string(label);
    ins->data = dup_string(str ? str : "");
    return (ir_value_t){ins->dest};
}

ir_value_t ir_build_load(ir_builder_t *b, const char *name)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return (ir_value_t){0};
    ins->op = IR_LOAD;
    ins->dest = b->next_value_id++;
    ins->name = dup_string(name ? name : "");
    return (ir_value_t){ins->dest};
}

void ir_build_store(ir_builder_t *b, const char *name, ir_value_t val)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return;
    ins->op = IR_STORE;
    ins->src1 = val.id;
    ins->name = dup_string(name ? name : "");
}

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

void ir_build_store_param(ir_builder_t *b, int index, ir_value_t val)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return;
    ins->op = IR_STORE_PARAM;
    ins->imm = index;
    ins->src1 = val.id;
}

ir_value_t ir_build_addr(ir_builder_t *b, const char *name)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return (ir_value_t){0};
    ins->op = IR_ADDR;
    ins->dest = b->next_value_id++;
    ins->name = dup_string(name ? name : "");
    return (ir_value_t){ins->dest};
}

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

void ir_build_store_ptr(ir_builder_t *b, ir_value_t addr, ir_value_t val)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return;
    ins->op = IR_STORE_PTR;
    ins->src1 = addr.id;
    ins->src2 = val.id;
}

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

void ir_build_return(ir_builder_t *b, ir_value_t val)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return;
    ins->op = IR_RETURN;
    ins->src1 = val.id;
}

ir_value_t ir_build_call(ir_builder_t *b, const char *name)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return (ir_value_t){0};
    ins->op = IR_CALL;
    ins->dest = b->next_value_id++;
    ins->name = dup_string(name ? name : "");
    return (ir_value_t){ins->dest};
}

void ir_build_func_begin(ir_builder_t *b, const char *name)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return;
    ins->op = IR_FUNC_BEGIN;
    ins->name = dup_string(name ? name : "");
}

void ir_build_func_end(ir_builder_t *b)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return;
    ins->op = IR_FUNC_END;
}

void ir_build_br(ir_builder_t *b, const char *label)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return;
    ins->op = IR_BR;
    ins->name = dup_string(label ? label : "");
}

void ir_build_bcond(ir_builder_t *b, ir_value_t cond, const char *label)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return;
    ins->op = IR_BCOND;
    ins->src1 = cond.id;
    ins->name = dup_string(label ? label : "");
}

void ir_build_label(ir_builder_t *b, const char *label)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return;
    ins->op = IR_LABEL;
    ins->name = dup_string(label ? label : "");
}

void ir_build_glob_var(ir_builder_t *b, const char *name)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return;
    ins->op = IR_GLOB_VAR;
    ins->name = dup_string(name ? name : "");
}

