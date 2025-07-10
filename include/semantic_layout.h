/*
 * Layout and metadata helpers for structs and unions.
 * Provides routines shared across semantic phases for
 * computing member offsets and copying declaration metadata.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_SEMANTIC_LAYOUT_H
#define VC_SEMANTIC_LAYOUT_H

#include "ast_stmt.h"
#include "symtable.h"

size_t layout_union_members(union_member_t *members, size_t count);
size_t layout_struct_members(struct_member_t *members, size_t count);

int compute_union_layout(stmt_t *decl, symtable_t *globals);
int compute_struct_layout(stmt_t *decl, symtable_t *globals);

int copy_union_metadata(symbol_t *sym, union_member_t *members,
                        size_t count, size_t total);
int copy_struct_metadata(symbol_t *sym, struct_member_t *members,
                         size_t count, size_t total);
int copy_aggregate_metadata(stmt_t *decl, symbol_t *sym,
                            symtable_t *globals);

#endif /* VC_SEMANTIC_LAYOUT_H */
