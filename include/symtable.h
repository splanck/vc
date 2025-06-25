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

/* Symbol table entry */
typedef struct symbol {
    char *name;
    char *ir_name;
    type_kind_t type;
    int param_index; /* -1 for locals */
    size_t array_size;
    int enum_value;
    int is_enum_const;
    int is_typedef;
    type_kind_t alias_type;
    int is_static;
    type_kind_t *param_types; /* for functions */
    size_t param_count;
    int is_prototype;
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

/* Add a symbol to the table. Returns non-zero on success. */
/* Locals */
int symtable_add(symtable_t *table, const char *name, const char *ir_name,
                 type_kind_t type, size_t array_size, int is_static);
/* Parameters are stored as locals with an index */
int symtable_add_param(symtable_t *table, const char *name, type_kind_t type,
                       int index);
/* Functions record return and parameter types */
int symtable_add_func(symtable_t *table, const char *name, type_kind_t ret_type,
                      type_kind_t *param_types, size_t param_count,
                      int is_prototype);
/* Globals live in a separate list */
int symtable_add_global(symtable_t *table, const char *name, const char *ir_name,
                        type_kind_t type, size_t array_size, int is_static);
int symtable_add_enum(symtable_t *table, const char *name, int value);
int symtable_add_enum_global(symtable_t *table, const char *name, int value);
int symtable_add_typedef(symtable_t *table, const char *name, type_kind_t type,
                         size_t array_size);
int symtable_add_typedef_global(symtable_t *table, const char *name,
                                type_kind_t type, size_t array_size);

/* Look up a symbol by name. Returns NULL if not found. */
symbol_t *symtable_lookup(symtable_t *table, const char *name);
symbol_t *symtable_lookup_global(symtable_t *table, const char *name);

#endif /* VC_SYMTABLE_H */
