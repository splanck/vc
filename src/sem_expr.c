#include <stdlib.h>
#include <string.h>
#include "semantic.h"

static int is_intlike(type_kind_t t)
{
    return t == TYPE_INT || t == TYPE_CHAR;
}

/* Evaluate a constant expression at compile time. Returns non-zero on success. */
int eval_const_expr(expr_t *expr, int *out)
{
    if (!expr)
        return 0;
    switch (expr->kind) {
    case EXPR_NUMBER:
        if (out)
            *out = (int)strtol(expr->number.value, NULL, 10);
        return 1;
    case EXPR_CHAR:
        if (out)
            *out = (int)expr->ch.value;
        return 1;
    case EXPR_UNARY:
        if (expr->unary.op == UNOP_NEG) {
            int val;
            if (eval_const_expr(expr->unary.operand, &val)) {
                if (out)
                    *out = -val;
                return 1;
            }
        }
        return 0;
    case EXPR_BINARY: {
        int a, b;
        if (!eval_const_expr(expr->binary.left, &a) ||
            !eval_const_expr(expr->binary.right, &b))
            return 0;
        switch (expr->binary.op) {
        case BINOP_ADD: if (out) *out = a + b; break;
        case BINOP_SUB: if (out) *out = a - b; break;
        case BINOP_MUL: if (out) *out = a * b; break;
        case BINOP_DIV: if (out) *out = b != 0 ? a / b : 0; break;
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
    default:
        return 0;
    }
}

static type_kind_t check_binary(expr_t *left, expr_t *right, symtable_t *vars,
                                symtable_t *funcs, ir_builder_t *ir,
                                ir_value_t *out, binop_t op)
{
    ir_value_t lval, rval;
    type_kind_t lt = check_expr(left, vars, funcs, ir, &lval);
    type_kind_t rt = check_expr(right, vars, funcs, ir, &rval);
    if (is_intlike(lt) && is_intlike(rt)) {
        if (out) {
            ir_op_t ir_op;
            switch (op) {
            case BINOP_ADD: ir_op = IR_ADD; break;
            case BINOP_SUB: ir_op = IR_SUB; break;
            case BINOP_MUL: ir_op = IR_MUL; break;
            case BINOP_DIV: ir_op = IR_DIV; break;
            case BINOP_EQ:  ir_op = IR_CMPEQ; break;
            case BINOP_NEQ: ir_op = IR_CMPNE; break;
            case BINOP_LT:  ir_op = IR_CMPLT; break;
            case BINOP_GT:  ir_op = IR_CMPGT; break;
            case BINOP_LE:  ir_op = IR_CMPLE; break;
            case BINOP_GE:  ir_op = IR_CMPGE; break;
            }
            *out = ir_build_binop(ir, ir_op, lval, rval);
        }
        return TYPE_INT;
    } else if ((lt == TYPE_PTR && is_intlike(rt) &&
                (op == BINOP_ADD || op == BINOP_SUB)) ||
               (is_intlike(lt) && rt == TYPE_PTR && op == BINOP_ADD)) {
        ir_value_t ptr = (lt == TYPE_PTR) ? lval : rval;
        ir_value_t idx = (lt == TYPE_PTR) ? rval : lval;
        if (op == BINOP_SUB && lt == TYPE_PTR) {
            ir_value_t zero = ir_build_const(ir, 0);
            idx = ir_build_binop(ir, IR_SUB, zero, idx);
        }
        if (out)
            *out = ir_build_binop(ir, IR_PTR_ADD, ptr, idx);
        return TYPE_PTR;
    } else if (lt == TYPE_PTR && rt == TYPE_PTR && op == BINOP_SUB) {
        if (out)
            *out = ir_build_binop(ir, IR_PTR_DIFF, lval, rval);
        return TYPE_INT;
    }
    semantic_set_error(left->line, left->column);
    return TYPE_UNKNOWN;
}

type_kind_t check_expr(expr_t *expr, symtable_t *vars, symtable_t *funcs,
                       ir_builder_t *ir, ir_value_t *out)
{
    if (!expr)
        return TYPE_UNKNOWN;
    switch (expr->kind) {
    case EXPR_NUMBER:
        if (out)
            *out = ir_build_const(ir, (int)strtol(expr->number.value, NULL, 10));
        return TYPE_INT;
    case EXPR_STRING:
        if (out)
            *out = ir_build_string(ir, expr->string.value);
        return TYPE_INT;
    case EXPR_CHAR:
        if (out)
            *out = ir_build_const(ir, (int)expr->ch.value);
        return TYPE_CHAR;
    case EXPR_UNARY:
        if (expr->unary.op == UNOP_DEREF) {
            ir_value_t addr;
            if (check_expr(expr->unary.operand, vars, funcs, ir, &addr) == TYPE_PTR) {
                if (out)
                    *out = ir_build_load_ptr(ir, addr);
                return TYPE_INT;
            }
            semantic_set_error(expr->unary.operand->line, expr->unary.operand->column);
            return TYPE_UNKNOWN;
        } else if (expr->unary.op == UNOP_ADDR) {
            if (expr->unary.operand->kind != EXPR_IDENT) {
                semantic_set_error(expr->unary.operand->line, expr->unary.operand->column);
                return TYPE_UNKNOWN;
            }
            symbol_t *sym = symtable_lookup(vars, expr->unary.operand->ident.name);
            if (!sym) {
                semantic_set_error(expr->unary.operand->line, expr->unary.operand->column);
                return TYPE_UNKNOWN;
            }
            if (out)
                *out = ir_build_addr(ir, sym->name);
            return TYPE_PTR;
        } else if (expr->unary.op == UNOP_NEG) {
            ir_value_t val;
            if (is_intlike(check_expr(expr->unary.operand, vars, funcs, ir, &val))) {
                if (out) {
                    ir_value_t zero = ir_build_const(ir, 0);
                    *out = ir_build_binop(ir, IR_SUB, zero, val);
                }
                return TYPE_INT;
            }
            semantic_set_error(expr->unary.operand->line, expr->unary.operand->column);
            return TYPE_UNKNOWN;
        }
        return TYPE_UNKNOWN;
    case EXPR_IDENT: {
        symbol_t *sym = symtable_lookup(vars, expr->ident.name);
        if (!sym) {
            semantic_set_error(expr->line, expr->column);
            return TYPE_UNKNOWN;
        }
        if (sym->type == TYPE_ARRAY) {
            if (out)
                *out = ir_build_addr(ir, sym->name);
            return TYPE_PTR;
        } else {
            if (out) {
                if (sym->param_index >= 0)
                    *out = ir_build_load_param(ir, sym->param_index);
                else
                    *out = ir_build_load(ir, expr->ident.name);
            }
            return sym->type;
        }
    }
    case EXPR_BINARY:
        return check_binary(expr->binary.left, expr->binary.right, vars, funcs,
                           ir, out, expr->binary.op);
    case EXPR_ASSIGN: {
        ir_value_t val;
        symbol_t *sym = symtable_lookup(vars, expr->assign.name);
        if (!sym) {
            semantic_set_error(expr->line, expr->column);
            return TYPE_UNKNOWN;
        }
        type_kind_t vt = check_expr(expr->assign.value, vars, funcs, ir, &val);
        if ((sym->type == TYPE_CHAR && is_intlike(vt)) || vt == sym->type) {
            if (sym->param_index >= 0)
                ir_build_store_param(ir, sym->param_index, val);
            else
                ir_build_store(ir, expr->assign.name, val);
            if (out)
                *out = val;
            return sym->type;
        }
        semantic_set_error(expr->line, expr->column);
        return TYPE_UNKNOWN;
    }
    case EXPR_INDEX: {
        if (expr->index.array->kind != EXPR_IDENT) {
            semantic_set_error(expr->line, expr->column);
            return TYPE_UNKNOWN;
        }
        symbol_t *sym = symtable_lookup(vars, expr->index.array->ident.name);
        if (!sym || sym->type != TYPE_ARRAY) {
            semantic_set_error(expr->line, expr->column);
            return TYPE_UNKNOWN;
        }
        ir_value_t idx_val;
        if (check_expr(expr->index.index, vars, funcs, ir, &idx_val) != TYPE_INT) {
            semantic_set_error(expr->index.index->line, expr->index.index->column);
            return TYPE_UNKNOWN;
        }
        int cval;
        if (sym->array_size && eval_const_expr(expr->index.index, &cval)) {
            if (cval < 0 || (size_t)cval >= sym->array_size) {
                semantic_set_error(expr->index.index->line, expr->index.index->column);
                return TYPE_UNKNOWN;
            }
        }
        if (out)
            *out = ir_build_load_idx(ir, sym->name, idx_val);
        return TYPE_INT;
    }
    case EXPR_ASSIGN_INDEX: {
        if (expr->assign_index.array->kind != EXPR_IDENT) {
            semantic_set_error(expr->line, expr->column);
            return TYPE_UNKNOWN;
        }
        symbol_t *sym = symtable_lookup(vars, expr->assign_index.array->ident.name);
        if (!sym || sym->type != TYPE_ARRAY) {
            semantic_set_error(expr->line, expr->column);
            return TYPE_UNKNOWN;
        }
        ir_value_t idx_val, val;
        if (check_expr(expr->assign_index.index, vars, funcs, ir, &idx_val) != TYPE_INT) {
            semantic_set_error(expr->assign_index.index->line, expr->assign_index.index->column);
            return TYPE_UNKNOWN;
        }
        if (check_expr(expr->assign_index.value, vars, funcs, ir, &val) != TYPE_INT) {
            semantic_set_error(expr->assign_index.value->line, expr->assign_index.value->column);
            return TYPE_UNKNOWN;
        }
        int cval;
        if (sym->array_size && eval_const_expr(expr->assign_index.index, &cval)) {
            if (cval < 0 || (size_t)cval >= sym->array_size) {
                semantic_set_error(expr->assign_index.index->line, expr->assign_index.index->column);
                return TYPE_UNKNOWN;
            }
        }
        ir_build_store_idx(ir, sym->name, idx_val, val);
        if (out)
            *out = val;
        return TYPE_INT;
    }
    case EXPR_CALL: {
        symbol_t *fsym = symtable_lookup(funcs, expr->call.name);
        if (!fsym) {
            semantic_set_error(expr->line, expr->column);
            return TYPE_UNKNOWN;
        }
        if (fsym->param_count != expr->call.arg_count) {
            semantic_set_error(expr->line, expr->column);
            return TYPE_UNKNOWN;
        }
        ir_value_t *vals = NULL;
        if (expr->call.arg_count) {
            vals = malloc(expr->call.arg_count * sizeof(*vals));
            if (!vals)
                return TYPE_UNKNOWN;
        }
        for (size_t i = 0; i < expr->call.arg_count; i++) {
            type_kind_t at = check_expr(expr->call.args[i], vars, funcs, ir,
                                        &vals[i]);
            type_kind_t pt = fsym->param_types[i];
            if (!((pt == TYPE_CHAR && is_intlike(at)) || at == pt)) {
                semantic_set_error(expr->call.args[i]->line, expr->call.args[i]->column);
                free(vals);
                return TYPE_UNKNOWN;
            }
        }
        for (size_t i = expr->call.arg_count; i > 0; i--)
            ir_build_arg(ir, vals[i - 1]);
        free(vals);
        ir_value_t call_val = ir_build_call(ir, expr->call.name,
                                           expr->call.arg_count);
        if (out)
            *out = call_val;
        return fsym->type;
    }
    }
    semantic_set_error(expr->line, expr->column);
    return TYPE_UNKNOWN;
}
