/*
 * Constant expression evaluation implementation.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include "error.h"
#include "consteval.h"
#include "symtable.h"

/* Forward declarations for helper functions */
static int eval_number(expr_t *expr, long long *out);
static int eval_char(expr_t *expr, long long *out);
static int eval_unary(expr_t *expr, symtable_t *vars,
                      int use_x86_64, long long *out);
static int eval_binary(expr_t *expr, symtable_t *vars,
                       int use_x86_64, long long *out);
static int eval_conditional(expr_t *expr, symtable_t *vars,
                            int use_x86_64, long long *out);
static int eval_ident(expr_t *expr, symtable_t *vars, long long *out);
static int eval_sizeof(expr_t *expr, int use_x86_64, long long *out);
static int lookup_member_offset(symbol_t *sym, const char *name, size_t *out);
static int eval_offsetof(expr_t *expr, symtable_t *vars, long long *out);

/* Report a constant overflow error and return 0 */
static int report_overflow(expr_t *expr)
{
    error_set(expr->line, expr->column, error_current_file, error_current_function);
    error_print("Constant overflow");
    return 0;
}

int is_intlike(type_kind_t t)
{
    switch (t) {
    case TYPE_INT: case TYPE_UINT:
    case TYPE_CHAR: case TYPE_UCHAR:
    case TYPE_SHORT: case TYPE_USHORT:
    case TYPE_LONG: case TYPE_ULONG:
    case TYPE_LLONG: case TYPE_ULLONG:
    case TYPE_BOOL:
    case TYPE_ENUM:
        return 1;
    default:
        return 0;
    }
}

int is_floatlike(type_kind_t t)
{
    return t == TYPE_FLOAT || t == TYPE_DOUBLE || t == TYPE_LDOUBLE;
}

/*
 * Evaluate a numeric literal and return its value.
 */
static int eval_number(expr_t *expr, long long *out)
{
    errno = 0;
    if (expr->number.is_unsigned) {
        unsigned long long val = strtoull(expr->number.value, NULL, 0);
        if (errno != 0)
            return 0;
        if (out)
            *out = (long long)val;
        return 1;
    } else {
        long long val = strtoll(expr->number.value, NULL, 0);
        if (errno != 0)
            return 0;
        if (out)
            *out = val;
        return 1;
    }
}

/*
 * Evaluate a character literal constant.
 */
static int eval_char(expr_t *expr, long long *out)
{
    if (out)
        *out = (long long)expr->ch.value;
    return 1;
}

/*
 * Evaluate a unary expression.  Only negation is supported.
 */
static int eval_unary(expr_t *expr, symtable_t *vars,
                      int use_x86_64, long long *out)
{
    if (expr->unary.op == UNOP_NEG) {
        long long val;
        if (eval_const_expr(expr->unary.operand, vars, use_x86_64, &val)) {
            if (val == LLONG_MIN)
                return report_overflow(expr);
            if (out)
                *out = -val;
            return 1;
        }
    }
    return 0;
}

/*
 * Evaluate a binary operator expression.
 */
static int eval_binary(expr_t *expr, symtable_t *vars,
                       int use_x86_64, long long *out)
{
    long long a, b;
    if (!eval_const_expr(expr->binary.left, vars, use_x86_64, &a) ||
        !eval_const_expr(expr->binary.right, vars, use_x86_64, &b))
        return 0;

    long long tmp;
    switch (expr->binary.op) {
    case BINOP_ADD:
        if (__builtin_add_overflow(a, b, &tmp))
            return report_overflow(expr);
        if (out) *out = tmp;
        break;
    case BINOP_SUB:
        if (__builtin_sub_overflow(a, b, &tmp))
            return report_overflow(expr);
        if (out) *out = tmp;
        break;
    case BINOP_MUL:
        if (__builtin_mul_overflow(a, b, &tmp))
            return report_overflow(expr);
        if (out) *out = tmp;
        break;
    case BINOP_DIV:
        if (b != 0) {
            if (a == LLONG_MIN && b == -1)
                return report_overflow(expr);
            tmp = a / b;
        } else {
            tmp = 0;
        }
        if (out) *out = tmp;
        break;
    case BINOP_MOD:     if (out) *out = b != 0 ? a % b : 0; break;
    case BINOP_SHL:     if (out) *out = a << b; break;
    case BINOP_SHR:     if (out) *out = a >> b; break;
    case BINOP_BITAND:  if (out) *out = a & b; break;
    case BINOP_BITXOR:  if (out) *out = a ^ b; break;
    case BINOP_BITOR:   if (out) *out = a | b; break;
    case BINOP_EQ:      if (out) *out = (a == b); break;
    case BINOP_NEQ:     if (out) *out = (a != b); break;
    case BINOP_LT:      if (out) *out = (a < b); break;
    case BINOP_GT:      if (out) *out = (a > b); break;
    case BINOP_LE:      if (out) *out = (a <= b); break;
    case BINOP_GE:      if (out) *out = (a >= b); break;
    default:
        return 0;
    }
    return 1;
}

