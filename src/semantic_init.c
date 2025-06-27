/*
 * Initializer list expansion helpers.
 * Evaluate array and struct initializers into constant value arrays.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include <string.h>
#include "semantic_init.h"
#include "consteval.h"
#include "error.h"

/*
 * Free a temporary values array and signal failure.  The error location
 * must be set by the caller before invoking this helper.
 */
static int cleanup_and_return(long long *vals)
{
    free(vals);
    return 0;
}

/*
 * Validate an array initializer entry and compute its destination index.
 * On success the index is stored in 'idx' and 'cur' is updated for the next
 * entry.  On failure an error is recorded and zero is returned.
 */
static int validate_array_entry(init_entry_t *ent, size_t array_size,
                                symtable_t *vars, size_t line,
                                size_t column, size_t *cur,
                                size_t *idx)
{
    size_t i = *cur;
    if (ent->kind == INIT_INDEX) {
        long long cidx;
        if (!eval_const_expr(ent->index, vars, &cidx) || cidx < 0 ||
            (size_t)cidx >= array_size) {
            error_set(ent->index->line, ent->index->column);
            return 0;
        }
        i = (size_t)cidx;
        *cur = i;
    } else if (ent->kind == INIT_FIELD) {
        error_set(line, column);
        return 0;
    }
    if (i >= array_size) {
        error_set(line, column);
        return 0;
    }
    *idx = i;
    return 1;
}

/*
 * Resolve a struct initializer entry to its member index.  Sequential
 * entries consume the next field, while field designators search by name.
 * On success the index is written to 'idx' and 'cur' updated accordingly.
 * Returns zero and records an error on failure.
 */
static int resolve_struct_field(init_entry_t *ent, symbol_t *sym,
                                size_t line, size_t column, size_t *cur,
                                size_t *idx)
{
    size_t i = *cur;
    if (ent->kind == INIT_FIELD) {
        int found = 0;
        for (size_t j = 0; j < sym->struct_member_count; j++) {
            if (strcmp(sym->struct_members[j].name, ent->field) == 0) {
                i = j;
                found = 1;
                break;
            }
        }
        if (!found) {
            error_set(line, column);
            return 0;
        }
        *cur = i;
    } else if (ent->kind != INIT_SIMPLE) {
        error_set(line, column);
        return 0;
    }
    if (i >= sym->struct_member_count) {
        error_set(line, column);
        return 0;
    }
    *idx = i;
    return 1;
}

/*
 * Expand an array initializer list. Returns non-zero on success and
 * stores a newly allocated array of `array_size` constant values in
 * `out_vals`. Unspecified elements are zero initialized. The caller is
 * responsible for freeing the returned array.
 */
int expand_array_initializer(init_entry_t *entries, size_t count,
                             size_t array_size, symtable_t *vars,
                             size_t line, size_t column,
                             long long **out_vals)
{
    if (!out_vals)
        return 0;
    if (array_size < count) {
        error_set(line, column);
        return 0;
    }
    long long *vals = calloc(array_size, sizeof(long long));
    if (!vals)
        return 0;
    size_t cur = 0;
    for (size_t i = 0; i < count; i++) {
        init_entry_t *ent = &entries[i];
        size_t idx;
        if (!validate_array_entry(ent, array_size, vars, line, column,
                                  &cur, &idx))
            return cleanup_and_return(vals);
        long long val;
        if (!eval_const_expr(ent->value, vars, &val)) {
            error_set(ent->value->line, ent->value->column);
            return cleanup_and_return(vals);
        }
        vals[idx] = val;
        cur = idx + 1;
    }
    *out_vals = vals;
    return 1;
}

/*
 * Expand a struct initializer list. The resulting array contains one
 * value for each struct member in declaration order. Missing members are
 * zero initialized. The caller frees the returned array.
 */
int expand_struct_initializer(init_entry_t *entries, size_t count,
                              symbol_t *sym, symtable_t *vars,
                              size_t line, size_t column,
                              long long **out_vals)
{
    if (!out_vals || !sym || !sym->struct_member_count) {
        error_set(line, column);
        return 0;
    }
    long long *vals = calloc(sym->struct_member_count, sizeof(long long));
    if (!vals)
        return 0;
    size_t cur = 0;
    for (size_t i = 0; i < count; i++) {
        init_entry_t *ent = &entries[i];
        size_t idx;
        if (!resolve_struct_field(ent, sym, line, column, &cur, &idx))
            return cleanup_and_return(vals);
        long long val;
        if (!eval_const_expr(ent->value, vars, &val)) {
            error_set(ent->value->line, ent->value->column);
            return cleanup_and_return(vals);
        }
        vals[idx] = val;
        cur = idx + 1;
    }
    *out_vals = vals;
    return 1;
}

