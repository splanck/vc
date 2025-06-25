/*
 * Semantic analysis and IR generation.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "semantic.h"
#include "symtable.h"
#include "util.h"
#include "label.h"
#include "error.h"

typedef struct label_entry {
    char *name;
    char *ir_name;
    struct label_entry *next;
} label_entry_t;

typedef struct {
    label_entry_t *head;
} label_table_t;

static void label_table_init(label_table_t *t) { t->head = NULL; }

static void label_table_free(label_table_t *t)
{
    label_entry_t *e = t->head;
    while (e) {
        label_entry_t *n = e->next;
        free(e->name);
        free(e->ir_name);
        free(e);
        e = n;
    }
    t->head = NULL;
}

static const char *label_table_get(label_table_t *t, const char *name)
{
    for (label_entry_t *e = t->head; e; e = e->next) {
        if (strcmp(e->name, name) == 0)
            return e->ir_name;
    }
    return NULL;
}

static const char *label_table_get_or_add(label_table_t *t, const char *name)
{
    const char *ir = label_table_get(t, name);
    if (ir)
        return ir;
    label_entry_t *e = malloc(sizeof(*e));
    if (!e)
        return NULL;
    e->name = vc_strdup(name);
    char buf[32];
    snprintf(buf, sizeof(buf), "Luser%d", label_next_id());
    e->ir_name = vc_strdup(buf);
    e->next = t->head;
    t->head = e;
    return e->ir_name;
}

static int is_intlike(type_kind_t t)
{
    return t == TYPE_INT || t == TYPE_CHAR ||
           t == TYPE_FLOAT || t == TYPE_DOUBLE;
}

static void symtable_pop_scope(symtable_t *table, symbol_t *old_head)
{
    while (table->head != old_head) {
        symbol_t *sym = table->head;
        table->head = sym->next;
        free(sym->name);
        free(sym->param_types);
        free(sym);
    }
}


/* Evaluate a constant expression at compile time. Returns non-zero on success. */
static int eval_const_expr(expr_t *expr, symtable_t *vars, int *out)
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
            if (eval_const_expr(expr->unary.operand, vars, &val)) {
                if (out)
                    *out = -val;
                return 1;
            }
        }
        return 0;
    case EXPR_BINARY: {
        int a, b;
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
            case TYPE_INT:  sz = 4; break;
            case TYPE_CHAR: sz = 1; break;
            case TYPE_PTR:  sz = 4; break;
            case TYPE_ARRAY: sz = (int)expr->sizeof_expr.array_size * 4; break;
            default: sz = 0; break;
            }
            *out = sz;
        }
        return 1;
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
            case BINOP_MOD: ir_op = IR_MOD; break;
            case BINOP_SHL: ir_op = IR_SHL; break;
            case BINOP_SHR: ir_op = IR_SHR; break;
            case BINOP_BITAND: ir_op = IR_AND; break;
            case BINOP_BITXOR: ir_op = IR_XOR; break;
            case BINOP_BITOR: ir_op = IR_OR; break;
            case BINOP_EQ:  ir_op = IR_CMPEQ; break;
            case BINOP_NEQ: ir_op = IR_CMPNE; break;
            case BINOP_LT:  ir_op = IR_CMPLT; break;
            case BINOP_GT:  ir_op = IR_CMPGT; break;
            case BINOP_LE:  ir_op = IR_CMPLE; break;
            case BINOP_GE:  ir_op = IR_CMPGE; break;
            case BINOP_LOGAND: ir_op = IR_CMPEQ; break; /* unreachable */
            case BINOP_LOGOR:  ir_op = IR_CMPEQ; break; /* unreachable */
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
    error_set(left->line, left->column);
    return TYPE_UNKNOWN;
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
            error_set(expr->unary.operand->line, expr->unary.operand->column);
            return TYPE_UNKNOWN;
        } else if (expr->unary.op == UNOP_ADDR) {
            if (expr->unary.operand->kind != EXPR_IDENT) {
                error_set(expr->unary.operand->line, expr->unary.operand->column);
                return TYPE_UNKNOWN;
            }
            symbol_t *sym = symtable_lookup(vars, expr->unary.operand->ident.name);
            if (!sym) {
                error_set(expr->unary.operand->line, expr->unary.operand->column);
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
            error_set(expr->unary.operand->line, expr->unary.operand->column);
            return TYPE_UNKNOWN;
        } else if (expr->unary.op == UNOP_NOT) {
            ir_value_t val;
            if (is_intlike(check_expr(expr->unary.operand, vars, funcs, ir, &val))) {
                if (out) {
                    ir_value_t zero = ir_build_const(ir, 0);
                    *out = ir_build_binop(ir, IR_CMPEQ, val, zero);
                }
                return TYPE_INT;
            }
            error_set(expr->unary.operand->line, expr->unary.operand->column);
            return TYPE_UNKNOWN;
        } else if (expr->unary.op == UNOP_PREINC || expr->unary.op == UNOP_PREDEC ||
                   expr->unary.op == UNOP_POSTINC || expr->unary.op == UNOP_POSTDEC) {
            if (expr->unary.operand->kind != EXPR_IDENT) {
                error_set(expr->unary.operand->line, expr->unary.operand->column);
                return TYPE_UNKNOWN;
            }
            symbol_t *sym = symtable_lookup(vars, expr->unary.operand->ident.name);
            if (!sym || !is_intlike(sym->type)) {
                error_set(expr->unary.operand->line, expr->unary.operand->column);
                return TYPE_UNKNOWN;
            }
            ir_value_t cur;
            if (sym->param_index >= 0)
                cur = ir_build_load_param(ir, sym->param_index);
            else
                cur = ir_build_load(ir, sym->name);
            ir_value_t one = ir_build_const(ir, 1);
            ir_op_t op = (expr->unary.op == UNOP_PREDEC || expr->unary.op == UNOP_POSTDEC)
                            ? IR_SUB : IR_ADD;
            ir_value_t upd = ir_build_binop(ir, op, cur, one);
            if (sym->param_index >= 0)
                ir_build_store_param(ir, sym->param_index, upd);
            else
                ir_build_store(ir, sym->name, upd);
            if (out)
                *out = (expr->unary.op == UNOP_PREINC || expr->unary.op == UNOP_PREDEC) ? upd : cur;
            return sym->type;
        }
        return TYPE_UNKNOWN;
    case EXPR_IDENT: {
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
    case EXPR_ASSIGN: {
        ir_value_t val;
        symbol_t *sym = symtable_lookup(vars, expr->assign.name);
        if (!sym) {
            error_set(expr->line, expr->column);
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
        error_set(expr->line, expr->column);
        return TYPE_UNKNOWN;
    }
    case EXPR_INDEX: {
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
        int cval;
        if (sym->array_size && eval_const_expr(expr->index.index, vars, &cval)) {
            if (cval < 0 || (size_t)cval >= sym->array_size) {
                error_set(expr->index.index->line, expr->index.index->column);
                return TYPE_UNKNOWN;
            }
        }
        if (out)
            *out = ir_build_load_idx(ir, sym->name, idx_val);
        return TYPE_INT;
    }
    case EXPR_ASSIGN_INDEX: {
        if (expr->assign_index.array->kind != EXPR_IDENT) {
            error_set(expr->line, expr->column);
            return TYPE_UNKNOWN;
        }
        symbol_t *sym = symtable_lookup(vars, expr->assign_index.array->ident.name);
        if (!sym || sym->type != TYPE_ARRAY) {
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
        int cval;
        if (sym->array_size && eval_const_expr(expr->assign_index.index, vars, &cval)) {
            if (cval < 0 || (size_t)cval >= sym->array_size) {
                error_set(expr->assign_index.index->line, expr->assign_index.index->column);
                return TYPE_UNKNOWN;
            }
        }
        ir_build_store_idx(ir, sym->name, idx_val, val);
        if (out)
            *out = val;
        return TYPE_INT;
    }
    case EXPR_MEMBER:
        /* struct member access not implemented */
        error_set(expr->line, expr->column);
        return TYPE_UNKNOWN;
    case EXPR_SIZEOF: {
        int sz = 0;
        if (expr->sizeof_expr.is_type) {
            switch (expr->sizeof_expr.type) {
            case TYPE_INT:  sz = 4; break;
            case TYPE_CHAR: sz = 1; break;
            case TYPE_PTR:  sz = 4; break;
            case TYPE_ARRAY: sz = (int)expr->sizeof_expr.array_size * 4; break;
            default: sz = 0; break;
            }
        } else {
            ir_builder_t tmp; ir_builder_init(&tmp);
            type_kind_t t = check_expr(expr->sizeof_expr.expr, vars, funcs, &tmp, NULL);
            ir_builder_free(&tmp);
            if (t == TYPE_INT) sz = 4;
            else if (t == TYPE_CHAR) sz = 1;
            else if (t == TYPE_PTR) sz = 4;
            else if (t == TYPE_ARRAY) {
                symbol_t *sym = NULL;
                if (expr->sizeof_expr.expr->kind == EXPR_IDENT)
                    sym = symtable_lookup(vars, expr->sizeof_expr.expr->ident.name);
                sz = sym ? (int)sym->array_size * 4 : 4;
            }
        }
        if (out)
            *out = ir_build_const(ir, sz);
        return TYPE_INT;
    }
    case EXPR_CALL: {
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
            if (!((pt == TYPE_CHAR && is_intlike(at)) || at == pt)) {
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
    }
    error_set(expr->line, expr->column);
    return TYPE_UNKNOWN;
}

/*
 * Validate a single statement.  Loop labels are used for 'break' and
 * 'continue' targets.  Returns non-zero on success.
 */
int check_stmt(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
               void *label_tab, ir_builder_t *ir, type_kind_t func_ret_type,
               const char *break_label, const char *continue_label)
{
    label_table_t *labels = (label_table_t *)label_tab;
    if (!stmt)
        return 0;
    switch (stmt->kind) {
    case STMT_EXPR: {
        ir_value_t tmp;
        return check_expr(stmt->expr.expr, vars, funcs, ir, &tmp) != TYPE_UNKNOWN;
    }
    case STMT_RETURN: {
        if (!stmt->ret.expr) {
            if (func_ret_type != TYPE_VOID) {
                error_set(stmt->line, stmt->column);
                return 0;
            }
            ir_value_t zero = ir_build_const(ir, 0);
            ir_build_return(ir, zero);
            return 1;
        }
        ir_value_t val;
        if (check_expr(stmt->ret.expr, vars, funcs, ir, &val) == TYPE_UNKNOWN) {
            error_set(stmt->ret.expr->line, stmt->ret.expr->column);
            return 0;
        }
        ir_build_return(ir, val);
        return 1;
    }
    case STMT_IF: {
        ir_value_t cond_val;
        if (check_expr(stmt->if_stmt.cond, vars, funcs, ir, &cond_val) == TYPE_UNKNOWN)
            return 0;
        char else_label[32];
        char end_label[32];
        int id = label_next_id();
        snprintf(else_label, sizeof(else_label), "L%d_else", id);
        snprintf(end_label, sizeof(end_label), "L%d_end", id);
        const char *target = stmt->if_stmt.else_branch ? else_label : end_label;
        ir_build_bcond(ir, cond_val, target);
        if (!check_stmt(stmt->if_stmt.then_branch, vars, funcs, labels, ir, func_ret_type,
                        break_label, continue_label))
            return 0;
        if (stmt->if_stmt.else_branch) {
            ir_build_br(ir, end_label);
            ir_build_label(ir, else_label);
            if (!check_stmt(stmt->if_stmt.else_branch, vars, funcs, labels, ir, func_ret_type,
                            break_label, continue_label))
                return 0;
        }
        ir_build_label(ir, end_label);
        return 1;
    }
    case STMT_WHILE: {
        ir_value_t cond_val;
        char start_label[32];
        char end_label[32];
        int id = label_next_id();
        snprintf(start_label, sizeof(start_label), "L%d_start", id);
        snprintf(end_label, sizeof(end_label), "L%d_end", id);
        ir_build_label(ir, start_label);
        if (check_expr(stmt->while_stmt.cond, vars, funcs, ir, &cond_val) == TYPE_UNKNOWN)
            return 0;
        ir_build_bcond(ir, cond_val, end_label);
        if (!check_stmt(stmt->while_stmt.body, vars, funcs, labels, ir, func_ret_type,
                        end_label, start_label))
            return 0;
        ir_build_br(ir, start_label);
        ir_build_label(ir, end_label);
        return 1;
    }
    case STMT_DO_WHILE: {
        ir_value_t cond_val;
        char start_label[32];
        char cond_label[32];
        char end_label[32];
        int id = label_next_id();
        snprintf(start_label, sizeof(start_label), "L%d_start", id);
        snprintf(cond_label, sizeof(cond_label), "L%d_cond", id);
        snprintf(end_label, sizeof(end_label), "L%d_end", id);
        ir_build_label(ir, start_label);
        if (!check_stmt(stmt->do_while_stmt.body, vars, funcs, labels, ir, func_ret_type,
                        end_label, cond_label))
            return 0;
        ir_build_label(ir, cond_label);
        if (check_expr(stmt->do_while_stmt.cond, vars, funcs, ir, &cond_val) == TYPE_UNKNOWN)
            return 0;
        ir_build_bcond(ir, cond_val, end_label);
        ir_build_br(ir, start_label);
        ir_build_label(ir, end_label);
        return 1;
    }
    case STMT_FOR: {
        ir_value_t cond_val;
        char start_label[32];
        char end_label[32];
        int id = label_next_id();
        snprintf(start_label, sizeof(start_label), "L%d_start", id);
        snprintf(end_label, sizeof(end_label), "L%d_end", id);
        symbol_t *old_head = vars->head;
        if (stmt->for_stmt.init_decl) {
            if (!check_stmt(stmt->for_stmt.init_decl, vars, funcs, labels, ir,
                            func_ret_type, NULL, NULL)) {
                symtable_pop_scope(vars, old_head);
                return 0;
            }
        } else {
            if (check_expr(stmt->for_stmt.init, vars, funcs, ir, &cond_val) == TYPE_UNKNOWN) {
                symtable_pop_scope(vars, old_head);
                return 0; /* reuse cond_val for init but ignore value */
            }
        }
        ir_build_label(ir, start_label);
        if (check_expr(stmt->for_stmt.cond, vars, funcs, ir, &cond_val) == TYPE_UNKNOWN)
        {
            symtable_pop_scope(vars, old_head);
            return 0;
        }
        ir_build_bcond(ir, cond_val, end_label);
        char cont_label[32];
        snprintf(cont_label, sizeof(cont_label), "L%d_cont", id);
        if (!check_stmt(stmt->for_stmt.body, vars, funcs, labels, ir, func_ret_type,
                        end_label, cont_label))
        {
            symtable_pop_scope(vars, old_head);
            return 0;
        }
        ir_build_label(ir, cont_label);
        if (check_expr(stmt->for_stmt.incr, vars, funcs, ir, &cond_val) == TYPE_UNKNOWN)
        {
            symtable_pop_scope(vars, old_head);
            return 0;
        }
        ir_build_br(ir, start_label);
        ir_build_label(ir, end_label);
        symtable_pop_scope(vars, old_head);
        return 1;
    }
    case STMT_SWITCH: {
        ir_value_t expr_val;
        if (check_expr(stmt->switch_stmt.expr, vars, funcs, ir, &expr_val) == TYPE_UNKNOWN)
            return 0;
        char end_label[32];
        char default_label[32];
        int id = label_next_id();
        snprintf(end_label, sizeof(end_label), "L%d_end", id);
        snprintf(default_label, sizeof(default_label), "L%d_default", id);
        char **case_labels = calloc(stmt->switch_stmt.case_count, sizeof(char *));
        if (!case_labels)
            return 0;
        for (size_t i = 0; i < stmt->switch_stmt.case_count; i++) {
            char lbl[32];
            snprintf(lbl, sizeof(lbl), "L%d_case%zu", id, i);
            case_labels[i] = vc_strdup(lbl);
            int cval;
            if (!eval_const_expr(stmt->switch_stmt.cases[i].expr, vars, &cval)) {
                for (size_t j = 0; j <= i; j++) free(case_labels[j]);
                free(case_labels);
                error_set(stmt->switch_stmt.cases[i].expr->line,
                          stmt->switch_stmt.cases[i].expr->column);
                return 0;
            }
            ir_value_t const_val = ir_build_const(ir, cval);
            ir_value_t cmp = ir_build_binop(ir, IR_CMPEQ, expr_val, const_val);
            ir_build_bcond(ir, cmp, case_labels[i]);
        }
        if (stmt->switch_stmt.default_body)
            ir_build_br(ir, default_label);
        else
            ir_build_br(ir, end_label);
        for (size_t i = 0; i < stmt->switch_stmt.case_count; i++) {
            ir_build_label(ir, case_labels[i]);
            if (!check_stmt(stmt->switch_stmt.cases[i].body, vars, funcs, labels, ir,
                            func_ret_type, end_label, NULL)) {
                for (size_t j = 0; j < stmt->switch_stmt.case_count; j++)
                    free(case_labels[j]);
                free(case_labels);
                return 0;
            }
            ir_build_br(ir, end_label);
        }
        if (stmt->switch_stmt.default_body) {
            ir_build_label(ir, default_label);
            if (!check_stmt(stmt->switch_stmt.default_body, vars, funcs, labels, ir,
                            func_ret_type, end_label, NULL)) {
                for (size_t j = 0; j < stmt->switch_stmt.case_count; j++)
                    free(case_labels[j]);
                free(case_labels);
                return 0;
            }
        }
        ir_build_label(ir, end_label);
        for (size_t j = 0; j < stmt->switch_stmt.case_count; j++)
            free(case_labels[j]);
        free(case_labels);
        return 1;
    }
    case STMT_LABEL: {
        const char *ir_name = label_table_get_or_add(labels, stmt->label.name);
        ir_build_label(ir, ir_name);
        return 1;
    }
    case STMT_GOTO: {
        const char *ir_name = label_table_get_or_add(labels, stmt->goto_stmt.name);
        ir_build_br(ir, ir_name);
        return 1;
    }
    case STMT_BREAK:
        if (!break_label) {
            error_set(stmt->line, stmt->column);
            return 0;
        }
        ir_build_br(ir, break_label);
        return 1;
    case STMT_CONTINUE:
        if (!continue_label) {
            error_set(stmt->line, stmt->column);
            return 0;
        }
        ir_build_br(ir, continue_label);
        return 1;
    case STMT_BLOCK: {
        symbol_t *old_head = vars->head;
        for (size_t i = 0; i < stmt->block.count; i++) {
            if (!check_stmt(stmt->block.stmts[i], vars, funcs, labels, ir, func_ret_type,
                            break_label, continue_label)) {
                symtable_pop_scope(vars, old_head);
                return 0;
            }
        }
        symtable_pop_scope(vars, old_head);
        return 1;
    }
    case STMT_ENUM_DECL: {
        int next = 0;
        for (size_t i = 0; i < stmt->enum_decl.count; i++) {
            enumerator_t *e = &stmt->enum_decl.items[i];
            int val = next;
            if (e->value) {
                if (!eval_const_expr(e->value, vars, &val)) {
                    error_set(e->value->line, e->value->column);
                    return 0;
                }
            }
            if (!symtable_add_enum(vars, e->name, val)) {
                error_set(stmt->line, stmt->column);
                return 0;
            }
            next = val + 1;
        }
        return 1;
    }
    case STMT_TYPEDEF: {
        if (!symtable_add_typedef(vars, stmt->typedef_decl.name,
                                  stmt->typedef_decl.type,
                                  stmt->typedef_decl.array_size)) {
            error_set(stmt->line, stmt->column);
            return 0;
        }
        return 1;
    }
    case STMT_VAR_DECL: {
        if (!symtable_add(vars, stmt->var_decl.name, stmt->var_decl.type,
                          stmt->var_decl.array_size)) {
            error_set(stmt->line, stmt->column);
            return 0;
        }
        if (stmt->var_decl.init) {
            ir_value_t val;
            type_kind_t vt = check_expr(stmt->var_decl.init, vars, funcs, ir, &val);
            if (!((stmt->var_decl.type == TYPE_CHAR && is_intlike(vt)) ||
                  vt == stmt->var_decl.type)) {
                error_set(stmt->var_decl.init->line, stmt->var_decl.init->column);
                return 0;
            }
            ir_build_store(ir, stmt->var_decl.name, val);
        } else if (stmt->var_decl.init_list) {
            if (stmt->var_decl.type != TYPE_ARRAY ||
                stmt->var_decl.array_size < stmt->var_decl.init_count) {
                error_set(stmt->line, stmt->column);
                return 0;
            }
            for (size_t i = 0; i < stmt->var_decl.init_count; i++) {
                int v;
                if (!eval_const_expr(stmt->var_decl.init_list[i], vars, &v)) {
                    error_set(stmt->var_decl.init_list[i]->line,
                              stmt->var_decl.init_list[i]->column);
                    return 0;
                }
                ir_value_t idx = ir_build_const(ir, (int)i);
                ir_value_t val = ir_build_const(ir, v);
                ir_build_store_idx(ir, stmt->var_decl.name, idx, val);
            }
        }
        return 1;
    }
    }
    return 0;
}

/*
 * Check a full function definition.  Parameters are placed in a local symbol
 * table chained to the list of globals.  IR for the body is generated while
 * validating statements.
 */
int check_func(func_t *func, symtable_t *funcs, symtable_t *globals,
               ir_builder_t *ir)
{
    if (!func)
        return 0;

    symtable_t locals;
    symtable_init(&locals);
    locals.globals = globals ? globals->globals : NULL;

    for (size_t i = 0; i < func->param_count; i++)
        symtable_add_param(&locals, func->param_names[i],
                           func->param_types[i], (int)i);

    ir_build_func_begin(ir, func->name);

    label_table_t labels;
    label_table_init(&labels);

    int ok = 1;
    for (size_t i = 0; i < func->body_count && ok; i++)
        ok = check_stmt(func->body[i], &locals, funcs, &labels, ir, func->return_type,
                        NULL, NULL);

    ir_build_func_end(ir);

    label_table_free(&labels);
    locals.globals = NULL;
    symtable_free(&locals);
    return ok;
}

/*
 * Validate a global variable declaration and emit the IR needed to
 * initialize it.  Only constant expressions are permitted for globals.
 */
int check_global(stmt_t *decl, symtable_t *globals, ir_builder_t *ir)
{
    if (!decl)
        return 0;
    if (decl->kind == STMT_ENUM_DECL) {
        int next = 0;
        for (size_t i = 0; i < decl->enum_decl.count; i++) {
            enumerator_t *e = &decl->enum_decl.items[i];
            int val = next;
            if (e->value) {
                if (!eval_const_expr(e->value, globals, &val)) {
                    error_set(e->value->line, e->value->column);
                    return 0;
                }
            }
            if (!symtable_add_enum_global(globals, e->name, val)) {
                error_set(decl->line, decl->column);
                return 0;
            }
            next = val + 1;
        }
        return 1;
    }
    if (decl->kind == STMT_TYPEDEF) {
        if (!symtable_add_typedef_global(globals, decl->typedef_decl.name,
                                         decl->typedef_decl.type,
                                         decl->typedef_decl.array_size)) {
            error_set(decl->line, decl->column);
            return 0;
        }
        return 1;
    }
    if (decl->kind != STMT_VAR_DECL)
        return 0;
    if (!symtable_add_global(globals, decl->var_decl.name,
                             decl->var_decl.type,
                             decl->var_decl.array_size)) {
        error_set(decl->line, decl->column);
        return 0;
    }
    if (decl->var_decl.type == TYPE_ARRAY) {
        size_t count = decl->var_decl.array_size;
        int *vals = calloc(count, sizeof(int));
        if (!vals)
            return 0;
        size_t init_count = decl->var_decl.init_count;
        if (init_count > count) {
            free(vals);
            error_set(decl->line, decl->column);
            return 0;
        }
        for (size_t i = 0; i < init_count; i++) {
            if (!eval_const_expr(decl->var_decl.init_list[i], globals, &vals[i])) {
                free(vals);
                error_set(decl->var_decl.init_list[i]->line,
                          decl->var_decl.init_list[i]->column);
                return 0;
            }
        }
        ir_build_glob_array(ir, decl->var_decl.name, vals, count);
        free(vals);
    } else {
        int value = 0;
        if (decl->var_decl.init) {
            if (!eval_const_expr(decl->var_decl.init, globals, &value)) {
                error_set(decl->var_decl.init->line, decl->var_decl.init->column);
                return 0;
            }
        }
        ir_build_glob_var(ir, decl->var_decl.name, value);
    }
    return 1;
}

