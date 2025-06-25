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

/* Allocate and initialise a new symbol entry */
static symbol_t *symtable_create_symbol(const char *name, const char *ir_name)
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
    sym->struct_members = NULL;
    sym->struct_member_count = 0;
    sym->total_size = 0;
    return sym;
}

/* Reset a symbol table so that both local and global lists are empty. */
void symtable_init(symtable_t *table)
{
    table->head = NULL;
    table->globals = NULL;
}

/* Free all symbols stored in the table and reset it to an empty state. */
void symtable_free(symtable_t *table)
{
    symbol_t *sym = table->head;
    while (sym) {
        symbol_t *next = sym->next;
        free(sym->name);
        for (size_t i = 0; i < sym->member_count; i++)
            free(sym->members[i].name);
        free(sym->members);
        for (size_t i = 0; i < sym->struct_member_count; i++)
            free(sym->struct_members[i].name);
        free(sym->struct_members);
        free(sym->param_types);
        free(sym);
        sym = next;
    }
    sym = table->globals;
    while (sym) {
        symbol_t *next = sym->next;
        free(sym->name);
        for (size_t i = 0; i < sym->member_count; i++)
            free(sym->members[i].name);
        free(sym->members);
        for (size_t i = 0; i < sym->struct_member_count; i++)
            free(sym->struct_members[i].name);
        free(sym->struct_members);
        free(sym->param_types);
        free(sym);
        sym = next;
    }
    table->head = NULL;
    table->globals = NULL;
}

/*
 * Search the table for a symbol by name.  Local symbols take precedence
 * over globals.  Returns NULL if the name is not present.
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
                 int is_static, int is_const, int is_volatile)
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
    sym->is_const = is_const;
    sym->is_volatile = is_volatile;
    sym->next = table->head;
    table->head = sym;
    return 1;
}

/*
 * Insert a function parameter.  Parameters are stored in the local list with
 * the index field recording the argument position.
 */
int symtable_add_param(symtable_t *table, const char *name, type_kind_t type,
                       size_t elem_size, int index)
{
    if (symtable_lookup(table, name))
        return 0;
    symbol_t *sym = symtable_create_symbol(name, name);
    if (!sym)
        return 0;
    sym->type = type;
    sym->elem_size = elem_size;
    sym->param_index = index;
    sym->next = table->head;
    table->head = sym;
    return 1;
}

/* Insert a global variable into the table. */
int symtable_add_global(symtable_t *table, const char *name, const char *ir_name,
                        type_kind_t type, size_t array_size, size_t elem_size,
                        int is_static, int is_const, int is_volatile)
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
    sym->is_const = is_const;
    sym->is_volatile = is_volatile;
    sym->next = table->globals;
    table->globals = sym;
    return 1;
}

/*
 * Insert a function symbol along with its return type and parameter types.
 */
