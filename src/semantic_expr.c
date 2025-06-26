/*
 * Semantic analysis and IR generation.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "semantic_expr.h"
#include "semantic_arith.h"
#include "semantic_mem.h"
#include "semantic_call.h"
#include "consteval.h"
#include "symtable.h"
#include "util.h"
#include "label.h"
#include "error.h"
#include <limits.h>

static type_kind_t check_number_expr(expr_t *expr, symtable_t *vars,
                                     symtable_t *funcs, ir_builder_t *ir,
                                     ir_value_t *out)
{
    (void)vars; (void)funcs;
    long long val = strtoll(expr->number.value, NULL, 0);
    if (out)
        *out = ir_build_const(ir, val);
    if (val > INT_MAX || val < INT_MIN)
        return TYPE_LLONG;
    return TYPE_INT;
}

static type_kind_t check_string_expr(expr_t *expr, symtable_t *vars,
                                     symtable_t *funcs, ir_builder_t *ir,
                                     ir_value_t *out)
{
    (void)vars; (void)funcs;
    if (out)
        *out = ir_build_string(ir, expr->string.value);
    return TYPE_PTR;
}

static type_kind_t check_char_expr(expr_t *expr, symtable_t *vars,
                                   symtable_t *funcs, ir_builder_t *ir,
                                   ir_value_t *out)
{
    (void)vars; (void)funcs;
    if (out)
        *out = ir_build_const(ir, (int)expr->ch.value);
    return TYPE_CHAR;
}

static type_kind_t check_ident_expr(expr_t *expr, symtable_t *vars,
                                    symtable_t *funcs, ir_builder_t *ir,
                                    ir_value_t *out)
{
    (void)funcs;
    symbol_t *sym = symtable_lookup(vars, expr->ident.name);
    if (!sym) {
        error_set(expr->line, expr->column);
        return TYPE_UNKNOWN;
    }
    if (sym->is_enum_const) {
        if (out)
            *out = ir_build_const(ir, sym->enum_value);
        return TYPE_INT;
    }
    if (sym->type == TYPE_ARRAY) {
        if (out)
            *out = ir_build_addr(ir, sym->ir_name);
        return TYPE_PTR;
    } else {
        if (out) {
            if (sym->param_index >= 0)
                *out = ir_build_load_param(ir, sym->param_index);
            else if (sym->is_volatile)
                *out = ir_build_load_vol(ir, sym->ir_name);
            else
                *out = ir_build_load(ir, sym->ir_name);
        }
        return sym->type;
    }
}

static type_kind_t check_cond_expr(expr_t *expr, symtable_t *vars,
                                   symtable_t *funcs, ir_builder_t *ir,
                                   ir_value_t *out)
{
    ir_value_t cond_val;
    if (!is_intlike(check_expr(expr->cond.cond, vars, funcs, ir, &cond_val))) {
        error_set(expr->cond.cond->line, expr->cond.cond->column);
        return TYPE_UNKNOWN;
    }

    char flabel[32], endlabel[32], tmp[32];
    int id = label_next_id();
    label_format_suffix("L", id, "_false", flabel);
    label_format_suffix("L", id, "_end", endlabel);
    label_format("tmp", id, tmp);
    ir_build_bcond(ir, cond_val, flabel);

    ir_value_t tval;
    type_kind_t tt = check_expr(expr->cond.then_expr, vars, funcs, ir, &tval);
    ir_build_store(ir, tmp, tval);
    ir_build_br(ir, endlabel);

    ir_build_label(ir, flabel);
    ir_value_t fval;
    type_kind_t ft = check_expr(expr->cond.else_expr, vars, funcs, ir, &fval);
    ir_build_store(ir, tmp, fval);
    ir_build_label(ir, endlabel);

    if (!(is_intlike(tt) && is_intlike(ft))) {
        error_set(expr->line, expr->column);
        return TYPE_UNKNOWN;
    }
    if (out)
        *out = ir_build_load(ir, tmp);
    if (tt == TYPE_LLONG || tt == TYPE_ULLONG ||
        ft == TYPE_LLONG || ft == TYPE_ULLONG)
        return TYPE_LLONG;
    return TYPE_INT;
}

static type_kind_t check_assign_expr(expr_t *expr, symtable_t *vars,
                                     symtable_t *funcs, ir_builder_t *ir,
                                     ir_value_t *out)
{
    (void)funcs;
    ir_value_t val;
    symbol_t *sym = symtable_lookup(vars, expr->assign.name);
    if (!sym) {
        error_set(expr->line, expr->column);
        return TYPE_UNKNOWN;
    }
    if (sym->is_const) {
        error_set(expr->line, expr->column);
        return TYPE_UNKNOWN;
    }

    type_kind_t vt = check_expr(expr->assign.value, vars, funcs, ir, &val);
    if (((is_intlike(sym->type) && is_intlike(vt)) ||
         (is_floatlike(sym->type) && (is_floatlike(vt) || is_intlike(vt)))) ||
        vt == sym->type) {
        if (sym->param_index >= 0)
            ir_build_store_param(ir, sym->param_index, val);
        else if (sym->is_volatile)
            ir_build_store_vol(ir, expr->assign.name, val);
        else
            ir_build_store(ir, expr->assign.name, val);
        if (out)
            *out = val;
        return sym->type;
    }
    error_set(expr->line, expr->column);
    return TYPE_UNKNOWN;
}

static type_kind_t check_sizeof_expr(expr_t *expr, symtable_t *vars,
                                     symtable_t *funcs, ir_builder_t *ir,
                                     ir_value_t *out)
{
    (void)funcs;
    int sz = 0;
    if (expr->sizeof_expr.is_type) {
        switch (expr->sizeof_expr.type) {
        case TYPE_CHAR: case TYPE_UCHAR: case TYPE_BOOL: sz = 1; break;
        case TYPE_SHORT: case TYPE_USHORT: sz = 2; break;
        case TYPE_INT: case TYPE_UINT: case TYPE_LONG: case TYPE_ULONG: sz = 4; break;
        case TYPE_LLONG: case TYPE_ULLONG: sz = 8; break;
        case TYPE_PTR:  sz = 4; break;
        case TYPE_ARRAY:
            sz = (int)expr->sizeof_expr.array_size *
                 (int)expr->sizeof_expr.elem_size;
            break;
        case TYPE_STRUCT:
            sz = (int)expr->sizeof_expr.elem_size;
            break;
        case TYPE_ENUM:
            sz = 4;
            break;
        default: sz = 0; break;
        }
    } else {
        ir_builder_t tmp; ir_builder_init(&tmp);
        type_kind_t t = check_expr(expr->sizeof_expr.expr, vars, funcs, &tmp, NULL);
        ir_builder_free(&tmp);
        if (t == TYPE_CHAR || t == TYPE_UCHAR || t == TYPE_BOOL) sz = 1;
        else if (t == TYPE_SHORT || t == TYPE_USHORT) sz = 2;
        else if (t == TYPE_INT || t == TYPE_UINT || t == TYPE_LONG || t == TYPE_ULONG || t == TYPE_ENUM) sz = 4;
        else if (t == TYPE_LLONG || t == TYPE_ULLONG) sz = 8;
        else if (t == TYPE_PTR) sz = 4;
        else if (t == TYPE_ARRAY) {
            symbol_t *sym = NULL;
            if (expr->sizeof_expr.expr->kind == EXPR_IDENT)
                sym = symtable_lookup(vars, expr->sizeof_expr.expr->ident.name);
            sz = sym ? (int)sym->array_size * (int)sym->elem_size : 4;
        } else if (t == TYPE_UNION) {
            symbol_t *sym = NULL;
            if (expr->sizeof_expr.expr->kind == EXPR_IDENT)
                sym = symtable_lookup(vars, expr->sizeof_expr.expr->ident.name);
            sz = sym ? (int)sym->total_size : 0;
        } else if (t == TYPE_STRUCT) {
            symbol_t *sym = NULL;
            if (expr->sizeof_expr.expr->kind == EXPR_IDENT)
                sym = symtable_lookup(vars, expr->sizeof_expr.expr->ident.name);
            sz = sym ? (int)sym->struct_total_size : 0;
        }
    }
    if (out)
        *out = ir_build_const(ir, sz);
    return TYPE_INT;
}


/*
 * Perform semantic analysis on an expression and emit IR code.
 * The type of the expression is returned, or TYPE_UNKNOWN on error.
 */
