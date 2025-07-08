/*
 * Symbol table interfaces.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_SYMTABLE_H
#define VC_SYMTABLE_H

#include <stddef.h>
#include "ast.h"
#include "ir_core.h"

/* Symbol table entry */
typedef struct symbol {
    char *name;
    char *ir_name;
    type_kind_t type;
    int param_index; /* -1 for locals */
    size_t array_size;
    size_t elem_size;
    ir_value_t vla_addr; /* base pointer for variable-length arrays */
    ir_value_t vla_size; /* runtime element count */
    int enum_value;
    int is_enum_const;
    int is_typedef;
    type_kind_t alias_type;
    union_member_t *members; /* for union declarations */
    size_t member_count;
    char *active_member;     /* last written union field */
    size_t total_size;
    struct_member_t *struct_members; /* for struct declarations */
    size_t struct_member_count;
    size_t struct_total_size;
    type_kind_t func_ret_type; /* for function pointers */
    size_t ret_struct_size;    /* size of struct/union return value */
    size_t *param_struct_sizes; /* sizes of aggregate parameters */
    type_kind_t *func_param_types;
    size_t func_param_count;
    int func_variadic;
    int is_static;
    int is_register;
    int is_const;
    int is_volatile;
    int is_restrict;
    type_kind_t *param_types; /* for functions */
    size_t param_count;
    int is_variadic;
    int is_prototype;
    int is_inline;
    int is_noreturn;
    struct symbol *next;
} symbol_t;

/* Symbol table container */
typedef struct {
    symbol_t *head;    /* locals or functions */
    symbol_t *globals; /* global variables */
} symtable_t;

/* Initialize and free a symbol table */
/*
 * The table maintains two singly linked lists: one for the current scope and
 * one for global symbols.  Insertion helpers add new entries to these lists and
 * lookup helpers search through them.
 */
void symtable_init(symtable_t *table);
void symtable_free(symtable_t *table);
/*
 * Allocate an unlinked symbol record.
 *
 * The caller must insert the returned symbol into either the
 * local `head` list or the global `globals` list.
 */
symbol_t *symtable_create_symbol(const char *name, const char *ir_name);

/* Add a symbol to the table. Returns non-zero on success. */
/* Locals */
int symtable_add(symtable_t *table, const char *name, const char *ir_name,
                 type_kind_t type, size_t array_size, size_t elem_size,
                 int is_static, int is_register, int is_const, int is_volatile,
                 int is_restrict);
/* Parameters are stored as locals with an index */
int symtable_add_param(symtable_t *table, const char *name, type_kind_t type,
                       size_t elem_size, int index, int is_restrict);
/* Functions record return and parameter types */
int symtable_add_func(symtable_t *table, const char *name, type_kind_t ret_type,
                      size_t ret_struct_size,
                      size_t *param_struct_sizes,
                      type_kind_t *param_types, size_t param_count,
                      int is_variadic, int is_prototype, int is_inline,
                      int is_noreturn);
/* Globals live in a separate list */
int symtable_add_global(symtable_t *table, const char *name, const char *ir_name,
                        type_kind_t type, size_t array_size, size_t elem_size,
                        int is_static, int is_register, int is_const, int is_volatile,
                        int is_restrict);
int symtable_add_enum(symtable_t *table, const char *name, int value);
int symtable_add_enum_global(symtable_t *table, const char *name, int value);
int symtable_add_enum_tag(symtable_t *table, const char *tag);
int symtable_add_enum_tag_global(symtable_t *table, const char *tag);
int symtable_add_typedef(symtable_t *table, const char *name, type_kind_t type,
                         size_t array_size, size_t elem_size);
int symtable_add_typedef_global(symtable_t *table, const char *name,
                                type_kind_t type, size_t array_size,
                                size_t elem_size);
int symtable_add_union(symtable_t *table, const char *tag,
                       union_member_t *members, size_t member_count);
int symtable_add_union_global(symtable_t *table, const char *tag,
                              union_member_t *members, size_t member_count);
int symtable_add_struct(symtable_t *table, const char *tag,
                        struct_member_t *members, size_t member_count);
int symtable_add_struct_global(symtable_t *table, const char *tag,
                               struct_member_t *members, size_t member_count);
symbol_t *symtable_lookup_struct(symtable_t *table, const char *tag);
symbol_t *symtable_lookup_union(symtable_t *table, const char *tag);

/*
 * Look up a symbol by name.
 *
 * The local `head` list is searched first, followed by the
 * global `globals` list. Returns NULL if not found.
 */
symbol_t *symtable_lookup(symtable_t *table, const char *name);
/* Search only the `globals` list. */
symbol_t *symtable_lookup_global(symtable_t *table, const char *name);

#endif /* VC_SYMTABLE_H */
