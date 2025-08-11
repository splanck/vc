#include <stdlib.h>
#include <string.h>
#include "ir_control.h"
#include "ir_builder.h"
#include "util.h"

void ir_build_arg(ir_builder_t *b, ir_value_t val, type_kind_t type)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return;
    ins->op = IR_ARG;
    ins->src1 = val.id;
    ins->imm = (long long)type;
}

void ir_build_return(ir_builder_t *b, ir_value_t val, type_kind_t type)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return;
    ins->op = IR_RETURN;
    ins->src1 = val.id;
    ins->type = type;
}

void ir_build_return_agg(ir_builder_t *b, ir_value_t ptr, type_kind_t type)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return;
    ins->op = IR_RETURN_AGG;
    ins->src1 = ptr.id;
    ins->type = type;
}

ir_value_t ir_build_call(ir_builder_t *b, const char *name, size_t arg_count)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return (ir_value_t){0};
    ins->op = IR_CALL;
    ins->dest = alloc_value_id(b);
    ins->name = vc_strdup(name ? name : "");
    if (!ins->name) {
        remove_instr(b, ins);
        return (ir_value_t){0};
    }
    ins->imm = (long long)arg_count;
    return (ir_value_t){ins->dest};
}

ir_value_t ir_build_call_nr(ir_builder_t *b, const char *name, size_t arg_count)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return (ir_value_t){0};
    ins->op = IR_CALL_NR;
    ins->name = vc_strdup(name ? name : "");
    if (!ins->name) {
        remove_instr(b, ins);
        return (ir_value_t){0};
    }
    ins->imm = (long long)arg_count;
    return (ir_value_t){0};
}

ir_value_t ir_build_call_ptr(ir_builder_t *b, ir_value_t func, size_t arg_count)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return (ir_value_t){0};
    ins->op = IR_CALL_PTR;
    ins->dest = alloc_value_id(b);
    ins->src1 = func.id;
    ins->imm = (long long)arg_count;
    return (ir_value_t){ins->dest};
}

ir_value_t ir_build_call_ptr_nr(ir_builder_t *b, ir_value_t func, size_t arg_count)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return (ir_value_t){0};
    ins->op = IR_CALL_PTR_NR;
    ins->src1 = func.id;
    ins->imm = (long long)arg_count;
    return (ir_value_t){0};
}

ir_instr_t *ir_build_func_begin(ir_builder_t *b, const char *name)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return NULL;
    ins->op = IR_FUNC_BEGIN;
    ins->name = vc_strdup(name ? name : "");
    if (!ins->name) {
        remove_instr(b, ins);
        return NULL;
    }
    ins->imm = 0;
    return ins;
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
    ins->name = vc_strdup(label ? label : "");
    if (!ins->name) {
        remove_instr(b, ins);
        return;
    }
}

void ir_build_bcond(ir_builder_t *b, ir_value_t cond, const char *label)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return;
    ins->op = IR_BCOND;
    ins->src1 = cond.id;
    ins->name = vc_strdup(label ? label : "");
    if (!ins->name) {
        remove_instr(b, ins);
        return;
    }
}

void ir_build_label(ir_builder_t *b, const char *label)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return;
    ins->op = IR_LABEL;
    ins->name = vc_strdup(label ? label : "");
    if (!ins->name) {
        remove_instr(b, ins);
        return;
    }
}
