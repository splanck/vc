/*
 * Arithmetic expression semantic helpers.
 * These routines validate arithmetic and comparison expressions
 * and emit the corresponding IR instructions.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "semantic_arith.h"
#include "semantic_expr.h"
#include "consteval.h"
#include "symtable.h"
#include "util.h"
#include "label.h"
#include "error.h"
#include <limits.h>

/* Mapping from BINOP_* to corresponding IR op.  Logical ops are
 * handled separately and use IR_CMPEQ here just as a placeholder. */
static const ir_op_t binop_to_ir[] = {
    [BINOP_ADD]    = IR_ADD,
    [BINOP_SUB]    = IR_SUB,
    [BINOP_MUL]    = IR_MUL,
    [BINOP_DIV]    = IR_DIV,
    [BINOP_MOD]    = IR_MOD,
    [BINOP_SHL]    = IR_SHL,
    [BINOP_SHR]    = IR_SHR,
    [BINOP_BITAND] = IR_AND,
    [BINOP_BITXOR] = IR_XOR,
    [BINOP_BITOR]  = IR_OR,
    [BINOP_EQ]     = IR_CMPEQ,
    [BINOP_NEQ]    = IR_CMPNE,
    [BINOP_LOGAND] = IR_CMPEQ, /* unreachable */
    [BINOP_LOGOR]  = IR_CMPEQ, /* unreachable */
    [BINOP_LT]     = IR_CMPLT,
    [BINOP_GT]     = IR_CMPGT,
    [BINOP_LE]     = IR_CMPLE,
    [BINOP_GE]     = IR_CMPGE,
};

/*
 * Perform a binary arithmetic or comparison operation.
 * Operand types are validated and the appropriate IR instruction
 * is emitted on success.
 */
type_kind_t check_binary(expr_t *left, expr_t *right, symtable_t *vars,
                         symtable_t *funcs, ir_builder_t *ir,
                         ir_value_t *out, binop_t op)
{
    ir_value_t lval, rval;
    type_kind_t lt = check_expr(left, vars, funcs, ir, &lval);
    type_kind_t rt = check_expr(right, vars, funcs, ir, &rval);
    if (is_complexlike(lt) && lt == rt &&
        (op == BINOP_ADD || op == BINOP_SUB || op == BINOP_MUL || op == BINOP_DIV)) {
        if (out) {
            ir_op_t cop = IR_CPLX_ADD;
            switch (op) {
            case BINOP_ADD: cop = IR_CPLX_ADD; break;
            case BINOP_SUB: cop = IR_CPLX_SUB; break;
            case BINOP_MUL: cop = IR_CPLX_MUL; break;
            case BINOP_DIV: cop = IR_CPLX_DIV; break;
            default: break;
            }
            *out = ir_build_binop(ir, cop, lval, rval);
        }
        return lt;
    } else if (is_floatlike(lt) && lt == rt &&
               (op == BINOP_ADD || op == BINOP_SUB || op == BINOP_MUL || op == BINOP_DIV)) {
        if (out) {
            ir_op_t fop = IR_FADD;
            switch (op) {
            case BINOP_ADD: fop = (lt == TYPE_LDOUBLE) ? IR_LFADD : IR_FADD; break;
            case BINOP_SUB: fop = (lt == TYPE_LDOUBLE) ? IR_LFSUB : IR_FSUB; break;
            case BINOP_MUL: fop = (lt == TYPE_LDOUBLE) ? IR_LFMUL : IR_FMUL; break;
            case BINOP_DIV: fop = (lt == TYPE_LDOUBLE) ? IR_LFDIV : IR_FDIV; break;
            default: break;
            }
            *out = ir_build_binop(ir, fop, lval, rval);
        }
        return lt;
    } else if (is_intlike(lt) && is_intlike(rt)) {
        if (out) {
            ir_op_t ir_op = binop_to_ir[op];
            *out = ir_build_binop(ir, ir_op, lval, rval);
        }
        if (lt == TYPE_LLONG || lt == TYPE_ULLONG ||
            rt == TYPE_LLONG || rt == TYPE_ULLONG)
            return TYPE_LLONG;
        return TYPE_INT;
    } else if ((lt == TYPE_PTR && is_intlike(rt) &&
                (op == BINOP_ADD || op == BINOP_SUB)) ||
               (is_intlike(lt) && rt == TYPE_PTR && op == BINOP_ADD)) {
        ir_value_t ptr = (lt == TYPE_PTR) ? lval : rval;
        ir_value_t idx = (lt == TYPE_PTR) ? rval : lval;
        size_t esz = 4;
        expr_t *ptexpr = (lt == TYPE_PTR) ? left : right;
        if (ptexpr->kind == EXPR_IDENT) {
            symbol_t *s = symtable_lookup(vars, ptexpr->ident.name);
            if (s && s->elem_size)
                esz = s->elem_size;
        }
        if (op == BINOP_SUB && lt == TYPE_PTR) {
            ir_value_t zero = ir_build_const(ir, 0);
            idx = ir_build_binop(ir, IR_SUB, zero, idx);
        }
        if (out)
            *out = ir_build_ptr_add(ir, ptr, idx, (int)esz);
        return TYPE_PTR;
    } else if (lt == TYPE_PTR && rt == TYPE_PTR && op == BINOP_SUB) {
        size_t esz = 4;
        if (left->kind == EXPR_IDENT) {
            symbol_t *s = symtable_lookup(vars, left->ident.name);
            if (s && s->elem_size)
                esz = s->elem_size;
        }
        if (out)
            *out = ir_build_ptr_diff(ir, lval, rval, (int)esz);
        return TYPE_INT;
    } else if (lt == TYPE_PTR && rt == TYPE_PTR &&
               (op == BINOP_EQ || op == BINOP_NEQ ||
                op == BINOP_LT || op == BINOP_GT ||
                op == BINOP_LE || op == BINOP_GE)) {
        if (out) {
            ir_op_t ir_op = binop_to_ir[op];
            *out = ir_build_binop(ir, ir_op, lval, rval);
        }
        return TYPE_INT;
    }
    error_set(left->line, left->column, error_current_file, error_current_function);
    return TYPE_UNKNOWN;
}

