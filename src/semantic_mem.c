/*
 * Memory expression semantic helpers.
 * These routines handle array and struct member access and
 * emit loads, stores and address computations in IR.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "semantic_mem.h"
#include "semantic_expr.h"
#include "consteval.h"
#include "symtable.h"
#include "semantic.h"
#include "util.h"
#include "label.h"
#include "error.h"
#include <limits.h>

/*
 * Search for a member within a struct or union symbol.  Both struct
 * and union member lists are scanned for a matching name and the
 * member's type and offset are returned.  The function returns 1 on
 * success and 0 if the member is not found.
 */
static int find_member(symbol_t *sym, const char *name, type_kind_t *type,
                       size_t *offset, unsigned *bit_width,
                       unsigned *bit_offset)
{
    if (!sym)
        return 0;
    if (sym->type == TYPE_UNION) {
        for (size_t i = 0; i < sym->member_count; i++) {
            if (strcmp(sym->members[i].name, name) == 0) {
                if (type)
                    *type = sym->members[i].type;
                if (offset)
                    *offset = sym->members[i].offset;
                if (bit_width)
                    *bit_width = sym->members[i].bit_width;
                if (bit_offset)
                    *bit_offset = sym->members[i].bit_offset;
                return 1;
            }
        }
    } else if (sym->type == TYPE_STRUCT) {
        for (size_t i = 0; i < sym->struct_member_count; i++) {
            if (strcmp(sym->struct_members[i].name, name) == 0) {
                if (type)
                    *type = sym->struct_members[i].type;
                if (offset)
                    *offset = sym->struct_members[i].offset;
                if (bit_width)
                    *bit_width = sym->struct_members[i].bit_width;
                if (bit_offset)
                    *bit_offset = sym->struct_members[i].bit_offset;
                return 1;
            }
        }
    }
    return 0;
}

/*
 * Validate array indexing and emit a load from the computed element
 * address in the IR.
 */
type_kind_t check_index_expr(expr_t *expr, symtable_t *vars,
                             symtable_t *funcs, ir_builder_t *ir,
                             ir_value_t *out)
{
    (void)funcs;
    if (expr->data.index.array->kind != EXPR_IDENT) {
        error_set(expr->line, expr->column, error_current_file, error_current_function);
        return TYPE_UNKNOWN;
    }
    symbol_t *sym = symtable_lookup(vars, expr->data.index.array->data.ident.name);
    if (!sym || sym->type != TYPE_ARRAY) {
        error_set(expr->line, expr->column, error_current_file, error_current_function);
        return TYPE_UNKNOWN;
    }
    ir_value_t idx_val;
    if (check_expr(expr->data.index.index, vars, funcs, ir, &idx_val) != TYPE_INT) {
        error_set(expr->data.index.index->line, expr->data.index.index->column, error_current_file, error_current_function);
        return TYPE_UNKNOWN;
    }
    long long cval;
    if (sym->array_size &&
        eval_const_expr(expr->data.index.index, vars,
                        semantic_get_x86_64(), &cval)) {
        if (cval < 0 || (size_t)cval >= sym->array_size) {
            error_set(expr->data.index.index->line, expr->data.index.index->column, error_current_file, error_current_function);
            return TYPE_UNKNOWN;
        }
    }
    if (out) {
        if (sym->vla_addr.id) {
            int esz = sym->elem_size ? (int)sym->elem_size : 4;
            ir_value_t addr = ir_build_ptr_add(ir, sym->vla_addr, idx_val, esz);
            *out = ir_build_load_ptr(ir, addr);
        } else {
            *out = sym->is_volatile
                     ? ir_build_load_idx_vol(ir, sym->ir_name, idx_val)
                     : ir_build_load_idx(ir, sym->ir_name, idx_val);
        }
    }
    return TYPE_INT;
}

/*
 * Validate assignment through an array index and emit a store to the
 * appropriate element in the IR.
 */
