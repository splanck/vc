#ifndef VC_SEMANTIC_H
#define VC_SEMANTIC_H

#include "ast.h"

/* Basic type categories */
typedef enum {
    TYPE_INT,
    TYPE_UNKNOWN
} type_kind_t;

/* Symbol table entry */
typedef struct symbol {
    char *name;
    type_kind_t type;
    struct symbol *next;
} symbol_t;

/* Symbol table container */
typedef struct {
    symbol_t *head;
} symtable_t;

/* Initialize and free a symbol table */
void symtable_init(symtable_t *table);
void symtable_free(symtable_t *table);

/* Add a symbol to the table. Returns non-zero on success. */
int symtable_add(symtable_t *table, const char *name, type_kind_t type);

/* Look up a symbol by name. Returns NULL if not found. */
symbol_t *symtable_lookup(symtable_t *table, const char *name);

/* Type check helpers */
type_kind_t check_expr(expr_t *expr, symtable_t *table);
int check_stmt(stmt_t *stmt, symtable_t *table);

#endif /* VC_SEMANTIC_H */
