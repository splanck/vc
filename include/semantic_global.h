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

#include "ast.h"
#include "ir_core.h"
#include "symtable.h"

size_t layout_union_members(union_member_t *members, size_t count);
size_t layout_struct_members(struct_member_t *members, size_t count);

int check_func(func_t *func, symtable_t *funcs, symtable_t *globals,
               ir_builder_t *ir);
int check_global(stmt_t *decl, symtable_t *globals, ir_builder_t *ir);

#endif /* VC_SEMANTIC_GLOBAL_H */