type_kind_t check_assign_index_expr(expr_t *expr, symtable_t *vars,
                                    symtable_t *funcs, ir_builder_t *ir,
                                    ir_value_t *out)
{
    (void)funcs;
    if (expr->data.assign_index.array->kind != EXPR_IDENT) {
        error_set(expr->line, expr->column, error_current_file, error_current_function);
        return TYPE_UNKNOWN;
    }
    symbol_t *sym = symtable_lookup(vars, expr->data.assign_index.array->data.ident.name);
    if (!sym || sym->type != TYPE_ARRAY) {
        error_set(expr->line, expr->column, error_current_file, error_current_function);
        return TYPE_UNKNOWN;
    }
    if (sym->is_const) {
        error_set(expr->line, expr->column, error_current_file, error_current_function);
        return TYPE_UNKNOWN;
    }

    ir_value_t idx_val, val;
    if (check_expr(expr->data.assign_index.index, vars, funcs, ir, &idx_val) != TYPE_INT) {
        error_set(expr->data.assign_index.index->line, expr->data.assign_index.index->column, error_current_file, error_current_function);
        return TYPE_UNKNOWN;
    }
    if (check_expr(expr->data.assign_index.value, vars, funcs, ir, &val) != TYPE_INT) {
        error_set(expr->data.assign_index.value->line, expr->data.assign_index.value->column, error_current_file, error_current_function);
        return TYPE_UNKNOWN;
    }
    long long cval;
    if (sym->array_size &&
        eval_const_expr(expr->data.assign_index.index, vars,
                        semantic_get_x86_64(), &cval)) {
        if (cval < 0 || (size_t)cval >= sym->array_size) {
            error_set(expr->data.assign_index.index->line, expr->data.assign_index.index->column, error_current_file, error_current_function);
            return TYPE_UNKNOWN;
        }
    }
    if (sym->vla_addr.id) {
        int esz = sym->elem_size ? (int)sym->elem_size : 4;
        ir_value_t addr = ir_build_ptr_add(ir, sym->vla_addr, idx_val, esz);
        ir_build_store_ptr(ir, addr, val);
    } else if (sym->is_volatile) {
        ir_build_store_idx_vol(ir, sym->ir_name, idx_val, val);
    } else {
        ir_build_store_idx(ir, sym->ir_name, idx_val, val);
    }
    if (out)
        *out = val;
    return TYPE_INT;
}

/*
 * Validate assignment to a struct or union member and generate a store
 * to the field's computed address in the IR.
 */
