/*
 * Core symbol table helpers.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include <string.h>
#include "symtable.h"
#include "util.h"

/*
 * Allocate and initialise a new symbol entry.
 *
 * The returned symbol is not inserted into any list;
 * callers add it to either the local `head` list or the
 * `globals` list.
 */
symbol_t *symtable_create_symbol(const char *name, const char *ir_name)
{
    symbol_t *sym = calloc(1, sizeof(*sym));
    if (!sym)
        return NULL;

    sym->name = vc_strdup(name ? name : "");
    if (!sym->name) {
        free(sym);
        return NULL;
    }

    const char *in = ir_name ? ir_name : name;
    if (in) {
        sym->ir_name = vc_strdup(in);
        if (!sym->ir_name) {
            free(sym->name);
            free(sym);
            return NULL;
        }
    } else {
        sym->ir_name = NULL;
    }

    sym->param_index = -1;
    sym->alias_type = TYPE_UNKNOWN;
    sym->elem_size = 0;
    sym->members = NULL;
    sym->member_count = 0;
    sym->total_size = 0;
    sym->struct_members = NULL;
    sym->struct_member_count = 0;
    sym->struct_total_size = 0;
    sym->func_ret_type = TYPE_UNKNOWN;
    sym->ret_struct_size = 0;
    sym->param_struct_sizes = NULL;
    sym->func_param_types = NULL;
    sym->func_param_count = 0;
    sym->func_variadic = 0;
    sym->is_restrict = 0;
    sym->is_register = 0;
    sym->is_variadic = 0;
    sym->is_inline = 0;
    return sym;
}

/* Reset a symbol table so that both local and global lists are empty. */
void symtable_init(symtable_t *table)
{
    table->head = NULL;
    table->globals = NULL;
}

/*
 * Free a singly linked list of symbols.
 *
 * Ownership of the list and each contained symbol is transferred to
 * this helper.  All dynamically allocated fields within every symbol
 * are released.
 */
static void free_symbol_list(symbol_t *sym)
{
    while (sym) {
        symbol_t *next = sym->next;
        free(sym->name);
        free(sym->ir_name);
        for (size_t i = 0; i < sym->member_count; i++)
            free(sym->members[i].name);
        free(sym->members);
        for (size_t i = 0; i < sym->struct_member_count; i++)
            free(sym->struct_members[i].name);
        free(sym->struct_members);
        free(sym->param_types);
        /* free any tracked aggregate parameter sizes */
        free(sym->param_struct_sizes);
        sym->param_struct_sizes = NULL;
        free(sym->func_param_types);
        free(sym);
        sym = next;
    }
}

/* Free all symbols stored in the table and reset it to an empty state. */
void symtable_free(symtable_t *table)
{
    free_symbol_list(table->head);
    free_symbol_list(table->globals);
    table->head = NULL;
    table->globals = NULL;
}

/*
 * Search the table for a symbol by name.
 *
 * The local `head` list is searched first followed by the
 * `globals` list. Returns NULL if the name is not present.
 */
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

/*
 * Insert a new local variable symbol.  The function fails if a symbol with the
 * same name already exists in either the local or global list.
 */
int symtable_add(symtable_t *table, const char *name, const char *ir_name,
                 type_kind_t type, size_t array_size, size_t elem_size,
                 int is_static, int is_register, int is_const, int is_volatile,
                 int is_restrict)
{
    if (symtable_lookup(table, name))
        return 0;
    symbol_t *sym = symtable_create_symbol(name, ir_name ? ir_name : name);
    if (!sym)
        return 0;
    sym->type = type;
    sym->array_size = array_size;
    sym->elem_size = elem_size;
    sym->is_static = is_static;
    sym->is_register = is_register;
    sym->is_const = is_const;
    sym->is_volatile = is_volatile;
    sym->is_restrict = is_restrict;
    sym->next = table->head;
    table->head = sym;
    return 1;
}

/*
 * Insert a function parameter.  Parameters are stored in the local list with
 * the index field recording the argument position.
 */
int symtable_add_param(symtable_t *table, const char *name, type_kind_t type,
                       size_t elem_size, int index, int is_restrict)
{
    if (symtable_lookup(table, name))
        return 0;
    symbol_t *sym = symtable_create_symbol(name, name);
    if (!sym)
        return 0;
    sym->type = type;
    sym->elem_size = elem_size;
    sym->param_index = index;
    sym->is_restrict = is_restrict;
    sym->next = table->head;
    table->head = sym;
    return 1;
}

/* Add a typedef in the current scope */
int symtable_add_typedef(symtable_t *table, const char *name, type_kind_t type,
                         size_t array_size, size_t elem_size)
{
    (void)array_size;
    if (symtable_lookup(table, name))
        return 0;
    symbol_t *sym = symtable_create_symbol(name, name);
    if (!sym)
        return 0;
    sym->type = TYPE_VOID;
    sym->is_typedef = 1;
    sym->alias_type = type;
    sym->elem_size = elem_size;
    sym->next = table->head;
    table->head = sym;
    return 1;
}

/* Add a typedef in the global scope */
int symtable_add_typedef_global(symtable_t *table, const char *name,
                                type_kind_t type, size_t array_size,
                                size_t elem_size)
{
    (void)array_size;
    for (symbol_t *sym = table->globals; sym; sym = sym->next) {
        if (strcmp(sym->name, name) == 0)
            return 0;
    }
    symbol_t *sym = symtable_create_symbol(name, name);
    if (!sym)
        return 0;
    sym->type = TYPE_VOID;
    sym->is_typedef = 1;
    sym->alias_type = type;
    sym->elem_size = elem_size;
    sym->next = table->globals;
    table->globals = sym;
    return 1;
}

