/*
 * Semantic analysis and IR generation.
 * Helper routines validate all expression kinds and build
 * the corresponding intermediate representation.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "semantic_expr.h"
#include "semantic_expr_ops.h"
#include "semantic_mem.h"
#include "semantic_call.h"
#include "consteval.h"
#include "symtable.h"
#include "util.h"
#include "label.h"
#include "error.h"
#include <limits.h>
#include <errno.h>

type_kind_t check_number_expr(expr_t *expr, symtable_t *vars,
                              symtable_t *funcs, ir_builder_t *ir,
                              ir_value_t *out);
type_kind_t check_string_expr(expr_t *expr, symtable_t *vars,
                              symtable_t *funcs, ir_builder_t *ir,
                              ir_value_t *out);
type_kind_t check_char_expr(expr_t *expr, symtable_t *vars,
                            symtable_t *funcs, ir_builder_t *ir,
                            ir_value_t *out);
type_kind_t check_complex_literal(expr_t *expr, symtable_t *vars,
                                  symtable_t *funcs, ir_builder_t *ir,
                                  ir_value_t *out);
type_kind_t check_cast_expr(expr_t *expr, symtable_t *vars,
                            symtable_t *funcs, ir_builder_t *ir,
                            ir_value_t *out);


/*
 * Resolve an identifier, ensuring it exists and loading its value or address
 * into the IR.  Enum constants become immediate values.
 */