type_kind_t check_assign_member_expr(expr_t *expr, symtable_t *vars,
                                     symtable_t *funcs, ir_builder_t *ir,
                                     ir_value_t *out)
{
    symbol_t *obj_sym = NULL;
    ir_value_t base_addr;
    if (expr->data.assign_member.via_ptr) {
        if (check_expr(expr->data.assign_member.object, vars, funcs, ir,
                       &base_addr) != TYPE_PTR) {
            error_set(expr->data.assign_member.object->line, expr->data.assign_member.object->column, error_current_file, error_current_function);
            return TYPE_UNKNOWN;
        }
        if (expr->data.assign_member.object->kind == EXPR_IDENT)
            obj_sym = symtable_lookup(vars,
                                      expr->data.assign_member.object->data.ident.name);
    } else {
        if (expr->data.assign_member.object->kind != EXPR_IDENT) {
            error_set(expr->data.assign_member.object->line, expr->data.assign_member.object->column, error_current_file, error_current_function);
            return TYPE_UNKNOWN;
        }
        obj_sym = symtable_lookup(vars, expr->data.assign_member.object->data.ident.name);
        if (!obj_sym || (obj_sym->type != TYPE_UNION && obj_sym->type != TYPE_STRUCT)) {
            error_set(expr->data.assign_member.object->line, expr->data.assign_member.object->column, error_current_file, error_current_function);
            return TYPE_UNKNOWN;
        }
        base_addr = ir_build_addr(ir, obj_sym->ir_name);
    }

    if (!obj_sym ||
        ((obj_sym->type == TYPE_UNION && obj_sym->member_count == 0) ||
         (obj_sym->type == TYPE_STRUCT && obj_sym->struct_member_count == 0))) {
        error_set(expr->line, expr->column, error_current_file, error_current_function);
        return TYPE_UNKNOWN;
    }

    type_kind_t mtype = TYPE_UNKNOWN;
    size_t moff = 0;
    unsigned mbw = 0, mbo = 0;
    if (!find_member(obj_sym, expr->data.assign_member.member, &mtype, &moff,
                     &mbw, &mbo)) {
        error_set(expr->line, expr->column, error_current_file, error_current_function);
        return TYPE_UNKNOWN;
    }

    ir_value_t val;
    type_kind_t vt = check_expr(expr->data.assign_member.value, vars, funcs, ir, &val);
    if (mbw > 0) {
        if (!is_intlike(vt)) {
            error_set(expr->data.assign_member.value->line, expr->data.assign_member.value->column,
                      error_current_file, error_current_function);
            return TYPE_UNKNOWN;
        }
    } else if (!(((is_intlike(mtype) && is_intlike(vt)) ||
                  (is_floatlike(mtype) && (is_floatlike(vt) || is_intlike(vt)))) ||
                 vt == mtype)) {
        error_set(expr->data.assign_member.value->line, expr->data.assign_member.value->column, error_current_file, error_current_function);
        return TYPE_UNKNOWN;
    }

    ir_value_t idx = ir_build_const(ir, (int)moff);
    ir_value_t addr = ir_build_ptr_add(ir, base_addr, idx, 1);
    int restr = 0;
    if (expr->data.assign_member.via_ptr &&
        expr->data.assign_member.object->kind == EXPR_IDENT) {
        symbol_t *rsym = symtable_lookup(vars,
                                         expr->data.assign_member.object->data.ident.name);
        restr = rsym ? rsym->is_restrict : 0;
    }
    if (mbw > 0) {
        ir_value_t word = restr ? ir_build_load_ptr_res(ir, addr)
                               : ir_build_load_ptr(ir, addr);
        unsigned mask_bits = (mbw == 32) ? 0xFFFFFFFFu : ((1u << mbw) - 1u);
        ir_value_t maskv = ir_build_const(ir, (int)mask_bits);
        ir_value_t clear_mask = ir_build_const(ir, ~(int)(mask_bits << mbo));
        word = ir_build_binop(ir, IR_AND, word, clear_mask);
        ir_value_t new_val = ir_build_binop(ir, IR_AND, val, maskv);
        if (mbo)
            new_val = ir_build_binop(ir, IR_SHL, new_val, ir_build_const(ir, (int)mbo));
        word = ir_build_binop(ir, IR_OR, word, new_val);
        if (restr)
            ir_build_store_ptr_res(ir, addr, word);
        else
            ir_build_store_ptr(ir, addr, word);
        if (out)
            *out = val;
        if (!expr->data.assign_member.via_ptr && obj_sym && obj_sym->type == TYPE_UNION) {
            free(obj_sym->active_member);
            obj_sym->active_member = vc_strdup(expr->data.assign_member.member);
        }
        return TYPE_INT;
    } else {
        if (restr)
            ir_build_store_ptr_res(ir, addr, val);
        else
            ir_build_store_ptr(ir, addr, val);
        if (out)
            *out = val;
        if (!expr->data.assign_member.via_ptr && obj_sym && obj_sym->type == TYPE_UNION) {
            free(obj_sym->active_member);
            obj_sym->active_member = vc_strdup(expr->data.assign_member.member);
        }
        return mtype;
    }
}

/*
 * Validate member access on a struct, union or pointer and emit a load
 * from the member's address.
 */
