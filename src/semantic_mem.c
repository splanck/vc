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
#include "util.h"
#include "label.h"
#include "error.h"
#include <limits.h>

/*
 * Validate array indexing and emit a load from the computed element
 * address in the IR.
 */
type_kind_t check_index_expr(expr_t *expr, symtable_t *vars,
                             symtable_t *funcs, ir_builder_t *ir,
                             ir_value_t *out)
{
    (void)funcs;
    if (expr->index.array->kind != EXPR_IDENT) {
        error_set(expr->line, expr->column);
        return TYPE_UNKNOWN;
    }
    symbol_t *sym = symtable_lookup(vars, expr->index.array->ident.name);
    if (!sym || sym->type != TYPE_ARRAY) {
        error_set(expr->line, expr->column);
        return TYPE_UNKNOWN;
    }
    ir_value_t idx_val;
    if (check_expr(expr->index.index, vars, funcs, ir, &idx_val) != TYPE_INT) {
        error_set(expr->index.index->line, expr->index.index->column);
        return TYPE_UNKNOWN;
    }
    long long cval;
    if (sym->array_size && eval_const_expr(expr->index.index, vars, &cval)) {
        if (cval < 0 || (size_t)cval >= sym->array_size) {
            error_set(expr->index.index->line, expr->index.index->column);
            return TYPE_UNKNOWN;
        }
    }
    if (out)
        *out = sym->is_volatile
                 ? ir_build_load_idx_vol(ir, sym->ir_name, idx_val)
                 : ir_build_load_idx(ir, sym->ir_name, idx_val);
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
    if (expr->assign_index.array->kind != EXPR_IDENT) {
        error_set(expr->line, expr->column);
        return TYPE_UNKNOWN;
    }
    symbol_t *sym = symtable_lookup(vars, expr->assign_index.array->ident.name);
    if (!sym || sym->type != TYPE_ARRAY) {
        error_set(expr->line, expr->column);
        return TYPE_UNKNOWN;
    }
    if (sym->is_const) {
        error_set(expr->line, expr->column);
        return TYPE_UNKNOWN;
    }

    ir_value_t idx_val, val;
    if (check_expr(expr->assign_index.index, vars, funcs, ir, &idx_val) != TYPE_INT) {
        error_set(expr->assign_index.index->line, expr->assign_index.index->column);
        return TYPE_UNKNOWN;
    }
    if (check_expr(expr->assign_index.value, vars, funcs, ir, &val) != TYPE_INT) {
        error_set(expr->assign_index.value->line, expr->assign_index.value->column);
        return TYPE_UNKNOWN;
    }
    long long cval;
    if (sym->array_size && eval_const_expr(expr->assign_index.index, vars, &cval)) {
        if (cval < 0 || (size_t)cval >= sym->array_size) {
            error_set(expr->assign_index.index->line, expr->assign_index.index->column);
            return TYPE_UNKNOWN;
        }
    }
    if (sym->is_volatile)
        ir_build_store_idx_vol(ir, sym->ir_name, idx_val, val);
    else
        ir_build_store_idx(ir, sym->ir_name, idx_val, val);
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
    if (expr->assign_member.via_ptr) {
        if (check_expr(expr->assign_member.object, vars, funcs, ir,
                       &base_addr) != TYPE_PTR) {
            error_set(expr->assign_member.object->line,
                      expr->assign_member.object->column);
            return TYPE_UNKNOWN;
        }
        if (expr->assign_member.object->kind == EXPR_IDENT)
            obj_sym = symtable_lookup(vars,
                                      expr->assign_member.object->ident.name);
    } else {
        if (expr->assign_member.object->kind != EXPR_IDENT) {
            error_set(expr->assign_member.object->line,
                      expr->assign_member.object->column);
            return TYPE_UNKNOWN;
        }
        obj_sym = symtable_lookup(vars, expr->assign_member.object->ident.name);
        if (!obj_sym || (obj_sym->type != TYPE_UNION && obj_sym->type != TYPE_STRUCT)) {
            error_set(expr->assign_member.object->line,
                      expr->assign_member.object->column);
            return TYPE_UNKNOWN;
        }
        base_addr = ir_build_addr(ir, obj_sym->ir_name);
    }

    if (!obj_sym ||
        ((obj_sym->type == TYPE_UNION && obj_sym->member_count == 0) ||
         (obj_sym->type == TYPE_STRUCT && obj_sym->struct_member_count == 0))) {
        error_set(expr->line, expr->column);
        return TYPE_UNKNOWN;
    }

    type_kind_t mtype = TYPE_UNKNOWN;
    size_t moff = 0;
    if (obj_sym->type == TYPE_UNION) {
        for (size_t i = 0; i < obj_sym->member_count; i++) {
            if (strcmp(obj_sym->members[i].name, expr->assign_member.member) == 0) {
                mtype = obj_sym->members[i].type;
                moff = obj_sym->members[i].offset;
                break;
            }
        }
    } else {
        for (size_t i = 0; i < obj_sym->struct_member_count; i++) {
            if (strcmp(obj_sym->struct_members[i].name, expr->assign_member.member) == 0) {
                mtype = obj_sym->struct_members[i].type;
                moff = obj_sym->struct_members[i].offset;
                break;
            }
        }
    }
    if (mtype == TYPE_UNKNOWN) {
        error_set(expr->line, expr->column);
        return TYPE_UNKNOWN;
    }

    ir_value_t val;
    type_kind_t vt = check_expr(expr->assign_member.value, vars, funcs, ir, &val);
    if (!(((is_intlike(mtype) && is_intlike(vt)) ||
           (is_floatlike(mtype) && (is_floatlike(vt) || is_intlike(vt)))) ||
          vt == mtype)) {
        error_set(expr->assign_member.value->line, expr->assign_member.value->column);
        return TYPE_UNKNOWN;
    }

    ir_value_t idx = ir_build_const(ir, (int)moff);
    ir_value_t addr = ir_build_ptr_add(ir, base_addr, idx, 1);
    ir_build_store_ptr(ir, addr, val);
    if (out)
        *out = val;
    return mtype;
}

