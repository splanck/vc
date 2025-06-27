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
        size_t idx = cur;
        if (ent->kind == INIT_INDEX) {
            long long cidx;
            if (!eval_const_expr(ent->index, vars, &cidx) || cidx < 0 ||
                (size_t)cidx >= array_size) {
                free(vals);
                error_set(ent->index->line, ent->index->column);
                return 0;
            }
            idx = (size_t)cidx;
            cur = idx;
        } else if (ent->kind == INIT_FIELD) {
            free(vals);
            error_set(line, column);
            return 0;
        }
        long long val;
        if (!eval_const_expr(ent->value, vars, &val)) {
            free(vals);
            error_set(ent->value->line, ent->value->column);
            return 0;
        }
        if (idx >= array_size) {
            free(vals);
            error_set(line, column);
            return 0;
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
        size_t idx = cur;
        if (ent->kind == INIT_FIELD) {
            int found = 0;
            for (size_t j = 0; j < sym->struct_member_count; j++) {
                if (strcmp(sym->struct_members[j].name, ent->field) == 0) {
                    idx = j;
                    found = 1;
                    break;
                }
            }
            if (!found) {
                free(vals);
                error_set(line, column);
                return 0;
            }
            cur = idx;
        } else if (ent->kind != INIT_SIMPLE) {
            free(vals);
            error_set(line, column);
            return 0;
        } else if (idx >= sym->struct_member_count) {
            free(vals);
            error_set(line, column);
            return 0;
        }
        long long val;
        if (!eval_const_expr(ent->value, vars, &val)) {
            free(vals);
            error_set(ent->value->line, ent->value->column);
            return 0;
        }
        vals[idx] = val;
        cur = idx + 1;
    }
    *out_vals = vals;
    return 1;
}