/*
 * Validate unary arithmetic and pointer operators.
 * Ensures operands are of the correct type and emits IR for the
 * resulting value or address manipulation.
 */

/* Handle pointer dereference operator. */
static type_kind_t unary_deref(expr_t *opnd, symtable_t *vars,
                               symtable_t *funcs, ir_builder_t *ir,
                               ir_value_t *out)
{
    ir_value_t addr;
    if (check_expr(opnd, vars, funcs, ir, &addr) == TYPE_PTR) {
        if (out) {
            int restr = 0;
            if (opnd->kind == EXPR_IDENT) {
                symbol_t *s = symtable_lookup(vars, opnd->ident.name);
                restr = s ? s->is_restrict : 0;
            }
            *out = restr ? ir_build_load_ptr_res(ir, addr)
                         : ir_build_load_ptr(ir, addr);
        }
        return TYPE_INT;
    }
    error_set(opnd->line, opnd->column, error_current_file, error_current_function);
    return TYPE_UNKNOWN;
}

/* Obtain the address of an identifier operand. */
static type_kind_t unary_addr(expr_t *opnd, symtable_t *vars,
                              symtable_t *funcs, ir_builder_t *ir,
                              ir_value_t *out)
{
    (void)funcs;
    if (opnd->kind != EXPR_IDENT) {
        error_set(opnd->line, opnd->column, error_current_file, error_current_function);
        return TYPE_UNKNOWN;
    }
    symbol_t *sym = symtable_lookup(vars, opnd->ident.name);
    if (!sym) {
        error_set(opnd->line, opnd->column, error_current_file, error_current_function);
        return TYPE_UNKNOWN;
    }
    if (out)
        *out = ir_build_addr(ir, sym->ir_name);
    return TYPE_PTR;
}

/* Negate an integer or floating point operand. */
static type_kind_t unary_neg(expr_t *opnd, symtable_t *vars,
                             symtable_t *funcs, ir_builder_t *ir,
                             ir_value_t *out)
{
    ir_value_t val;
    type_kind_t vt = check_expr(opnd, vars, funcs, ir, &val);
    if (is_intlike(vt)) {
        if (out) {
            ir_value_t zero = ir_build_const(ir, 0);
            *out = ir_build_binop(ir, IR_SUB, zero, val);
        }
        return (vt == TYPE_LLONG || vt == TYPE_ULLONG) ? TYPE_LLONG : TYPE_INT;
    } else if (is_floatlike(vt)) {
        if (out) {
            ir_value_t zero = ir_build_const(ir, 0);
            ir_op_t op = (vt == TYPE_LDOUBLE) ? IR_LFSUB : IR_FSUB;
            *out = ir_build_binop(ir, op, zero, val);
        }
        return vt;
    }
    error_set(opnd->line, opnd->column, error_current_file, error_current_function);
    return TYPE_UNKNOWN;
}

/* Logical negation of an integer operand. */
static type_kind_t unary_not(expr_t *opnd, symtable_t *vars,
                             symtable_t *funcs, ir_builder_t *ir,
                             ir_value_t *out)
{
    ir_value_t val;
    if (is_intlike(check_expr(opnd, vars, funcs, ir, &val))) {
        if (out) {
            ir_value_t zero = ir_build_const(ir, 0);
            *out = ir_build_binop(ir, IR_CMPEQ, val, zero);
        }
        return TYPE_INT;
    }
    error_set(opnd->line, opnd->column, error_current_file, error_current_function);
    return TYPE_UNKNOWN;
}

