/*
 * Control flow statement helpers.
 * Provides label table utilities along with routines for validating
 * if/else and switch statements while emitting the necessary IR.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_SEMANTIC_CONTROL_H
#define VC_SEMANTIC_CONTROL_H

#include "ast_stmt.h"
#include "ir_core.h"
#include "symtable.h"
#include "util.h"

typedef struct label_entry {
    char *name;
    char *ir_name;
    struct label_entry *next;
} label_entry_t;

typedef struct {
    label_entry_t *head;
} label_table_t;

/* Initialize a label table */
void label_table_init(label_table_t *t);

/* Free all entries in a label table */
void label_table_free(label_table_t *t);

/* Look up a label and return its IR name or NULL */
const char *label_table_get(label_table_t *t, const char *name);

/* Get or add a label and return its IR name */
const char *label_table_get_or_add(label_table_t *t, const char *name);

/* Check an if/else statement and emit IR */
int check_if_stmt(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
                  label_table_t *labels, ir_builder_t *ir,
                  type_kind_t func_ret_type,
                  const char *break_label, const char *continue_label);

/* Check a switch statement and emit IR */
int check_switch_stmt(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
                      label_table_t *labels, ir_builder_t *ir,
                      type_kind_t func_ret_type);

#endif /* VC_SEMANTIC_CONTROL_H */
