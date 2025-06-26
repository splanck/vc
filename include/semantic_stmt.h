/*
 * Statement semantic validation helpers.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_SEMANTIC_STMT_H
#define VC_SEMANTIC_STMT_H

#include "ast.h"
#include "ir.h"
#include "symtable.h"

typedef struct label_entry {
    char *name;
    char *ir_name;
    struct label_entry *next;
} label_entry_t;

typedef struct {
    label_entry_t *head;
} label_table_t;

void label_table_init(label_table_t *t);
void label_table_free(label_table_t *t);
const char *label_table_get(label_table_t *t, const char *name);
const char *label_table_get_or_add(label_table_t *t, const char *name);

int check_stmt(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
               void *labels, ir_builder_t *ir, type_kind_t func_ret_type,
               const char *break_label, const char *continue_label);

#endif /* VC_SEMANTIC_STMT_H */