/* Pre/post increment and decrement of an identifier. */
static type_kind_t unary_incdec(expr_t *expr, symtable_t *vars,
                                symtable_t *funcs, ir_builder_t *ir,
                                ir_value_t *out)
{
    expr_t *opnd = expr->unary.operand;
    (void)funcs;
    if (opnd->kind != EXPR_IDENT) {
        error_set(opnd->line, opnd->column, error_current_file, error_current_function);
        return TYPE_UNKNOWN;
    }

    symbol_t *sym = symtable_lookup(vars, opnd->ident.name);
    if (!sym || !(is_intlike(sym->type) || is_floatlike(sym->type) ||
                  sym->type == TYPE_PTR)) {
        error_set(opnd->line, opnd->column, error_current_file, error_current_function);
        return TYPE_UNKNOWN;
    }

    ir_value_t cur = sym->param_index >= 0
                         ? ir_build_load_param(ir, sym->param_index)
                         : (sym->is_volatile ? ir_build_load_vol(ir, sym->ir_name)
                                             : ir_build_load(ir, sym->ir_name));

    if (sym->type == TYPE_PTR) {
        int esz = sym->elem_size ? (int)sym->elem_size : 4;
        int step = (expr->unary.op == UNOP_PREDEC ||
                    expr->unary.op == UNOP_POSTDEC)
                       ? -1
                       : 1;
        ir_value_t idx = ir_build_const(ir, step);
        ir_value_t upd = ir_build_ptr_add(ir, cur, idx, esz);
        if (sym->param_index >= 0)
            ir_build_store_param(ir, sym->param_index, upd);
        else if (sym->is_volatile)
            ir_build_store_vol(ir, sym->ir_name, upd);
        else
            ir_build_store(ir, sym->ir_name, upd);
        if (out)
            *out = (expr->unary.op == UNOP_PREINC ||
                    expr->unary.op == UNOP_PREDEC)
                       ? upd
                       : cur;
        return TYPE_PTR;
    }

    ir_value_t one = ir_build_const(ir, 1);
    ir_op_t ir_op;
    if (is_floatlike(sym->type))
        ir_op = (expr->unary.op == UNOP_PREDEC ||
                 expr->unary.op == UNOP_POSTDEC)
                    ? (sym->type == TYPE_LDOUBLE ? IR_LFSUB : IR_FSUB)
                    : (sym->type == TYPE_LDOUBLE ? IR_LFADD : IR_FADD);
    else
        ir_op = (expr->unary.op == UNOP_PREDEC ||
                 expr->unary.op == UNOP_POSTDEC)
                    ? IR_SUB
                    : IR_ADD;
    ir_value_t upd = ir_build_binop(ir, ir_op, cur, one);
    if (sym->param_index >= 0)
        ir_build_store_param(ir, sym->param_index, upd);
    else if (sym->is_volatile)
        ir_build_store_vol(ir, sym->ir_name, upd);
    else
        ir_build_store(ir, sym->ir_name, upd);
    if (out)
        *out = (expr->unary.op == UNOP_PREINC ||
                expr->unary.op == UNOP_PREDEC)
                   ? upd
                   : cur;
    return sym->type;
}

type_kind_t check_unary_expr(expr_t *expr, symtable_t *vars,
                             symtable_t *funcs, ir_builder_t *ir,
                             ir_value_t *out)
{
    switch (expr->unary.op) {
    case UNOP_DEREF:
        return unary_deref(expr->unary.operand, vars, funcs, ir, out);
    case UNOP_ADDR:
        return unary_addr(expr->unary.operand, vars, funcs, ir, out);
    case UNOP_NEG:
        return unary_neg(expr->unary.operand, vars, funcs, ir, out);
    case UNOP_NOT:
        return unary_not(expr->unary.operand, vars, funcs, ir, out);
    case UNOP_PREINC: case UNOP_PREDEC:
    case UNOP_POSTINC: case UNOP_POSTDEC:
        return unary_incdec(expr, vars, funcs, ir, out);
    default:
        return TYPE_UNKNOWN;
    }
}

/*
 * Validate a binary expression.  Logical operations are handled
 * separately while other operators delegate to check_binary to
 * verify operand types and emit the IR instruction.
 */
type_kind_t check_binary_expr(expr_t *expr, symtable_t *vars,
                              symtable_t *funcs, ir_builder_t *ir,
                              ir_value_t *out)
{
    if (expr->binary.op == BINOP_LOGAND || expr->binary.op == BINOP_LOGOR) {
        ir_value_t lval, rval;
        if (!is_intlike(check_expr(expr->binary.left, vars, funcs, ir, &lval))) {
            error_set(expr->binary.left->line, expr->binary.left->column, error_current_file, error_current_function);
            return TYPE_UNKNOWN;
        }
        if (!is_intlike(check_expr(expr->binary.right, vars, funcs, ir, &rval))) {
            error_set(expr->binary.right->line, expr->binary.right->column, error_current_file, error_current_function);
            return TYPE_UNKNOWN;
        }
        if (out) {
            if (expr->binary.op == BINOP_LOGAND)
                *out = ir_build_logand(ir, lval, rval);
            else
                *out = ir_build_logor(ir, lval, rval);
        }
        return TYPE_INT;
    }
    return check_binary(expr->binary.left, expr->binary.right, vars, funcs,
                        ir, out, expr->binary.op);
}

