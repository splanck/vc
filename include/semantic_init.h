/*
 * Initializer list expansion helpers.
 *
 * These routines convert array and struct initializer lists
 * into constant values.  All expressions must be constant; on
 * failure an error location is recorded and zero is returned.
 * The caller owns the returned array of values.
 */
#ifndef VC_SEMANTIC_INIT_H
#define VC_SEMANTIC_INIT_H

#include "ast.h"
#include "symtable.h"

int expand_array_initializer(init_entry_t *entries, size_t count,
                             size_t array_size, symtable_t *vars,
                             size_t line, size_t column,
                             long long **out_vals);

int expand_struct_initializer(init_entry_t *entries, size_t count,
                              symbol_t *sym, symtable_t *vars,
                              size_t line, size_t column,
                              long long **out_vals);

#endif /* VC_SEMANTIC_INIT_H */
