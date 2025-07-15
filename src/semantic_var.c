/*
 * Local variable initialization and layout helpers.
 * Contains routines extracted from statement semantics for
 * computing layouts and emitting IR for variable initialization.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include <string.h>
#include "semantic_var.h"
#include "semantic_init.h"
#include "semantic_expr.h"
#include "semantic_global.h"
#include "semantic_layout.h"
#include "consteval.h"
#include "symtable.h"
#include "semantic.h"
#include "ir_core.h"
#include "ir_global.h"
#include "util.h"
#include "error.h"

/* Helpers for variable initialization */
/*
 * Emit IR for a static array initialized with constant values.  The
 * array contents are provided up front and written directly to the
 * global data section.
 */
static int init_static_array(ir_builder_t *ir, const char *name,
                             const long long *vals, size_t count)
{
    return ir_build_glob_array(ir, name, vals, count, 1, 0);
}

/*
 * Store constant values into a dynamic array variable.  Each element is
 * written with either a volatile or normal store depending on the
 * declaration.
 */
static void init_dynamic_array(ir_builder_t *ir, const char *name,
                               const long long *vals, size_t count,
                               int is_volatile)
{
    for (size_t i = 0; i < count; i++) {
        ir_value_t idxv = ir_build_const(ir, (int)i);
        ir_value_t valv = ir_build_const(ir, vals[i]);
        if (is_volatile)
            ir_build_store_idx_vol(ir, name, idxv, valv);
        else
            ir_build_store_idx(ir, name, idxv, valv);
    }
}

/*
 * Store a constant initializer into a struct field.  The address of the
 * field is computed using the provided base pointer and offset and the
 * constant value is written through that pointer.
 */
static void init_struct_member(ir_builder_t *ir, ir_value_t base,
                               size_t off, long long val)
{
    ir_value_t offv = ir_build_const(ir, (int)off);
    ir_value_t addr = ir_build_ptr_add(ir, base, offv, 1);
    ir_value_t valv = ir_build_const(ir, val);
    ir_build_store_ptr(ir, addr, valv);
}

/*
 * Expand an initializer list for an array variable.  The initializer
 * entries are evaluated, converted to constant values and stored either
 * at sequential indices or at explicit positions.  The resulting array
 * is then emitted using the appropriate helper.
 */
static int handle_array_init(stmt_t *stmt, symbol_t *sym, symtable_t *vars,
                             ir_builder_t *ir)
{
    long long *vals;
    if (!expand_array_initializer(STMT_VAR_DECL(stmt).init_list, STMT_VAR_DECL(stmt).init_count,
                                  sym->array_size, vars, stmt->line, stmt->column,
                                  &vals))
        return 0;
    if (STMT_VAR_DECL(stmt).is_static) {
        if (!init_static_array(ir, sym->ir_name, vals, sym->array_size)) {
            free(vals);
            return 0;
        }
    }
    else
        init_dynamic_array(ir, sym->ir_name, vals, sym->array_size,
                           STMT_VAR_DECL(stmt).is_volatile);
    free(vals);
    return 1;
}

/*
 * Expand an initializer list for a struct variable.  Each provided
 * initializer assigns to the next field or to a named field.  Constant
 * values are stored into the computed member addresses of the struct
 * variable.
 */
static int handle_struct_init(stmt_t *stmt, symbol_t *sym, symtable_t *vars,
                              ir_builder_t *ir)
{
    long long *vals;
    if (!expand_struct_initializer(STMT_VAR_DECL(stmt).init_list, STMT_VAR_DECL(stmt).init_count,
                                   sym, vars, stmt->line, stmt->column, &vals))
        return 0;
    ir_value_t base = ir_build_addr(ir, sym->ir_name);
    for (size_t i = 0; i < sym->struct_member_count; i++)
        init_struct_member(ir, base, sym->struct_members[i].offset, vals[i]);
    free(vals);
    return 1;
}

