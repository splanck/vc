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

static type_kind_t check_binary(expr_t *left, expr_t *right, symtable_t *table)
{
    type_kind_t lt = check_expr(left, table);
    type_kind_t rt = check_expr(right, table);
    if (lt == TYPE_INT && rt == TYPE_INT)
        return TYPE_INT;
    return TYPE_UNKNOWN;
}

type_kind_t check_expr(expr_t *expr, symtable_t *table)
{
    if (!expr)
        return TYPE_UNKNOWN;
    switch (expr->kind) {
    case EXPR_NUMBER:
        return TYPE_INT;
    case EXPR_IDENT: {
        symbol_t *sym = symtable_lookup(table, expr->ident.name);
        if (!sym)
            return TYPE_UNKNOWN;
        return sym->type;
    }
    case EXPR_BINARY:
        return check_binary(expr->binary.left, expr->binary.right, table);
    }
    return TYPE_UNKNOWN;
}

int check_stmt(stmt_t *stmt, symtable_t *table)
{
    if (!stmt)
        return 0;
    switch (stmt->kind) {
    case STMT_EXPR:
        return check_expr(stmt->expr.expr, table) != TYPE_UNKNOWN;
    case STMT_RETURN:
        return check_expr(stmt->ret.expr, table) != TYPE_UNKNOWN;
    }
    return 0;
}