type_kind_t check_expr(expr_t *expr, symtable_t *vars, symtable_t *funcs,
                       ir_builder_t *ir, ir_value_t *out)
{
    if (!expr)
        return TYPE_UNKNOWN;
    switch (expr->kind) {
    case EXPR_NUMBER:
        return check_number_expr(expr, vars, funcs, ir, out);
    case EXPR_STRING:
        return check_string_expr(expr, vars, funcs, ir, out);
    case EXPR_CHAR:
        return check_char_expr(expr, vars, funcs, ir, out);
    case EXPR_UNARY:
        return check_unary_expr(expr, vars, funcs, ir, out);
    case EXPR_IDENT:
        return check_ident_expr(expr, vars, funcs, ir, out);
    case EXPR_BINARY:
        return check_binary_expr(expr, vars, funcs, ir, out);
    case EXPR_COND:
        return check_cond_expr(expr, vars, funcs, ir, out);
    case EXPR_ASSIGN:
        return check_assign_expr(expr, vars, funcs, ir, out);
    case EXPR_INDEX:
        return check_index_expr(expr, vars, funcs, ir, out);
    case EXPR_ASSIGN_INDEX:
        return check_assign_index_expr(expr, vars, funcs, ir, out);
    case EXPR_ASSIGN_MEMBER:
        return check_assign_member_expr(expr, vars, funcs, ir, out);
    case EXPR_MEMBER:
        return check_member_expr(expr, vars, funcs, ir, out);
    case EXPR_SIZEOF:
        return check_sizeof_expr(expr, vars, funcs, ir, out);
    case EXPR_COMPLIT:
        return check_complit_expr(expr, vars, funcs, ir, out);
    case EXPR_CALL:
        return check_call_expr(expr, vars, funcs, ir, out);
    }
    error_set(expr->line, expr->column);
    return TYPE_UNKNOWN;
}