type_kind_t check_member_expr(expr_t *expr, symtable_t *vars,
                               symtable_t *funcs, ir_builder_t *ir,
                               ir_value_t *out)
{
    if (!expr->data.member.object)
        return TYPE_UNKNOWN;
    symbol_t *obj_sym = NULL;
    ir_value_t base_addr;
    if (expr->data.member.via_ptr) {
        if (check_expr(expr->data.member.object, vars, funcs, ir,
                       &base_addr) != TYPE_PTR) {
            error_set(expr->data.member.object->line, expr->data.member.object->column, error_current_file, error_current_function);
            return TYPE_UNKNOWN;
        }
        if (expr->data.member.object->kind == EXPR_IDENT)
            obj_sym = symtable_lookup(vars,
                                      expr->data.member.object->data.ident.name);
    } else {
        if (expr->data.member.object->kind != EXPR_IDENT) {
            error_set(expr->data.member.object->line, expr->data.member.object->column, error_current_file, error_current_function);
            return TYPE_UNKNOWN;
        }
        obj_sym = symtable_lookup(vars, expr->data.member.object->data.ident.name);
        if (!obj_sym || (obj_sym->type != TYPE_UNION && obj_sym->type != TYPE_STRUCT)) {
            error_set(expr->data.member.object->line, expr->data.member.object->column, error_current_file, error_current_function);
            return TYPE_UNKNOWN;
        }
        base_addr = ir_build_addr(ir, obj_sym->ir_name);
    }

    if (!obj_sym ||
        ((obj_sym->type == TYPE_UNION && obj_sym->member_count == 0) ||
         (obj_sym->type == TYPE_STRUCT && obj_sym->struct_member_count == 0))) {
        error_set(expr->line, expr->column, error_current_file, error_current_function);
        return TYPE_UNKNOWN;
    }

    type_kind_t mtype = TYPE_UNKNOWN;
    size_t moff = 0;
    unsigned mbw = 0, mbo = 0;
    if (!find_member(obj_sym, expr->data.member.member, &mtype, &moff,
                     &mbw, &mbo)) {
        error_set(expr->line, expr->column, error_current_file, error_current_function);
        return TYPE_UNKNOWN;
    }

    if (!expr->data.member.via_ptr && obj_sym && obj_sym->type == TYPE_UNION &&
        obj_sym->active_member && strcmp(obj_sym->active_member, expr->data.member.member) != 0) {
        error_set(expr->line, expr->column, error_current_file, error_current_function);
        return TYPE_UNKNOWN;
    }

    if (out) {
        ir_value_t idx = ir_build_const(ir, (int)moff);
        ir_value_t addr = ir_build_ptr_add(ir, base_addr, idx, 1);
        int restr = 0;
        if (expr->data.member.via_ptr && expr->data.member.object->kind == EXPR_IDENT) {
            symbol_t *rsym = symtable_lookup(vars,
                                            expr->data.member.object->data.ident.name);
            restr = rsym ? rsym->is_restrict : 0;
        }
        ir_value_t word = restr ? ir_build_load_ptr_res(ir, addr)
                                : ir_build_load_ptr(ir, addr);
        if (mbw > 0) {
            if (mbo)
                word = ir_build_binop(ir, IR_SHR, word,
                                      ir_build_const(ir, (int)mbo));
            unsigned mask_bits = (mbw == 32) ? 0xFFFFFFFFu : ((1u << mbw) - 1u);
            ir_value_t maskv = ir_build_const(ir, (int)mask_bits);
            word = ir_build_binop(ir, IR_AND, word, maskv);
            *out = word;
        } else {
            *out = word;
        }
    }
    return (mbw > 0) ? TYPE_INT : mtype;
}

/*
 * Validate a compound literal, allocate temporary storage and emit IR
 * to initialize each element.
 */
type_kind_t check_complit_expr(expr_t *expr, symtable_t *vars,
                               symtable_t *funcs, ir_builder_t *ir,
                               ir_value_t *out)
{
    size_t count = expr->data.compound.init_count;
    size_t arr_sz = expr->data.compound.array_size;
    if (expr->data.compound.type == TYPE_ARRAY && arr_sz == 0)
        arr_sz = count;
    size_t total = (arr_sz ? arr_sz : 1) * expr->data.compound.elem_size;
    ir_value_t sizev = ir_build_const(ir, (int)total);
    ir_value_t addr = ir_build_alloca(ir, sizev);
    if (expr->data.compound.init_list) {
        for (size_t i = 0; i < count; i++) {
            init_entry_t *e = &expr->data.compound.init_list[i];
            if (e->kind != INIT_SIMPLE)
                return TYPE_UNKNOWN;
            ir_value_t val;
            if (check_expr(e->value, vars, funcs, ir, &val) == TYPE_UNKNOWN)
                return TYPE_UNKNOWN;
            ir_value_t idxv = ir_build_const(ir, (int)i);
            ir_value_t ptr = ir_build_ptr_add(ir, addr, idxv,
                                             (int)expr->data.compound.elem_size);
            ir_build_store_ptr(ir, ptr, val);
        }
    } else if (expr->data.compound.init) {
        ir_value_t val;
        if (check_expr(expr->data.compound.init, vars, funcs, ir, &val) == TYPE_UNKNOWN)
            return TYPE_UNKNOWN;
        ir_build_store_ptr(ir, addr, val);
    }
    if (out) {
        if (expr->data.compound.type == TYPE_ARRAY || expr->data.compound.type == TYPE_STRUCT || expr->data.compound.type == TYPE_UNION)
            *out = addr;
        else
            *out = ir_build_load_ptr(ir, addr);
    }
    if (expr->data.compound.type == TYPE_ARRAY || expr->data.compound.type == TYPE_STRUCT || expr->data.compound.type == TYPE_UNION)
        return TYPE_PTR;
    else
        return expr->data.compound.type;
}

