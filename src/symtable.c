/*
 * Symbol table data structure.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include <string.h>
#include "symtable.h"
#include "util.h"

void symtable_init(symtable_t *table)
{
    table->head = NULL;
    table->globals = NULL;
}

void symtable_free(symtable_t *table)
{
    symbol_t *sym = table->head;
    while (sym) {
        symbol_t *next = sym->next;
        free(sym->name);
        free(sym->param_types);
        free(sym);
        sym = next;
    }
    sym = table->globals;
    while (sym) {
        symbol_t *next = sym->next;
        free(sym->name);
        free(sym->param_types);
        free(sym);
        sym = next;
    }
    table->head = NULL;
    table->globals = NULL;
}

symbol_t *symtable_lookup(symtable_t *table, const char *name)
{
    for (symbol_t *sym = table->head; sym; sym = sym->next) {
        if (strcmp(sym->name, name) == 0)
            return sym;
    }
    for (symbol_t *sym = table->globals; sym; sym = sym->next) {
        if (strcmp(sym->name, name) == 0)
            return sym;
    }
    return NULL;
}

int symtable_add(symtable_t *table, const char *name, type_kind_t type,
                 size_t array_size)
{
    if (symtable_lookup(table, name))
        return 0;
    symbol_t *sym = malloc(sizeof(*sym));
    if (!sym)
        return 0;
    sym->name = vc_strdup(name ? name : "");
    if (!sym->name) {
        free(sym);
        return 0;
    }
    sym->type = type;
    sym->array_size = array_size;
    sym->param_index = -1;
    sym->param_types = NULL;
    sym->param_count = 0;
    sym->next = table->head;
    table->head = sym;
    return 1;
}

int symtable_add_param(symtable_t *table, const char *name, type_kind_t type,
                       int index)
{
    if (symtable_lookup(table, name))
        return 0;
    symbol_t *sym = malloc(sizeof(*sym));
    if (!sym)
        return 0;
    sym->name = vc_strdup(name ? name : "");
    if (!sym->name) {
        free(sym);
        return 0;
    }
    sym->type = type;
    sym->array_size = 0;
    sym->param_index = index;
    sym->param_types = NULL;
    sym->param_count = 0;
    sym->next = table->head;
    table->head = sym;
    return 1;
}

int symtable_add_global(symtable_t *table, const char *name, type_kind_t type,
                        size_t array_size)
{
    for (symbol_t *sym = table->globals; sym; sym = sym->next) {
        if (strcmp(sym->name, name) == 0)
            return 0;
    }
    symbol_t *sym = malloc(sizeof(*sym));
    if (!sym)
        return 0;
    sym->name = vc_strdup(name ? name : "");
    if (!sym->name) {
        free(sym);
        return 0;
    }
    sym->type = type;
    sym->array_size = array_size;
    sym->param_index = -1;
    sym->param_types = NULL;
    sym->param_count = 0;
    sym->next = table->globals;
    table->globals = sym;
    return 1;
}

int symtable_add_func(symtable_t *table, const char *name, type_kind_t ret_type,
                      type_kind_t *param_types, size_t param_count)
{
    if (symtable_lookup(table, name))
        return 0;
    symbol_t *sym = malloc(sizeof(*sym));
    if (!sym)
        return 0;
    sym->name = vc_strdup(name ? name : "");
    if (!sym->name) {
        free(sym);
        return 0;
    }
    sym->type = ret_type;
    sym->param_index = -1;
    sym->param_count = param_count;
    sym->param_types = NULL;
    if (param_count) {
        sym->param_types = malloc(param_count * sizeof(*sym->param_types));
        if (!sym->param_types) {
            free(sym->name);
            free(sym);
            return 0;
        }
        for (size_t i = 0; i < param_count; i++)
            sym->param_types[i] = param_types[i];
    }
    sym->next = table->head;
    table->head = sym;
    return 1;
}

symbol_t *symtable_lookup_global(symtable_t *table, const char *name)
{
    for (symbol_t *sym = table->globals; sym; sym = sym->next) {
        if (strcmp(sym->name, name) == 0)
            return sym;
    }
    return NULL;
}