/*
 * Evaluate a ternary conditional expression.
 */
static int eval_conditional(expr_t *expr, symtable_t *vars,
                            int use_x86_64, long long *out)
{
    long long cval;
    if (!eval_const_expr(expr->cond.cond, vars, use_x86_64, &cval))
        return 0;
    if (cval)
        return eval_const_expr(expr->cond.then_expr, vars, use_x86_64, out);
    return eval_const_expr(expr->cond.else_expr, vars, use_x86_64, out);
}

/*
 * Resolve an identifier that refers to an enumeration constant.
 */
static int eval_ident(expr_t *expr, symtable_t *vars, long long *out)
{
    symbol_t *sym = vars ? symtable_lookup(vars, expr->ident.name) : NULL;
    if (sym && sym->is_enum_const) {
        if (out)
            *out = sym->enum_value;
        return 1;
    }
    return 0;
}

/*
 * Evaluate a sizeof type expression.
 */
static int eval_sizeof(expr_t *expr, int use_x86_64, long long *out)
{
    if (!expr->sizeof_expr.is_type)
        return 0;
    if (out) {
        int sz = 0;
        switch (expr->sizeof_expr.type) {
        case TYPE_CHAR: case TYPE_UCHAR: case TYPE_BOOL: sz = 1; break;
        case TYPE_SHORT: case TYPE_USHORT: sz = 2; break;
        case TYPE_INT: case TYPE_UINT:
        case TYPE_LONG: case TYPE_ULONG: sz = 4; break;
        case TYPE_LLONG: case TYPE_ULLONG: sz = 8; break;
        case TYPE_PTR:
            sz = use_x86_64 ? 8 : 4;
            break;
        case TYPE_ARRAY:
            sz = (int)expr->sizeof_expr.array_size *
                 (int)expr->sizeof_expr.elem_size;
            break;
        case TYPE_UNION:
            sz = (int)expr->sizeof_expr.elem_size;
            break;
        default: sz = 0; break;
        }
        *out = sz;
    }
    return 1;
}

/* Find a member offset within a struct or union symbol. */
static int lookup_member_offset(symbol_t *sym, const char *name, size_t *out)
{
    if (!sym)
        return 0;
    if (sym->type == TYPE_UNION) {
        for (size_t i = 0; i < sym->member_count; i++) {
            if (strcmp(sym->members[i].name, name) == 0) {
                if (out)
                    *out = sym->members[i].offset;
                return 1;
            }
        }
    } else if (sym->type == TYPE_STRUCT) {
        for (size_t i = 0; i < sym->struct_member_count; i++) {
            if (strcmp(sym->struct_members[i].name, name) == 0) {
                if (out)
                    *out = sym->struct_members[i].offset;
                return 1;
            }
        }
    }
    return 0;
}

/* Evaluate an offsetof expression. */
static int eval_offsetof(expr_t *expr, symtable_t *vars, long long *out)
{
    symbol_t *sym = NULL;
    if (expr->offsetof_expr.type == TYPE_STRUCT)
        sym = vars ? symtable_lookup_struct(vars, expr->offsetof_expr.tag) : NULL;
    else if (expr->offsetof_expr.type == TYPE_UNION)
        sym = vars ? symtable_lookup_union(vars, expr->offsetof_expr.tag) : NULL;
    if (!sym || expr->offsetof_expr.member_count == 0)
        return 0;
    size_t off = 0;
    if (!lookup_member_offset(sym, expr->offsetof_expr.members[0], &off))
        return 0;
    if (out)
        *out = (long long)off;
    return 1;
}

/*
 * Evaluate a cast expression when the operand is constant.
 * The value is returned unchanged as primitive types share a
 * unified representation in the evaluator.
 */
static int eval_cast(expr_t *expr, symtable_t *vars,
                     int use_x86_64, long long *out)
{
    (void)use_x86_64;
    long long val;
    if (!eval_const_expr(expr->cast.expr, vars, use_x86_64, &val))
        return 0;
    if (out)
        *out = val;
    return 1;
}

int eval_const_expr(expr_t *expr, symtable_t *vars,
                    int use_x86_64, long long *out)
{
    if (!expr)
        return 0;
    switch (expr->kind) {
    case EXPR_NUMBER:
        return eval_number(expr, out);
    case EXPR_CHAR:
        return eval_char(expr, out);
    case EXPR_UNARY:
        return eval_unary(expr, vars, use_x86_64, out);
    case EXPR_BINARY:
        return eval_binary(expr, vars, use_x86_64, out);
    case EXPR_COND:
        return eval_conditional(expr, vars, use_x86_64, out);
    case EXPR_IDENT:
        return eval_ident(expr, vars, out);
    case EXPR_CAST:
        return eval_cast(expr, vars, use_x86_64, out);
    case EXPR_SIZEOF:
        return eval_sizeof(expr, use_x86_64, out);
    case EXPR_OFFSETOF:
        return eval_offsetof(expr, vars, out);
    default:
        return 0;
    }
}

