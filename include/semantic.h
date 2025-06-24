#ifndef VC_SEMANTIC_H
#define VC_SEMANTIC_H

#include "ast.h"
#include "ir.h"

/* Symbol table entry */
typedef struct symbol {
    char *name;
    type_kind_t type;
    int param_index; /* -1 for locals */
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
int symtable_add(symtable_t *table, const char *name, type_kind_t type);
int symtable_add_func(symtable_t *table, const char *name, type_kind_t ret_type,
                      type_kind_t *param_types, size_t param_count);
int symtable_add_param(symtable_t *table, const char *name, type_kind_t type,
                       int index);
int symtable_add_global(symtable_t *table, const char *name, type_kind_t type);

/* Look up a symbol by name. Returns NULL if not found. */
symbol_t *symtable_lookup(symtable_t *table, const char *name);
symbol_t *symtable_lookup_global(symtable_t *table, const char *name);

/* Type check helpers */
type_kind_t check_expr(expr_t *expr, symtable_t *vars, symtable_t *funcs,
                       ir_builder_t *ir, ir_value_t *out);
int check_stmt(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
               ir_builder_t *ir, type_kind_t func_ret_type,
               const char *break_label, const char *continue_label);
int check_func(func_t *func, symtable_t *funcs, symtable_t *globals,
               ir_builder_t *ir);
int check_global(stmt_t *decl, symtable_t *globals, ir_builder_t *ir);

/* Print the last semantic error with source location */
void semantic_print_error(const char *msg);

#endif /* VC_SEMANTIC_H */