/*
 * Compute layout information for an aggregate variable.  Struct and
 * union members are assigned offsets and the resulting element size is
 * stored back into the declaration.
 */
int compute_var_layout(stmt_t *stmt, symtable_t *vars)
{
    if (STMT_VAR_DECL(stmt).type == TYPE_UNION) {
        if (STMT_VAR_DECL(stmt).member_count) {
            size_t max = layout_union_members(STMT_VAR_DECL(stmt).members,
                                              STMT_VAR_DECL(stmt).member_count);
            STMT_VAR_DECL(stmt).elem_size = max;
        } else if (STMT_VAR_DECL(stmt).tag) {
            symbol_t *utype = symtable_lookup_union(vars, STMT_VAR_DECL(stmt).tag);
            if (!utype) {
                error_set(&error_ctx,stmt->line, stmt->column, NULL, NULL);
                return 0;
            }
            STMT_VAR_DECL(stmt).elem_size = utype->total_size;
        }
    }

    if (STMT_VAR_DECL(stmt).type == TYPE_STRUCT) {
        if (STMT_VAR_DECL(stmt).member_count) {
            size_t total = layout_struct_members(
                (struct_member_t *)STMT_VAR_DECL(stmt).members,
                STMT_VAR_DECL(stmt).member_count);
            STMT_VAR_DECL(stmt).elem_size = total;
        } else if (STMT_VAR_DECL(stmt).tag) {
            symbol_t *stype = symtable_lookup_struct(vars, STMT_VAR_DECL(stmt).tag);
            if (!stype) {
                error_set(&error_ctx,stmt->line, stmt->column, NULL, NULL);
                return 0;
            }
            STMT_VAR_DECL(stmt).elem_size = stype->struct_total_size;
        }
    }

    return 1;
}

/*
 * Emit IR for a static initializer using a constant expression.
 * Handles scalars and aggregates placed in the global data section.
 */
static int emit_static_initializer(stmt_t *stmt, symbol_t *sym,
                                   symtable_t *vars, ir_builder_t *ir)
{
    long long cval;
    if (!eval_const_expr(STMT_VAR_DECL(stmt).init, vars,
                        semantic_get_x86_64(), &cval)) {
        error_set(&error_ctx,STMT_VAR_DECL(stmt).init->line, STMT_VAR_DECL(stmt).init->column, NULL, NULL);
        return 0;
    }
    if (STMT_VAR_DECL(stmt).type == TYPE_UNION)
        ir_build_glob_union(ir, sym->ir_name, (int)sym->elem_size, 1,
                           sym->alignment);
    else if (STMT_VAR_DECL(stmt).type == TYPE_STRUCT)
        ir_build_glob_struct(ir, sym->ir_name, (int)sym->struct_total_size, 1,
                            sym->alignment);
    else
        ir_build_glob_var(ir, sym->ir_name, cval, 1, sym->alignment);
    return 1;
}

/*
 * Emit IR for a dynamic initializer evaluated at runtime.
 * The expression is checked for type compatibility and then stored.
 */
static int emit_dynamic_initializer(stmt_t *stmt, symbol_t *sym,
                                    symtable_t *vars, symtable_t *funcs,
                                    ir_builder_t *ir)
{
    ir_value_t val;
    type_kind_t vt = check_expr(STMT_VAR_DECL(stmt).init, vars, funcs, ir, &val);
    if (!(((is_intlike(STMT_VAR_DECL(stmt).type) && is_intlike(vt)) ||
           (is_floatlike(STMT_VAR_DECL(stmt).type) &&
            (is_floatlike(vt) || is_intlike(vt)))) ||
          vt == STMT_VAR_DECL(stmt).type)) {
        error_set(&error_ctx,STMT_VAR_DECL(stmt).init->line, STMT_VAR_DECL(stmt).init->column, NULL, NULL);
        return 0;
    }
    if (STMT_VAR_DECL(stmt).is_volatile)
        ir_build_store_vol(ir, sym->ir_name, val);
    else
        ir_build_store(ir, sym->ir_name, val);
    return 1;
}