static type_kind_t check_ident_expr(expr_t *expr, symtable_t *vars,
                                    symtable_t *funcs, ir_builder_t *ir,
                                    ir_value_t *out)
{
    (void)funcs;
    symbol_t *sym = symtable_lookup(vars, expr->data.ident.name);
    if (!sym) {
        if (strcmp(expr->data.ident.name, "__func__") == 0) {
            if (out)
                *out = ir_build_string(ir, error_current_function ? error_current_function : "");
            return TYPE_PTR;
        }
        error_set(expr->line, expr->column, error_current_file, error_current_function);
        if (out)
            *out = (ir_value_t){0};
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

/* Validate the operand types of a conditional expression. */
static int validate_cond_operands(expr_t *expr, symtable_t *vars,
                                  symtable_t *funcs, type_kind_t *ct,
                                  type_kind_t *tt, type_kind_t *ft)
{
    ir_builder_t tmpb; ir_builder_init(&tmpb);
    ir_value_t tmpv;
    *ct = check_expr(expr->data.cond.cond, vars, funcs, &tmpb, &tmpv);
    *tt = check_expr(expr->data.cond.then_expr, vars, funcs, &tmpb, NULL);
    *ft = check_expr(expr->data.cond.else_expr, vars, funcs, &tmpb, NULL);
    ir_builder_free(&tmpb);

    if (!is_intlike(*ct) || *tt == TYPE_UNKNOWN || *ft == TYPE_UNKNOWN) {
        if (!is_intlike(*ct))
            error_set(expr->data.cond.cond->line, expr->data.cond.cond->column,
                      error_current_file, error_current_function);
        return 0;
    }
    if (!(is_intlike(*tt) && is_intlike(*ft))) {
        error_set(expr->line, expr->column, error_current_file,
                  error_current_function);
        return 0;
    }
    return 1;
}

/* Emit IR for the conditional branches. */
static int emit_cond_branches(expr_t *expr, symtable_t *vars,
                              symtable_t *funcs, ir_builder_t *ir,
                              ir_value_t *out)
{
    ir_value_t cond_val;
    check_expr(expr->data.cond.cond, vars, funcs, ir, &cond_val);

    char flabel[32], endlabel[32], tmp[32];
    int id = label_next_id();
    if (!label_format_suffix("L", id, "_false", flabel) ||
        !label_format_suffix("L", id, "_end", endlabel) ||
        !label_format("tmp", id, tmp))
        return 0;

    ir_build_bcond(ir, cond_val, flabel);
    ir_value_t tval;
    check_expr(expr->data.cond.then_expr, vars, funcs, ir, &tval);
    ir_build_store(ir, tmp, tval);
    ir_build_br(ir, endlabel);

    ir_build_label(ir, flabel);
    ir_value_t fval;
    check_expr(expr->data.cond.else_expr, vars, funcs, ir, &fval);
    ir_build_store(ir, tmp, fval);
    ir_build_label(ir, endlabel);

    if (out)
        *out = ir_build_load(ir, tmp);
    return 1;
}

/* Determine the resulting type of a conditional expression. */
static type_kind_t result_cond_type(type_kind_t tt, type_kind_t ft)
{
    if (tt == TYPE_LLONG || tt == TYPE_ULLONG ||
        ft == TYPE_LLONG || ft == TYPE_ULLONG)
        return TYPE_LLONG;
    return TYPE_INT;
}

/* Check basic type compatibility for assignments. */
static int types_compatible(type_kind_t lhs_type, type_kind_t rhs_type)
{
    return ((is_intlike(lhs_type) && is_intlike(rhs_type)) ||
            (is_floatlike(lhs_type) &&
             (is_floatlike(rhs_type) || is_intlike(rhs_type))) ||
            (is_complexlike(lhs_type) && rhs_type == lhs_type) ||
            lhs_type == rhs_type);
}

/*
 * Validate a ternary conditional expression.  Both branches are checked and
 * IR is emitted to select the appropriate value based on the condition.
 */
static type_kind_t check_cond_expr(expr_t *expr, symtable_t *vars,
                                   symtable_t *funcs, ir_builder_t *ir,
                                   ir_value_t *out)
{
    type_kind_t ct, tt, ft;
    if (!validate_cond_operands(expr, vars, funcs, &ct, &tt, &ft)) {
        if (out)
            *out = (ir_value_t){0};
        return TYPE_UNKNOWN;
    }

    if (!emit_cond_branches(expr, vars, funcs, ir, out)) {
        if (out)
            *out = (ir_value_t){0};
        return TYPE_UNKNOWN;
    }

    (void)ct;
    return result_cond_type(tt, ft);
}

/*
 * Validate an assignment to a variable and generate the store operation in
 * the IR.  Assignments are permitted when:
 *   - both types are integral;
 *   - the destination is floating point and the source is either floating
 *     point or integral;
 *   - both types are the same complex type;
 *   - or the types match exactly (used for pointers and aggregates).
 */
static type_kind_t check_assign_expr(expr_t *expr, symtable_t *vars,
                                     symtable_t *funcs, ir_builder_t *ir,
                                     ir_value_t *out)
{
    (void)funcs;
    ir_value_t val;

    /* Step 1: symbol lookup */
    symbol_t *sym = symtable_lookup(vars, expr->data.assign.name);
    if (!sym) {
        error_set(expr->line, expr->column, error_current_file, error_current_function);
        error_print("unknown identifier");
        if (out)
            *out = (ir_value_t){0};
        return TYPE_UNKNOWN;
    }

    /* Step 2: const protection */
    if (sym->is_const) {
        error_set(expr->line, expr->column, error_current_file, error_current_function);
        error_print("assignment to const");
        if (out)
            *out = (ir_value_t){0};
        return TYPE_UNKNOWN;
    }

    /* Step 3: type compatibility */
    type_kind_t vt = check_expr(expr->data.assign.value, vars, funcs, ir, &val);
    if (!types_compatible(sym->type, vt)) {
        error_set(expr->line, expr->column, error_current_file, error_current_function);
        error_print("incompatible types in assignment");
        if (out)
            *out = (ir_value_t){0};
        return TYPE_UNKNOWN;
    }

    /* Step 4: IR emission */
    if (sym->param_index >= 0)
        ir_build_store_param(ir, sym->param_index, val);
    else if (sym->is_volatile)
        ir_build_store_vol(ir, sym->ir_name, val);
    else
        ir_build_store(ir, sym->ir_name, val);

    if (out)
        *out = val;
    return sym->type;
}

/*
 * Validate a type cast expression. The operand expression is evaluated
 * and checked for compatibility with the destination type. No IR is
 * emitted for the conversion itself as primitive types share the same
 * representation.
 */

/*
 * Determine the byte size of a type operand of sizeof().
 * Array and struct sizes are provided by the parser.
 */
static int sizeof_from_type(type_kind_t type, size_t array_size,
                            size_t elem_size)
{
    switch (type) {
    /* single byte integer types */
    case TYPE_CHAR: case TYPE_UCHAR: case TYPE_BOOL:
        return 1;
    /* two byte integer types */
    case TYPE_SHORT: case TYPE_USHORT:
        return 2;
    /* four byte integer types, also used for enums */
    case TYPE_INT: case TYPE_UINT: case TYPE_LONG: case TYPE_ULONG:
    case TYPE_ENUM:
        return 4;
    /* eight byte integer types */
    case TYPE_LLONG: case TYPE_ULLONG:
        return 8;
    /* pointer size */
    case TYPE_PTR:
        return 4;
    case TYPE_ARRAY:
        /* arrays: element size times element count */
        return (int)array_size * (int)elem_size;
    case TYPE_STRUCT:
        /* elem_size stores the total struct size */
        return (int)elem_size;
    default:
        return 0;
    }
}

/*
 * Determine the byte size of an expression operand of sizeof().
 * For aggregate types the symbol table is consulted when possible.
 */
static int sizeof_from_expr(expr_t *op, type_kind_t t, symtable_t *vars)
{
    if (t == TYPE_CHAR || t == TYPE_UCHAR || t == TYPE_BOOL)
        return 1;
    if (t == TYPE_SHORT || t == TYPE_USHORT)
        return 2;
    if (t == TYPE_INT || t == TYPE_UINT || t == TYPE_LONG ||
        t == TYPE_ULONG || t == TYPE_ENUM)
        return 4;
    if (t == TYPE_LLONG || t == TYPE_ULLONG)
        return 8;
    if (t == TYPE_PTR)
        return 4;
    if (t == TYPE_ARRAY) {
        symbol_t *sym = NULL;
        if (op && op->kind == EXPR_IDENT)
            sym = symtable_lookup(vars, op->data.ident.name);
        return sym ? (int)sym->array_size * (int)sym->elem_size : 4;
    }
    if (t == TYPE_UNION) {
        symbol_t *sym = NULL;
        if (op && op->kind == EXPR_IDENT)
            sym = symtable_lookup(vars, op->data.ident.name);
        return sym ? (int)sym->total_size : 0;
    }
    if (t == TYPE_STRUCT) {
        symbol_t *sym = NULL;
        if (op && op->kind == EXPR_IDENT)
            sym = symtable_lookup(vars, op->data.ident.name);
        return sym ? (int)sym->struct_total_size : 0;
    }
    return 0;
}

/*
 * Compute the size of a type or expression and emit a constant IR value with
 * that size in bytes.
 */
static type_kind_t check_sizeof_expr(expr_t *expr, symtable_t *vars,
                                     symtable_t *funcs, ir_builder_t *ir,
                                     ir_value_t *out)
{
    (void)funcs;
    if (expr->data.sizeof_expr.is_type) {
        int sz = sizeof_from_type(expr->data.sizeof_expr.type,
                                 expr->data.sizeof_expr.array_size,
                                 expr->data.sizeof_expr.elem_size);
        if (out)
            *out = ir_build_const(ir, sz);
    } else {
        ir_builder_t tmp; ir_builder_init(&tmp);
        type_kind_t t = check_expr(expr->data.sizeof_expr.expr, vars, funcs,
                                   &tmp, NULL);
        ir_builder_free(&tmp);

        symbol_t *sym = NULL;
        if (expr->data.sizeof_expr.expr &&
            expr->data.sizeof_expr.expr->kind == EXPR_IDENT)
            sym = symtable_lookup(vars,
                                  expr->data.sizeof_expr.expr->data.ident.name);

        if (sym && sym->vla_size.id) {
            ir_value_t eszv = ir_build_const(ir, (int)sym->elem_size);
            ir_value_t total = ir_build_binop(ir, IR_MUL,
                                              sym->vla_size, eszv);
            if (out)
                *out = total;
        } else {
            int sz = sizeof_from_expr(expr->data.sizeof_expr.expr, t, vars);
            if (out)
                *out = ir_build_const(ir, sz);
        }
    }
    return TYPE_INT;
}

/* Compute the offset of a member within a struct or union type. */
static type_kind_t check_offsetof_expr(expr_t *expr, symtable_t *vars,
                                       symtable_t *funcs, ir_builder_t *ir,
                                       ir_value_t *out)
{
    (void)funcs;
    symbol_t *sym = NULL;
    if (expr->data.offsetof_expr.type == TYPE_STRUCT)
        sym = symtable_lookup_struct(vars, expr->data.offsetof_expr.tag);
    else if (expr->data.offsetof_expr.type == TYPE_UNION)
        sym = symtable_lookup_union(vars, expr->data.offsetof_expr.tag);
    if (!sym || expr->data.offsetof_expr.member_count == 0) {
        error_set(expr->line, expr->column, error_current_file,
                  error_current_function);
        return TYPE_UNKNOWN;
    }
    size_t off = 0;
    int found = 0;
    if (sym->type == TYPE_STRUCT) {
        for (size_t i = 0; i < sym->struct_member_count; i++)
            if (strcmp(sym->struct_members[i].name,
                       expr->data.offsetof_expr.members[0]) == 0) {
                off = sym->struct_members[i].offset;
                found = 1; break;
            }
    } else {
        for (size_t i = 0; i < sym->member_count; i++)
            if (strcmp(sym->members[i].name,
                       expr->data.offsetof_expr.members[0]) == 0) {
                off = sym->members[i].offset;
                found = 1; break;
            }
    }
    if (!found) {
        error_set(expr->line, expr->column, error_current_file,
                  error_current_function);
        return TYPE_UNKNOWN;
    }
    if (out)
        *out = ir_build_const(ir, (int)off);
    return TYPE_INT;
}

static type_kind_t check_alignof_expr(expr_t *expr, symtable_t *vars,
                                      symtable_t *funcs, ir_builder_t *ir,
                                      ir_value_t *out)
{
    (void)funcs; (void)vars;
    int al = 0;
    if (expr->data.alignof_expr.is_type) {
        switch (expr->data.alignof_expr.type) {
        case TYPE_CHAR: case TYPE_UCHAR: case TYPE_BOOL: al = 1; break;
        case TYPE_SHORT: case TYPE_USHORT: al = 2; break;
        case TYPE_INT: case TYPE_UINT: case TYPE_LONG: case TYPE_ULONG:
        case TYPE_ENUM: case TYPE_FLOAT: al = 4; break;
        case TYPE_LLONG: case TYPE_ULLONG: case TYPE_DOUBLE: al = 8; break;
        case TYPE_PTR: al = 4; break;
        default: al = 1; break;
        }
    } else {
        ir_builder_t tmp; ir_builder_init(&tmp);
        type_kind_t t = check_expr(expr->data.alignof_expr.expr, vars, funcs, &tmp, NULL);
        ir_builder_free(&tmp);
        switch (t) {
        case TYPE_CHAR: case TYPE_UCHAR: case TYPE_BOOL: al = 1; break;
        case TYPE_SHORT: case TYPE_USHORT: al = 2; break;
        case TYPE_INT: case TYPE_UINT: case TYPE_LONG: case TYPE_ULONG:
        case TYPE_ENUM: case TYPE_FLOAT: al = 4; break;
        case TYPE_LLONG: case TYPE_ULLONG: case TYPE_DOUBLE: al = 8; break;
        case TYPE_PTR: al = 4; break;
        default: al = 1; break;
        }
    }
    if (out)
        *out = ir_build_const(ir, al);
    return TYPE_INT;
}


/*
 * Perform semantic analysis on an expression and emit IR code.
 * The type of the expression is returned, or TYPE_UNKNOWN on error.
 */
type_kind_t check_expr(expr_t *expr, symtable_t *vars, symtable_t *funcs,
                       ir_builder_t *ir, ir_value_t *out)
{
    if (!expr) {
        if (out)
            *out = (ir_value_t){0};
        return TYPE_UNKNOWN;
    }
    ir_builder_set_loc(ir, error_current_file, expr->line, expr->column);
    switch (expr->kind) {
    case EXPR_NUMBER:
        return check_number_expr(expr, vars, funcs, ir, out);
    case EXPR_STRING:
        return check_string_expr(expr, vars, funcs, ir, out);
    case EXPR_CHAR:
        return check_char_expr(expr, vars, funcs, ir, out);
    case EXPR_COMPLEX_LITERAL:
        return check_complex_literal(expr, vars, funcs, ir, out);
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
    case EXPR_OFFSETOF:
        return check_offsetof_expr(expr, vars, funcs, ir, out);
    case EXPR_ALIGNOF:
        return check_alignof_expr(expr, vars, funcs, ir, out);
    case EXPR_CAST:
        return check_cast_expr(expr, vars, funcs, ir, out);
    case EXPR_COMPLIT:
        return check_complit_expr(expr, vars, funcs, ir, out);
    case EXPR_CALL:
        return check_call_expr(expr, vars, funcs, ir, out);
    }
    error_set(expr->line, expr->column, error_current_file, error_current_function);
    if (out)
        *out = (ir_value_t){0};
    return TYPE_UNKNOWN;
}
