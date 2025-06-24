#ifndef VC_SYMTABLE_H
#define VC_SYMTABLE_H

#include <stddef.h>
#include "ast.h"

/* Symbol table entry */
typedef struct symbol {
    char *name;
    type_kind_t type;
    int param_index; /* -1 for locals */
    size_t array_size;
    type_kind_t *param_types; /* for functions */
    size_t param_count;
    struct symbol *next;
} symbol_t;

/* Symbol table container */
typedef struct {
    symbol_t *head;    /* locals or functions */
    symbol_t *globals; /* global variables */
} symtable_t;

/* Initialize and free a symbol table */
void symtable_init(symtable_t *table);
void symtable_free(symtable_t *table);

/* Add a symbol to the table. Returns non-zero on success. */
int symtable_add(symtable_t *table, const char *name, type_kind_t type,
                 size_t array_size);
int symtable_add_param(symtable_t *table, const char *name, type_kind_t type,
                       int index);
int symtable_add_func(symtable_t *table, const char *name, type_kind_t ret_type,
                      type_kind_t *param_types, size_t param_count);
int symtable_add_global(symtable_t *table, const char *name, type_kind_t type,
                        size_t array_size);

/* Look up a symbol by name. Returns NULL if not found. */
symbol_t *symtable_lookup(symtable_t *table, const char *name);
symbol_t *symtable_lookup_global(symtable_t *table, const char *name);

#endif /* VC_SYMTABLE_H */