int symtable_add_func(symtable_t *table, const char *name, type_kind_t ret_type,
                      type_kind_t *param_types, size_t param_count,
                      int is_prototype)
{
    if (symtable_lookup(table, name))
        return 0;
    symbol_t *sym = symtable_create_symbol(name, name);
    if (!sym)
        return 0;
    sym->type = ret_type;
    sym->param_count = param_count;
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

/* Insert an enum constant in the current scope */
int symtable_add_enum(symtable_t *table, const char *name, int value)
{
    if (symtable_lookup(table, name))
        return 0;
    symbol_t *sym = symtable_create_symbol(name, name);
    if (!sym)
        return 0;
    sym->type = TYPE_INT;
    sym->enum_value = value;
    sym->is_enum_const = 1;
    sym->next = table->head;
    table->head = sym;
    return 1;
}

/* Insert an enum constant in the global scope */
int symtable_add_enum_global(symtable_t *table, const char *name, int value)
{
    for (symbol_t *sym = table->globals; sym; sym = sym->next) {
        if (strcmp(sym->name, name) == 0)
            return 0;
    }
    symbol_t *sym = symtable_create_symbol(name, name);
    if (!sym)
        return 0;
    sym->type = TYPE_INT;
    sym->enum_value = value;
    sym->is_enum_const = 1;
    sym->next = table->globals;
    table->globals = sym;
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

/* Look up a name only in the global list. */
symbol_t *symtable_lookup_global(symtable_t *table, const char *name)
{
    for (symbol_t *sym = table->globals; sym; sym = sym->next) {
        if (strcmp(sym->name, name) == 0)
            return sym;
    }
    return NULL;
}

/* Insert a union type definition in the current scope */
int symtable_add_union(symtable_t *table, const char *tag,
                       union_member_t *members, size_t member_count)
{
    if (symtable_lookup(table, tag))
        return 0;
    symbol_t *sym = symtable_create_symbol(tag, tag);
    if (!sym)
        return 0;
    sym->type = TYPE_UNION;
    if (member_count) {
        sym->members = malloc(member_count * sizeof(*sym->members));
        if (!sym->members) {
            free(sym->name);
            free(sym->ir_name);
            free(sym);
            return 0;
        }
        for (size_t i = 0; i < member_count; i++) {
            sym->members[i].name = vc_strdup(members[i].name);
            sym->members[i].type = members[i].type;
            sym->members[i].elem_size = members[i].elem_size;
            sym->members[i].offset = members[i].offset;
        }
    }
    sym->member_count = member_count;
    size_t max = 0;
    for (size_t i = 0; i < member_count; i++)
        if (members[i].elem_size > max)
            max = members[i].elem_size;
    sym->total_size = max;
    sym->next = table->head;
    table->head = sym;
    return 1;
}

/* Insert a union type definition in the global scope */
int symtable_add_union_global(symtable_t *table, const char *tag,
                              union_member_t *members, size_t member_count)
{
    for (symbol_t *sym = table->globals; sym; sym = sym->next) {
        if (strcmp(sym->name, tag) == 0)
            return 0;
    }
    symbol_t *sym = symtable_create_symbol(tag, tag);
    if (!sym)
        return 0;
    sym->type = TYPE_UNION;
    if (member_count) {
        sym->members = malloc(member_count * sizeof(*sym->members));
        if (!sym->members) {
            free(sym->name);
            free(sym->ir_name);
            free(sym);
            return 0;
        }
        for (size_t i = 0; i < member_count; i++) {
            sym->members[i].name = vc_strdup(members[i].name);
            sym->members[i].type = members[i].type;
            sym->members[i].elem_size = members[i].elem_size;
            sym->members[i].offset = members[i].offset;
        }
    }
    sym->member_count = member_count;
    size_t max = 0;
    for (size_t i = 0; i < member_count; i++)
        if (members[i].elem_size > max)
            max = members[i].elem_size;
    sym->total_size = max;
    sym->next = table->globals;
    table->globals = sym;
    return 1;
}

/* Look up a union type definition by tag */
symbol_t *symtable_lookup_union(symtable_t *table, const char *tag)
{
    for (symbol_t *sym = table->head; sym; sym = sym->next) {
        if (sym->type == TYPE_UNION && sym->member_count > 0 &&
            strcmp(sym->name, tag) == 0)
            return sym;
    }
    for (symbol_t *sym = table->globals; sym; sym = sym->next) {
        if (sym->type == TYPE_UNION && sym->member_count > 0 &&
            strcmp(sym->name, tag) == 0)
            return sym;
    }
    return NULL;
}

int symtable_add_struct(symtable_t *table, const char *tag,
                        struct_member_t *members, size_t member_count)
{
    if (symtable_lookup(table, tag))
        return 0;
    symbol_t *sym = symtable_create_symbol(tag, tag);
    if (!sym)
        return 0;
    sym->type = TYPE_STRUCT;
    if (member_count) {
        sym->struct_members = malloc(member_count * sizeof(*sym->struct_members));
        if (!sym->struct_members) {
            free(sym->name); free(sym->ir_name); free(sym); return 0;
        }
        for (size_t i = 0; i < member_count; i++) {
            sym->struct_members[i].name = vc_strdup(members[i].name);
            sym->struct_members[i].type = members[i].type;
            sym->struct_members[i].elem_size = members[i].elem_size;
            sym->struct_members[i].offset = members[i].offset;
        }
    }
    sym->struct_member_count = member_count;
    size_t total = 0;
    for (size_t i = 0; i < member_count; i++)
        total += members[i].elem_size;
    sym->total_size = total;
    sym->next = table->head;
    table->head = sym;
    return 1;
}

int symtable_add_struct_global(symtable_t *table, const char *tag,
                               struct_member_t *members, size_t member_count)
{
    for (symbol_t *sym = table->globals; sym; sym = sym->next) {
        if (strcmp(sym->name, tag) == 0)
            return 0;
    }
    symbol_t *sym = symtable_create_symbol(tag, tag);
    if (!sym)
        return 0;
    sym->type = TYPE_STRUCT;
    if (member_count) {
        sym->struct_members = malloc(member_count * sizeof(*sym->struct_members));
        if (!sym->struct_members) { free(sym->name); free(sym->ir_name); free(sym); return 0; }
        for (size_t i = 0; i < member_count; i++) {
            sym->struct_members[i].name = vc_strdup(members[i].name);
            sym->struct_members[i].type = members[i].type;
            sym->struct_members[i].elem_size = members[i].elem_size;
            sym->struct_members[i].offset = members[i].offset;
        }
    }
    sym->struct_member_count = member_count;
    size_t total = 0; for (size_t i = 0; i < member_count; i++) total += members[i].elem_size;
    sym->total_size = total;
    sym->next = table->globals;
    table->globals = sym;
    return 1;
}

symbol_t *symtable_lookup_struct(symtable_t *table, const char *tag)
{
    for (symbol_t *sym = table->head; sym; sym = sym->next) {
        if (sym->type == TYPE_STRUCT && sym->struct_member_count > 0 &&
            strcmp(sym->name, tag) == 0)
            return sym;
    }
    for (symbol_t *sym = table->globals; sym; sym = sym->next) {
        if (sym->type == TYPE_STRUCT && sym->struct_member_count > 0 &&
            strcmp(sym->name, tag) == 0)
            return sym;
    }
    return NULL;
}

