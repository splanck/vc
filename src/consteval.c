/*
 * Constant expression evaluation implementation.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include "consteval.h"
#include "symtable.h"

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

int eval_const_expr(expr_t *expr, symtable_t *vars, long long *out)
{
    if (!expr)
        return 0;
    switch (expr->kind) {
    case EXPR_NUMBER:
        if (out)
            *out = strtoll(expr->number.value, NULL, 0);
        return 1;
    case EXPR_CHAR:
        if (out)
            *out = (long long)expr->ch.value;
        return 1;
    case EXPR_UNARY:
        if (expr->unary.op == UNOP_NEG) {
            long long val;
            if (eval_const_expr(expr->unary.operand, vars, &val)) {
                if (out)
                    *out = -val;
                return 1;
            }
        }
        return 0;
    case EXPR_BINARY: {
        long long a, b;
        if (!eval_const_expr(expr->binary.left, vars, &a) ||
            !eval_const_expr(expr->binary.right, vars, &b))
            return 0;
        switch (expr->binary.op) {
        case BINOP_ADD: if (out) *out = a + b; break;
        case BINOP_SUB: if (out) *out = a - b; break;
        case BINOP_MUL: if (out) *out = a * b; break;
        case BINOP_DIV: if (out) *out = b != 0 ? a / b : 0; break;
        case BINOP_MOD: if (out) *out = b != 0 ? a % b : 0; break;
        case BINOP_SHL: if (out) *out = a << b; break;
        case BINOP_SHR: if (out) *out = a >> b; break;
        case BINOP_BITAND: if (out) *out = a & b; break;
        case BINOP_BITXOR: if (out) *out = a ^ b; break;
        case BINOP_BITOR: if (out) *out = a | b; break;
        case BINOP_EQ:  if (out) *out = (a == b); break;
        case BINOP_NEQ: if (out) *out = (a != b); break;
        case BINOP_LT:  if (out) *out = (a < b); break;
        case BINOP_GT:  if (out) *out = (a > b); break;
        case BINOP_LE:  if (out) *out = (a <= b); break;
        case BINOP_GE:  if (out) *out = (a >= b); break;
        default:
            return 0;
        }
        return 1;
    }
    case EXPR_COND: {
        long long cval;
        if (!eval_const_expr(expr->cond.cond, vars, &cval))
            return 0;
        if (cval)
            return eval_const_expr(expr->cond.then_expr, vars, out);
        else
            return eval_const_expr(expr->cond.else_expr, vars, out);
    }
    case EXPR_IDENT: {
        symbol_t *sym = vars ? symtable_lookup(vars, expr->ident.name) : NULL;
        if (sym && sym->is_enum_const) {
            if (out)
                *out = sym->enum_value;
            return 1;
        }
        return 0;
    }
    case EXPR_SIZEOF:
        if (!expr->sizeof_expr.is_type)
            return 0;
        if (out) {
            int sz = 0;
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
            case TYPE_UNION:
                sz = (int)expr->sizeof_expr.elem_size;
                break;
            default: sz = 0; break;
            }
            *out = sz;
        }
        return 1;
    default:
        return 0;
    }
}

