/*
 * Global symbol table helpers.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include <string.h>
#include "symtable.h"
#include "util.h"

/* Insert a global variable into the table. */
int symtable_add_global(symtable_t *table, const char *name, const char *ir_name,
                        type_kind_t type, size_t array_size, size_t elem_size,
                        int is_static, int is_register, int is_const, int is_volatile,
                        int is_restrict)
{
    for (symbol_t *sym = table->globals; sym; sym = sym->next) {
        if (strcmp(sym->name, name) == 0)
            return 0;
    }
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
    sym->next = table->globals;
    table->globals = sym;
    return 1;
}

/*
 * Insert a function symbol along with its return type and parameter types.
 */
int symtable_add_func(symtable_t *table, const char *name, type_kind_t ret_type,
                      type_kind_t *param_types, size_t param_count,
                      int is_variadic, int is_prototype)
{
    if (symtable_lookup(table, name))
        return 0;
    symbol_t *sym = symtable_create_symbol(name, name);
    if (!sym)
        return 0;
    sym->type = ret_type;
    sym->param_count = param_count;
    sym->is_variadic = is_variadic;
    if (param_count) {
        sym->param_types = malloc(param_count * sizeof(*sym->param_types));
        if (!sym->param_types) {
            free(sym->name);
            free(sym->ir_name);
            free(sym);
            return 0;
        }
        for (size_t i = 0; i < param_count; i++)
            sym->param_types[i] = param_types[i];
    }
    sym->next = table->head;
    table->head = sym;
    sym->is_prototype = is_prototype;
    return 1;
}

/*
 * Look up a symbol name only in the global `globals` list.
 *
 * No search of the local `head` list is performed.
 */
symbol_t *symtable_lookup_global(symtable_t *table, const char *name)
{
    for (symbol_t *sym = table->globals; sym; sym = sym->next) {
        if (strcmp(sym->name, name) == 0)
            return sym;
    }
    return NULL;
}

