#include <stdlib.h>
#include <string.h>
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

static type_kind_t check_binary(expr_t *left, expr_t *right, symtable_t *table,
                                ir_builder_t *ir, ir_value_t *out, binop_t op)
{
    ir_value_t lval, rval;
    type_kind_t lt = check_expr(left, table, ir, &lval);
    type_kind_t rt = check_expr(right, table, ir, &rval);
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

type_kind_t check_expr(expr_t *expr, symtable_t *table, ir_builder_t *ir,
                       ir_value_t *out)
{
    if (!expr)
        return TYPE_UNKNOWN;
    switch (expr->kind) {
    case EXPR_NUMBER:
        if (out)
            *out = ir_build_const(ir, (int)strtol(expr->number.value, NULL, 10));
        return TYPE_INT;
    case EXPR_IDENT: {
        symbol_t *sym = symtable_lookup(table, expr->ident.name);
        if (!sym)
            return TYPE_UNKNOWN;
        if (out)
            *out = ir_build_load(ir, expr->ident.name);
        return sym->type;
    }
    case EXPR_BINARY:
        return check_binary(expr->binary.left, expr->binary.right, table, ir,
                           out, expr->binary.op);
    case EXPR_ASSIGN: {
        ir_value_t val;
        symbol_t *sym = symtable_lookup(table, expr->assign.name);
        if (!sym)
            return TYPE_UNKNOWN;
        if (check_expr(expr->assign.value, table, ir, &val) == TYPE_INT) {
            ir_build_store(ir, expr->assign.name, val);
            if (out)
                *out = val;
            return TYPE_INT;
        }
        return TYPE_UNKNOWN;
    }
    }
    return TYPE_UNKNOWN;
}

int check_stmt(stmt_t *stmt, symtable_t *table, ir_builder_t *ir)
{
    if (!stmt)
        return 0;
    switch (stmt->kind) {
    case STMT_EXPR: {
        ir_value_t tmp;
        return check_expr(stmt->expr.expr, table, ir, &tmp) != TYPE_UNKNOWN;
    }
    case STMT_RETURN: {
        ir_value_t val;
        if (check_expr(stmt->ret.expr, table, ir, &val) == TYPE_UNKNOWN)
            return 0;
        ir_build_return(ir, val);
        return 1;
    }
    case STMT_VAR_DECL:
        return symtable_add(table, stmt->var_decl.name, TYPE_INT);
    }
    return 0;
}