/*
 * Validate member access on a struct, union or pointer and emit a load
 * from the member's address.
 */
type_kind_t check_member_expr(expr_t *expr, symtable_t *vars,
                               symtable_t *funcs, ir_builder_t *ir,
                               ir_value_t *out)
{
    if (!expr->member.object)
        return TYPE_UNKNOWN;
    symbol_t *obj_sym = NULL;
    ir_value_t base_addr;
    if (expr->member.via_ptr) {
        if (check_expr(expr->member.object, vars, funcs, ir,
                       &base_addr) != TYPE_PTR) {
            error_set(expr->member.object->line,
                      expr->member.object->column);
            return TYPE_UNKNOWN;
        }
        if (expr->member.object->kind == EXPR_IDENT)
            obj_sym = symtable_lookup(vars,
                                      expr->member.object->ident.name);
    } else {
        if (expr->member.object->kind != EXPR_IDENT) {
            error_set(expr->member.object->line,
                      expr->member.object->column);
            return TYPE_UNKNOWN;
        }
        obj_sym = symtable_lookup(vars, expr->member.object->ident.name);
        if (!obj_sym || (obj_sym->type != TYPE_UNION && obj_sym->type != TYPE_STRUCT)) {
            error_set(expr->member.object->line,
                      expr->member.object->column);
            return TYPE_UNKNOWN;
        }
        base_addr = ir_build_addr(ir, obj_sym->ir_name);
    }

    if (!obj_sym ||
        ((obj_sym->type == TYPE_UNION && obj_sym->member_count == 0) ||
         (obj_sym->type == TYPE_STRUCT && obj_sym->struct_member_count == 0))) {
        error_set(expr->line, expr->column);
        return TYPE_UNKNOWN;
    }

    type_kind_t mtype = TYPE_UNKNOWN;
    size_t moff = 0;
    if (obj_sym->type == TYPE_UNION) {
        for (size_t i = 0; i < obj_sym->member_count; i++) {
            if (strcmp(obj_sym->members[i].name, expr->member.member) == 0) {
                mtype = obj_sym->members[i].type;
                moff = obj_sym->members[i].offset;
                break;
            }
        }
    } else {
        for (size_t i = 0; i < obj_sym->struct_member_count; i++) {
            if (strcmp(obj_sym->struct_members[i].name, expr->member.member) == 0) {
                mtype = obj_sym->struct_members[i].type;
                moff = obj_sym->struct_members[i].offset;
                break;
            }
        }
    }
    if (mtype == TYPE_UNKNOWN) {
        error_set(expr->line, expr->column);
        return TYPE_UNKNOWN;
    }

    if (out) {
        ir_value_t idx = ir_build_const(ir, (int)moff);
        ir_value_t addr = ir_build_ptr_add(ir, base_addr, idx, 1);
        *out = ir_build_load_ptr(ir, addr);
    }
    return mtype;
}

/*
 * Validate a compound literal, allocate temporary storage and emit IR
 * to initialize each element.
 */
type_kind_t check_complit_expr(expr_t *expr, symtable_t *vars,
                               symtable_t *funcs, ir_builder_t *ir,
                               ir_value_t *out)
{
    size_t count = expr->compound.init_count;
    size_t arr_sz = expr->compound.array_size;
    if (expr->compound.type == TYPE_ARRAY && arr_sz == 0)
        arr_sz = count;
    size_t total = (arr_sz ? arr_sz : 1) * expr->compound.elem_size;
    ir_value_t sizev = ir_build_const(ir, (int)total);
    ir_value_t addr = ir_build_alloca(ir, sizev);
    if (expr->compound.init_list) {
        for (size_t i = 0; i < count; i++) {
            init_entry_t *e = &expr->compound.init_list[i];
            if (e->kind != INIT_SIMPLE)
                return TYPE_UNKNOWN;
            ir_value_t val;
            if (check_expr(e->value, vars, funcs, ir, &val) == TYPE_UNKNOWN)
                return TYPE_UNKNOWN;
            ir_value_t idxv = ir_build_const(ir, (int)i);
            ir_value_t ptr = ir_build_ptr_add(ir, addr, idxv,
                                             (int)expr->compound.elem_size);
            ir_build_store_ptr(ir, ptr, val);
        }
    } else if (expr->compound.init) {
        ir_value_t val;
        if (check_expr(expr->compound.init, vars, funcs, ir, &val) == TYPE_UNKNOWN)
            return TYPE_UNKNOWN;
        ir_build_store_ptr(ir, addr, val);
    }
    if (out) {
        if (expr->compound.type == TYPE_ARRAY || expr->compound.type == TYPE_STRUCT || expr->compound.type == TYPE_UNION)
            *out = addr;
        else
            *out = ir_build_load_ptr(ir, addr);
    }
    if (expr->compound.type == TYPE_ARRAY || expr->compound.type == TYPE_STRUCT || expr->compound.type == TYPE_UNION)
        return TYPE_PTR;
    else
        return expr->compound.type;
}