/*
 * Emit IR for an initializer list of an aggregate variable.
 * Dispatches to the array or struct handler as appropriate.
 */
static int emit_aggregate_initializer(stmt_t *stmt, symbol_t *sym,
                                      symtable_t *vars, ir_builder_t *ir)
{
    if (STMT_VAR_DECL(stmt).type == TYPE_ARRAY)
        return handle_array_init(stmt, sym, vars, ir);
    if (STMT_VAR_DECL(stmt).type == TYPE_STRUCT)
        return handle_struct_init(stmt, sym, vars, ir);
    error_set(&error_ctx,stmt->line, stmt->column, NULL, NULL);
    return 0;
}
/*
 * Evaluate the element count of a variable-length array and allocate
 * the required storage on the stack.  The resulting base pointer and
 * runtime length are stored in the symbol record.
 */
int handle_vla_size(stmt_t *stmt, symbol_t *sym, symtable_t *vars,
                    symtable_t *funcs, ir_builder_t *ir)
{
    ir_value_t lenv;
    if (check_expr(STMT_VAR_DECL(stmt).size_expr, vars, funcs, ir, &lenv) ==
        TYPE_UNKNOWN)
        return 0;
    ir_value_t eszv = ir_build_const(ir, (int)STMT_VAR_DECL(stmt).elem_size);
    ir_value_t total = ir_build_binop(ir, IR_MUL, lenv, eszv);
    sym->vla_addr = ir_build_alloca(ir, total);
    sym->vla_size = lenv;
    return 1;
}

/*
 * Case 1: initialization with a constant expression.  The value can be
 * emitted directly into the data section and requires no runtime code.
 */
static int emit_static_init(stmt_t *stmt, symbol_t *sym, symtable_t *vars,
                            ir_builder_t *ir)
{
    return emit_static_initializer(stmt, sym, vars, ir);
}

/*
 * Case 2: scalar initializer evaluated at runtime for automatic storage.
 * The initializer expression is validated and stored through generated IR.
 */
static int emit_dynamic_init(stmt_t *stmt, symbol_t *sym, symtable_t *vars,
                             symtable_t *funcs, ir_builder_t *ir)
{
    return emit_dynamic_initializer(stmt, sym, vars, funcs, ir);
}

/*
 * Case 3: initializer list for an array or struct.  Elements are expanded
 * and stored sequentially via the aggregate initializer helper.
 */
static int emit_aggregate_init(stmt_t *stmt, symbol_t *sym,
                               symtable_t *vars, ir_builder_t *ir)
{
    return emit_aggregate_initializer(stmt, sym, vars, ir);
}

/*
 * Emit IR for any initializer attached to the variable.
 * Copies aggregate member metadata to the symbol and writes
 * constant or computed values using the IR builder.
 * Returns non-zero on success.
 */
int emit_var_initializer(stmt_t *stmt, symbol_t *sym,
                         symtable_t *vars, symtable_t *funcs,
                         ir_builder_t *ir)
{
    enum {
        INIT_NONE,
        INIT_STATIC,
        INIT_DYNAMIC,
        INIT_AGGREGATE
    } kind = INIT_NONE;

    if (!copy_aggregate_metadata(stmt, sym, vars))
        return 0;

    if (STMT_VAR_DECL(stmt).init)
        kind = STMT_VAR_DECL(stmt).is_static ? INIT_STATIC : INIT_DYNAMIC;
    else if (STMT_VAR_DECL(stmt).init_list)
        kind = INIT_AGGREGATE;

    switch (kind) {
    case INIT_STATIC:
        return emit_static_init(stmt, sym, vars, ir);
    case INIT_DYNAMIC:
        return emit_dynamic_init(stmt, sym, vars, funcs, ir);
    case INIT_AGGREGATE:
        return emit_aggregate_init(stmt, sym, vars, ir);
    default:
        return 1;
    }
}

