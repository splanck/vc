#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "semantic.h"

static char *dup_string(const char *s)
{
    size_t len = strlen(s);
    char *out = malloc(len + 1);
    if (!out)
        return NULL;
    memcpy(out, s, len + 1);
    return out;
}

static int next_label_id = 0;

void symtable_init(symtable_t *table)
{
    table->head = NULL;
}

void symtable_free(symtable_t *table)
{
    symbol_t *sym = table->head;
    while (sym) {
        symbol_t *next = sym->next;
        free(sym->name);
        free(sym);
        sym = next;
    }
    table->head = NULL;
}

symbol_t *symtable_lookup(symtable_t *table, const char *name)
{
    for (symbol_t *sym = table->head; sym; sym = sym->next) {
        if (strcmp(sym->name, name) == 0)
            return sym;
    }
    return NULL;
}

int symtable_add(symtable_t *table, const char *name, type_kind_t type)
{
    if (symtable_lookup(table, name))
        return 0;
    symbol_t *sym = malloc(sizeof(*sym));
    if (!sym)
        return 0;
    sym->name = dup_string(name ? name : "");
    if (!sym->name) {
        free(sym);
        return 0;
    }
    sym->type = type;
    sym->next = table->head;
    table->head = sym;
    return 1;
}

static type_kind_t check_binary(expr_t *left, expr_t *right, symtable_t *vars,
                                symtable_t *funcs, ir_builder_t *ir,
                                ir_value_t *out, binop_t op)
{
    ir_value_t lval, rval;
    type_kind_t lt = check_expr(left, vars, funcs, ir, &lval);
    type_kind_t rt = check_expr(right, vars, funcs, ir, &rval);
    if (lt == TYPE_INT && rt == TYPE_INT) {
        if (out) {
            ir_op_t ir_op;
            switch (op) {
            case BINOP_ADD: ir_op = IR_ADD; break;
            case BINOP_SUB: ir_op = IR_SUB; break;
            case BINOP_MUL: ir_op = IR_MUL; break;
            case BINOP_DIV: ir_op = IR_DIV; break;
            }
            *out = ir_build_binop(ir, ir_op, lval, rval);
        }
        return TYPE_INT;
    }
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
        return TYPE_INT;
    case EXPR_UNARY:
        if (expr->unary.op == UNOP_DEREF) {
            ir_value_t addr;
            if (check_expr(expr->unary.operand, vars, funcs, ir, &addr) == TYPE_PTR) {
                if (out)
                    *out = ir_build_load_ptr(ir, addr);
                return TYPE_INT;
            }
            return TYPE_UNKNOWN;
        } else if (expr->unary.op == UNOP_ADDR) {
            if (expr->unary.operand->kind != EXPR_IDENT)
                return TYPE_UNKNOWN;
            symbol_t *sym = symtable_lookup(vars, expr->unary.operand->ident.name);
            if (!sym)
                return TYPE_UNKNOWN;
            if (out)
                *out = ir_build_addr(ir, sym->name);
            return TYPE_PTR;
        }
        return TYPE_UNKNOWN;
    case EXPR_IDENT: {
        symbol_t *sym = symtable_lookup(vars, expr->ident.name);
        if (!sym)
            return TYPE_UNKNOWN;
        if (out)
            *out = ir_build_load(ir, expr->ident.name);
        return sym->type;
    }
    case EXPR_BINARY:
        return check_binary(expr->binary.left, expr->binary.right, vars, funcs,
                           ir, out, expr->binary.op);
    case EXPR_ASSIGN: {
        ir_value_t val;
        symbol_t *sym = symtable_lookup(vars, expr->assign.name);
        if (!sym)
            return TYPE_UNKNOWN;
        if (check_expr(expr->assign.value, vars, funcs, ir, &val) == sym->type) {
            ir_build_store(ir, expr->assign.name, val);
            if (out)
                *out = val;
            return sym->type;
        }
        return TYPE_UNKNOWN;
    }
    case EXPR_CALL: {
        symbol_t *fsym = symtable_lookup(funcs, expr->call.name);
        if (!fsym)
            return TYPE_UNKNOWN;
        if (out)
            *out = ir_build_call(ir, expr->call.name);
        return fsym->type;
    }
    }
    return TYPE_UNKNOWN;
}

int check_stmt(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
               ir_builder_t *ir)
{
    if (!stmt)
        return 0;
    switch (stmt->kind) {
    case STMT_EXPR: {
        ir_value_t tmp;
        return check_expr(stmt->expr.expr, vars, funcs, ir, &tmp) != TYPE_UNKNOWN;
    }
    case STMT_RETURN: {
        ir_value_t val;
        if (check_expr(stmt->ret.expr, vars, funcs, ir, &val) == TYPE_UNKNOWN)
            return 0;
        ir_build_return(ir, val);
        return 1;
    }
    case STMT_IF: {
        ir_value_t cond_val;
        if (check_expr(stmt->if_stmt.cond, vars, funcs, ir, &cond_val) == TYPE_UNKNOWN)
            return 0;
        char else_label[32];
        char end_label[32];
        int id = next_label_id++;
        snprintf(else_label, sizeof(else_label), "L%d_else", id);
        snprintf(end_label, sizeof(end_label), "L%d_end", id);
        const char *target = stmt->if_stmt.else_branch ? else_label : end_label;
        ir_build_bcond(ir, cond_val, target);
        if (!check_stmt(stmt->if_stmt.then_branch, vars, funcs, ir))
            return 0;
        if (stmt->if_stmt.else_branch) {
            ir_build_br(ir, end_label);
            ir_build_label(ir, else_label);
            if (!check_stmt(stmt->if_stmt.else_branch, vars, funcs, ir))
                return 0;
        }
        ir_build_label(ir, end_label);
        return 1;
    }
    case STMT_WHILE: {
        ir_value_t cond_val;
        char start_label[32];
        char end_label[32];
        int id = next_label_id++;
        snprintf(start_label, sizeof(start_label), "L%d_start", id);
        snprintf(end_label, sizeof(end_label), "L%d_end", id);
        ir_build_label(ir, start_label);
        if (check_expr(stmt->while_stmt.cond, vars, funcs, ir, &cond_val) == TYPE_UNKNOWN)
            return 0;
        ir_build_bcond(ir, cond_val, end_label);
        if (!check_stmt(stmt->while_stmt.body, vars, funcs, ir))
            return 0;
        ir_build_br(ir, start_label);
        ir_build_label(ir, end_label);
        return 1;
    }
    case STMT_VAR_DECL:
        return symtable_add(vars, stmt->var_decl.name, stmt->var_decl.type);
    }
    return 0;
}

int check_func(func_t *func, symtable_t *funcs, ir_builder_t *ir)
{
    if (!func)
        return 0;

    symtable_t locals;
    symtable_init(&locals);

    ir_build_func_begin(ir, func->name);

    int ok = 1;
    for (size_t i = 0; i < func->body_count && ok; i++)
        ok = check_stmt(func->body[i], &locals, funcs, ir);

    ir_build_func_end(ir);

    symtable_free(&locals);
    return ok;
}

