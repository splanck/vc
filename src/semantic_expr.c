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




type_kind_t check_binary(expr_t *left, expr_t *right, symtable_t *vars,
                                symtable_t *funcs, ir_builder_t *ir,
                                ir_value_t *out, binop_t op)
{
    ir_value_t lval, rval;
    type_kind_t lt = check_expr(left, vars, funcs, ir, &lval);
    type_kind_t rt = check_expr(right, vars, funcs, ir, &rval);
    if (is_floatlike(lt) && lt == rt &&
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
    }
    error_set(left->line, left->column);
    return TYPE_UNKNOWN;
}

type_kind_t check_unary_expr(expr_t *expr, symtable_t *vars,
                                    symtable_t *funcs, ir_builder_t *ir,
                                    ir_value_t *out)
{
    expr_t *opnd = expr->unary.operand;
    switch (expr->unary.op) {
    case UNOP_DEREF: {
        ir_value_t addr;
        if (check_expr(opnd, vars, funcs, ir, &addr) == TYPE_PTR) {
            if (out)
                *out = ir_build_load_ptr(ir, addr);
            return TYPE_INT;
        }
        error_set(opnd->line, opnd->column);
        return TYPE_UNKNOWN;
    }
    case UNOP_ADDR:
        if (opnd->kind != EXPR_IDENT) {
            error_set(opnd->line, opnd->column);
            return TYPE_UNKNOWN;
        }
        {
            symbol_t *sym = symtable_lookup(vars, opnd->ident.name);
            if (!sym) {
                error_set(opnd->line, opnd->column);
                return TYPE_UNKNOWN;
            }
            if (out)
                *out = ir_build_addr(ir, sym->ir_name);
            return TYPE_PTR;
        }
    case UNOP_NEG: {
        ir_value_t val;
        type_kind_t vt = check_expr(opnd, vars, funcs, ir, &val);
        if (is_intlike(vt)) {
            if (out) {
                ir_value_t zero = ir_build_const(ir, 0);
                *out = ir_build_binop(ir, IR_SUB, zero, val);
            }
            return vt == TYPE_LLONG || vt == TYPE_ULLONG ? TYPE_LLONG : TYPE_INT;
        } else if (is_floatlike(vt)) {
            if (out) {
                ir_value_t zero = ir_build_const(ir, 0);
                ir_op_t op = (vt == TYPE_LDOUBLE) ? IR_LFSUB : IR_FSUB;
                *out = ir_build_binop(ir, op, zero, val);
            }
            return vt;
        }
        error_set(opnd->line, opnd->column);
        return TYPE_UNKNOWN;
    }
    case UNOP_NOT: {
        ir_value_t val;
        if (is_intlike(check_expr(opnd, vars, funcs, ir, &val))) {
            if (out) {
                ir_value_t zero = ir_build_const(ir, 0);
                *out = ir_build_binop(ir, IR_CMPEQ, val, zero);
            }
            return TYPE_INT;
        }
        error_set(opnd->line, opnd->column);
        return TYPE_UNKNOWN;
    }
    case UNOP_PREINC: case UNOP_PREDEC:
    case UNOP_POSTINC: case UNOP_POSTDEC:
        if (opnd->kind != EXPR_IDENT) {
            error_set(opnd->line, opnd->column);
            return TYPE_UNKNOWN;
        }
        {
            symbol_t *sym = symtable_lookup(vars, opnd->ident.name);
            if (!sym ||
                !(is_intlike(sym->type) || is_floatlike(sym->type) ||
                  sym->type == TYPE_PTR)) {
                error_set(opnd->line, opnd->column);
                return TYPE_UNKNOWN;
            }

            ir_value_t cur = sym->param_index >= 0
                                 ? ir_build_load_param(ir, sym->param_index)
                                 : (sym->is_volatile
                                        ? ir_build_load_vol(ir, sym->ir_name)
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
                         expr->unary.op == UNOP_POSTDEC) ? IR_SUB : IR_ADD;
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
    default:
        return TYPE_UNKNOWN;
    }
}

type_kind_t check_binary_expr(expr_t *expr, symtable_t *vars,
                                     symtable_t *funcs, ir_builder_t *ir,
                                     ir_value_t *out)
{
    if (expr->binary.op == BINOP_LOGAND || expr->binary.op == BINOP_LOGOR) {
        ir_value_t lval, rval;
        if (!is_intlike(check_expr(expr->binary.left, vars, funcs, ir, &lval))) {
            error_set(expr->binary.left->line, expr->binary.left->column);
            return TYPE_UNKNOWN;
        }
        if (!is_intlike(check_expr(expr->binary.right, vars, funcs, ir, &rval))) {
            error_set(expr->binary.right->line, expr->binary.right->column);
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

static type_kind_t check_index_expr(expr_t *expr, symtable_t *vars,
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

static type_kind_t check_assign_index_expr(expr_t *expr, symtable_t *vars,
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

static type_kind_t check_assign_member_expr(expr_t *expr, symtable_t *vars,
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

static type_kind_t check_member_expr(expr_t *expr, symtable_t *vars,
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

static type_kind_t check_complit_expr(expr_t *expr, symtable_t *vars,
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

static type_kind_t check_call_expr(expr_t *expr, symtable_t *vars,
                                   symtable_t *funcs, ir_builder_t *ir,
                                   ir_value_t *out)
{
    (void)vars;
    symbol_t *fsym = symtable_lookup(funcs, expr->call.name);
    if (!fsym) {
        error_set(expr->line, expr->column);
        return TYPE_UNKNOWN;
    }
    if (fsym->param_count != expr->call.arg_count) {
        error_set(expr->line, expr->column);
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
        if (!(((is_intlike(pt) && is_intlike(at)) ||
               (is_floatlike(pt) && is_floatlike(at))) || at == pt)) {
            error_set(expr->call.args[i]->line, expr->call.args[i]->column);
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
