/*
 * Compound type symbol helpers (structs, unions and enums).
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include <string.h>
#include "symtable.h"
#include "util.h"

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

/* Record an enum tag in the current scope */
int symtable_add_enum_tag(symtable_t *table, const char *tag)
{
    if (symtable_lookup(table, tag))
        return 0;
    symbol_t *sym = symtable_create_symbol(tag, tag);
    if (!sym)
        return 0;
    sym->type = TYPE_ENUM;
    sym->next = table->head;
    table->head = sym;
    return 1;
}

/* Record an enum tag in the global scope */
int symtable_add_enum_tag_global(symtable_t *table, const char *tag)
{
    for (symbol_t *sym = table->globals; sym; sym = sym->next) {
        if (strcmp(sym->name, tag) == 0)
            return 0;
    }
    symbol_t *sym = symtable_create_symbol(tag, tag);
    if (!sym)
        return 0;
    sym->type = TYPE_ENUM;
    sym->next = table->globals;
    table->globals = sym;
    return 1;
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
            sym->members[i].bit_width = members[i].bit_width;
            sym->members[i].bit_offset = members[i].bit_offset;
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
            sym->members[i].bit_width = members[i].bit_width;
            sym->members[i].bit_offset = members[i].bit_offset;
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

/*
 * Look up a union type definition by tag by scanning both lists.
 * Returns NULL if the tag is not defined.
 */
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

/* Insert a struct type definition in the current scope */
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
            free(sym->name);
            free(sym->ir_name);
            free(sym);
            return 0;
        }
        for (size_t i = 0; i < member_count; i++) {
            sym->struct_members[i].name = vc_strdup(members[i].name);
            sym->struct_members[i].type = members[i].type;
            sym->struct_members[i].elem_size = members[i].elem_size;
            sym->struct_members[i].offset = members[i].offset;
            sym->struct_members[i].bit_width = members[i].bit_width;
            sym->struct_members[i].bit_offset = members[i].bit_offset;
        }
    }
    sym->struct_member_count = member_count;
    size_t total = 0;
    for (size_t i = 0; i < member_count; i++) {
        size_t end = sym->struct_members[i].offset;
        if (sym->struct_members[i].bit_width > 0) {
            unsigned bits = sym->struct_members[i].bit_offset +
                            sym->struct_members[i].bit_width;
            end += (bits + 7) / 8;
        } else {
            end += sym->struct_members[i].elem_size;
        }
        if (end > total)
            total = end;
    }
    sym->struct_total_size = total;
    sym->next = table->head;
    table->head = sym;
    return 1;
}

/* Insert a struct type definition in the global scope */
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
        if (!sym->struct_members) {
            free(sym->name);
            free(sym->ir_name);
            free(sym);
            return 0;
        }
        for (size_t i = 0; i < member_count; i++) {
            sym->struct_members[i].name = vc_strdup(members[i].name);
            sym->struct_members[i].type = members[i].type;
            sym->struct_members[i].elem_size = members[i].elem_size;
            sym->struct_members[i].offset = members[i].offset;
            sym->struct_members[i].bit_width = members[i].bit_width;
            sym->struct_members[i].bit_offset = members[i].bit_offset;
        }
    }
    sym->struct_member_count = member_count;
    size_t total = 0;
    for (size_t i = 0; i < member_count; i++) {
        size_t end = sym->struct_members[i].offset;
        if (sym->struct_members[i].bit_width > 0) {
            unsigned bits = sym->struct_members[i].bit_offset +
                            sym->struct_members[i].bit_width;
            end += (bits + 7) / 8;
        } else {
            end += sym->struct_members[i].elem_size;
        }
        if (end > total)
            total = end;
    }
    sym->struct_total_size = total;
    sym->next = table->globals;
    table->globals = sym;
    return 1;
}

/*
 * Look up a struct type definition by tag across both lists.
 * Returns NULL if the tag is unknown.
 */
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

