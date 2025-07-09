#include <stdlib.h>
#include <string.h>
#include "ir_memory.h"
#include "ir_builder.h"
#include "util.h"
#include "error.h"

ir_value_t ir_build_load(ir_builder_t *b, const char *name)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return (ir_value_t){0};
    ins->op = IR_LOAD;
    ins->dest = alloc_value_id(b);
    ins->name = vc_strdup(name ? name : "");
    if (!ins->name) {
        remove_instr(b, ins);
        return (ir_value_t){0};
    }
    if (name)
        ins->alias_set = get_alias(b, name);
    return (ir_value_t){ins->dest};
}

ir_value_t ir_build_load_vol(ir_builder_t *b, const char *name)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return (ir_value_t){0};
    ins->op = IR_LOAD;
    ins->dest = alloc_value_id(b);
    ins->name = vc_strdup(name ? name : "");
    if (!ins->name) {
        remove_instr(b, ins);
        return (ir_value_t){0};
    }
    ins->is_volatile = 1;
    if (name)
        ins->alias_set = get_alias(b, name);
    return (ir_value_t){ins->dest};
}

void ir_build_store(ir_builder_t *b, const char *name, ir_value_t val)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return;
    ins->op = IR_STORE;
    ins->src1 = val.id;
    ins->name = vc_strdup(name ? name : "");
    if (!ins->name) {
        remove_instr(b, ins);
        return;
    }
    if (name)
        ins->alias_set = get_alias(b, name);
}

void ir_build_store_vol(ir_builder_t *b, const char *name, ir_value_t val)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return;
    ins->op = IR_STORE;
    ins->src1 = val.id;
    ins->name = vc_strdup(name ? name : "");
    if (!ins->name) {
        remove_instr(b, ins);
        return;
    }
    ins->is_volatile = 1;
    if (name)
        ins->alias_set = get_alias(b, name);
}

ir_value_t ir_build_load_param(ir_builder_t *b, int index)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return (ir_value_t){0};
    ins->op = IR_LOAD_PARAM;
    ins->dest = alloc_value_id(b);
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
    ins->dest = alloc_value_id(b);
    ins->name = vc_strdup(name ? name : "");
    if (!ins->name) {
        remove_instr(b, ins);
        return (ir_value_t){0};
    }
    return (ir_value_t){ins->dest};
}

ir_value_t ir_build_load_ptr(ir_builder_t *b, ir_value_t addr)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return (ir_value_t){0};
    ins->op = IR_LOAD_PTR;
    ins->dest = alloc_value_id(b);
    ins->src1 = addr.id;
    return (ir_value_t){ins->dest};
}

ir_value_t ir_build_load_ptr_res(ir_builder_t *b, ir_value_t addr)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return (ir_value_t){0};
    ins->op = IR_LOAD_PTR;
    ins->dest = alloc_value_id(b);
    ins->src1 = addr.id;
    ins->is_restrict = 1;
    ins->alias_set = b->next_alias_id++;
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

void ir_build_store_ptr_res(ir_builder_t *b, ir_value_t addr, ir_value_t val)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return;
    ins->op = IR_STORE_PTR;
    ins->src1 = addr.id;
    ins->src2 = val.id;
    ins->is_restrict = 1;
    ins->alias_set = b->next_alias_id++;
}

ir_value_t ir_build_ptr_add(ir_builder_t *b, ir_value_t ptr, ir_value_t idx,
                            int elem_size)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return (ir_value_t){0};
    ins->op = IR_PTR_ADD;
    ins->dest = alloc_value_id(b);
    ins->src1 = ptr.id;
    ins->src2 = idx.id;
    ins->imm = elem_size;
    return (ir_value_t){ins->dest};
}

ir_value_t ir_build_ptr_diff(ir_builder_t *b, ir_value_t a, ir_value_t bptr,
                             int elem_size)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return (ir_value_t){0};
    ins->op = IR_PTR_DIFF;
    ins->dest = alloc_value_id(b);
    ins->src1 = a.id;
    ins->src2 = bptr.id;
    ins->imm = elem_size;
    return (ir_value_t){ins->dest};
}

ir_value_t ir_build_load_idx(ir_builder_t *b, const char *name, ir_value_t idx)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return (ir_value_t){0};
    ins->op = IR_LOAD_IDX;
    ins->dest = alloc_value_id(b);
    ins->src1 = idx.id;
    ins->name = vc_strdup(name ? name : "");
    if (!ins->name) {
        remove_instr(b, ins);
        return (ir_value_t){0};
    }
    if (name)
        ins->alias_set = get_alias(b, name);
    return (ir_value_t){ins->dest};
}

ir_value_t ir_build_load_idx_vol(ir_builder_t *b, const char *name, ir_value_t idx)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return (ir_value_t){0};
    ins->op = IR_LOAD_IDX;
    ins->dest = alloc_value_id(b);
    ins->src1 = idx.id;
    ins->name = vc_strdup(name ? name : "");
    if (!ins->name) {
        remove_instr(b, ins);
        return (ir_value_t){0};
    }
    ins->is_volatile = 1;
    if (name)
        ins->alias_set = get_alias(b, name);
    return (ir_value_t){ins->dest};
}

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
    if (!ins->name) {
        remove_instr(b, ins);
        return;
    }
    if (name)
        ins->alias_set = get_alias(b, name);
}

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
    if (!ins->name) {
        remove_instr(b, ins);
        return;
    }
    ins->is_volatile = 1;
    if (name)
        ins->alias_set = get_alias(b, name);
}

ir_value_t ir_build_alloca(ir_builder_t *b, ir_value_t size)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return (ir_value_t){0};
    ins->op = IR_ALLOCA;
    ins->dest = alloc_value_id(b);
    ins->src1 = size.id;
    return (ir_value_t){ins->dest};
}
