/*
 * Global and function semantic validation helpers.
 * Provides routines that check global declarations and
 * generate IR for variables and functions.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_SEMANTIC_GLOBAL_H
#define VC_SEMANTIC_GLOBAL_H

#include "ast_stmt.h"
#include "ir_core.h"
#include "symtable.h"

/* Current maximum alignment for struct packing */
extern size_t semantic_pack_alignment;
extern int semantic_stack_offset;
extern int semantic_stack_zero;
extern int semantic_named_locals;
void semantic_set_pack(size_t align);
void semantic_set_named_locals(int flag);

int check_func(func_t *func, symtable_t *funcs, symtable_t *globals,
               ir_builder_t *ir);
int check_global(stmt_t *decl, symtable_t *globals, ir_builder_t *ir);
void semantic_global_cleanup(void);

#endif /* VC_SEMANTIC_GLOBAL_H */
