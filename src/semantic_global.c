/*
 * Global and function semantic validation.
 * Routines verify declarations/definitions and build IR for
 * global data and function bodies.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "semantic_global.h"
#include "semantic_stmt.h"
#include "semantic_control.h"
#include "consteval.h"
#include "semantic_expr.h"
#include "semantic_init.h"
#include "semantic_layout.h"
#include "symtable.h"
#include "ir_global.h"
#include "util.h"
#include "semantic_inline.h"
#include "label.h"
#include "error.h"
#include "preproc_macros.h"
#include <limits.h>
#include <stdint.h>

static preproc_context_t func_ctx;

/* Active struct packing alignment (0 means natural) */
size_t semantic_pack_alignment = 0;

/* total bytes of automatic storage for the current function */
int semantic_stack_offset = 0;

/* target bitness for constant evaluation and call conventions */
static int semantic_x86_64 = 0;

void semantic_set_x86_64(int flag)
{
    semantic_x86_64 = flag;
}

int semantic_get_x86_64(void)
{
    return semantic_x86_64;
}

void semantic_set_pack(size_t align)
{
    semantic_pack_alignment = align;
}



/*
 * Validate a function definition against its prior declaration and emit
 * IR for the body.  Parameter types are checked for consistency, local
 * variables are allocated and each statement in the body is validated.
 */
int check_func(func_t *func, symtable_t *funcs, symtable_t *globals,
               ir_builder_t *ir)
{
    if (!func)
        return 0;

    error_current_function = func->name;
    preproc_set_function(&func_ctx, func->name);

    symbol_t *decl = symtable_lookup(funcs, func->name);
    if (!decl) {
        error_set(0, 0, error_current_file, error_current_function);
        return 0;
    }
    if (decl->is_inline && semantic_inline_already_emitted(func->name)) {
        preproc_set_function(&func_ctx, NULL);
        error_current_function = NULL;
        return 1;
    }

    warn_unreachable_function(func, funcs);
    int mismatch = decl->type != func->return_type ||
                   decl->param_count != func->param_count ||
                   decl->is_variadic != func->is_variadic;
    for (size_t i = 0; i < decl->param_count && !mismatch; i++)
        if (decl->param_types[i] != func->param_types[i])
            mismatch = 1;
    if (mismatch) {
        error_set(0, 0, error_current_file, error_current_function);
        return 0;
    }

    symtable_t locals;
    symtable_init(&locals);
    locals.globals = globals ? globals->globals : NULL;
    semantic_stack_offset = 0;

    for (size_t i = 0; i < func->param_count; i++)
        symtable_add_param(&locals, func->param_names[i],
                           func->param_types[i],
                           func->param_elem_sizes ? func->param_elem_sizes[i] : 4,
                           (int)i,
                           func->param_is_restrict ? func->param_is_restrict[i] : 0);

    ir_instr_t *func_begin = ir_build_func_begin(ir, func->name);

    label_table_t labels;
    label_table_init(&labels);

    int ok = 1;
    for (size_t i = 0; i < func->body_count && ok; i++)
        ok = check_stmt(func->body[i], &locals, funcs, &labels, ir, func->return_type,
                        NULL, NULL);

    if (func_begin)
        func_begin->imm = semantic_stack_offset;
    ir_build_func_end(ir);

    label_table_free(&labels);
    locals.globals = NULL;
    symtable_free(&locals);
    if (decl->is_inline && !semantic_mark_inline_emitted(func->name)) {
        error_set(0, 0, error_current_file, error_current_function);
        preproc_set_function(&func_ctx, NULL);
        error_current_function = NULL;
        return 0;
    }
    preproc_set_function(&func_ctx, NULL);
    error_current_function = NULL;
    return ok;
}

/*
 * Add enumeration constants and an optional tag to the global symbol
 * table.  Each enumerator's constant value is determined and recorded
 * so that the enumeration can be referenced throughout the program.
 */
static int check_enum_decl_global(stmt_t *decl, symtable_t *globals)
{
    int next = 0;
    for (size_t i = 0; i < STMT_ENUM_DECL(decl).count; i++) {
        enumerator_t *e = &STMT_ENUM_DECL(decl).items[i];
        long long val = next;
        if (e->value) {
            if (!eval_const_expr(e->value, globals,
                                 semantic_get_x86_64(), &val)) {
                error_set(e->value->line, e->value->column, error_current_file, error_current_function);
                return 0;
            }
        }
        if (!symtable_add_enum_global(globals, e->name, (int)val)) {
            error_set(decl->line, decl->column, error_current_file, error_current_function);
            return 0;
        }
        next = (int)val + 1;
    }
    if (STMT_ENUM_DECL(decl).tag && STMT_ENUM_DECL(decl).tag[0])
        symtable_add_enum_tag_global(globals, STMT_ENUM_DECL(decl).tag);
    return 1;
}

/*
 * Register a struct type globally and compute its total size.  Member
 * offsets are laid out, the type is inserted into the global symbol
 * table and its computed size is stored.
 */
static int check_struct_decl_global(stmt_t *decl, symtable_t *globals)
{
    size_t total = layout_struct_members(STMT_STRUCT_DECL(decl).members,
                                         STMT_STRUCT_DECL(decl).count);
    if (!symtable_add_struct_global(globals, STMT_STRUCT_DECL(decl).tag,
                                    STMT_STRUCT_DECL(decl).members,
                                    STMT_STRUCT_DECL(decl).count)) {
        error_set(decl->line, decl->column, error_current_file, error_current_function);
        return 0;
    }
    symbol_t *stype =
        symtable_lookup_struct(globals, STMT_STRUCT_DECL(decl).tag);
    if (stype)
        stype->struct_total_size = total;
    return 1;
}

/*
 * Register a union type globally after laying out its members.  The
 * layout helper assigns offsets and the resulting type is stored in the
 * global symbol table for later use.
 */
static int check_union_decl_global(stmt_t *decl, symtable_t *globals)
{
    layout_union_members(STMT_UNION_DECL(decl).members, STMT_UNION_DECL(decl).count);
    if (!symtable_add_union_global(globals, STMT_UNION_DECL(decl).tag,
                                   STMT_UNION_DECL(decl).members,
                                   STMT_UNION_DECL(decl).count)) {
        error_set(decl->line, decl->column, error_current_file, error_current_function);
        return 0;
    }
    return 1;
}

/* Evaluate a _Static_assert at global scope */
static int check_static_assert_stmt(stmt_t *stmt, symtable_t *globals)
{
    long long val;
    if (!eval_const_expr(STMT_STATIC_ASSERT(stmt).expr, globals,
                         semantic_get_x86_64(), &val)) {
        error_set(STMT_STATIC_ASSERT(stmt).expr->line, STMT_STATIC_ASSERT(stmt).expr->column,
                  error_current_file, error_current_function);
        return 0;
    }
    if (val == 0) {
        error_set(stmt->line, stmt->column, error_current_file, error_current_function);
        error_print(STMT_STATIC_ASSERT(stmt).message);
        return 0;
    }
    return 1;
}

/*
 * Compute layout information for an aggregate global variable.  Struct and
 * union members are assigned offsets and the resulting element size is stored
 * back into the declaration for later use.
 */

static int compute_global_layout(stmt_t *decl, symtable_t *globals)
{
    if (STMT_VAR_DECL(decl).type == TYPE_UNION)
        return compute_union_layout(decl, globals);
    if (STMT_VAR_DECL(decl).type == TYPE_STRUCT)
        return compute_struct_layout(decl, globals);
    return 1;
}

/*
 * Register a global variable in the symbol table.
 * Layout any aggregate members and copy them to the inserted symbol.
 * Returns the created symbol or NULL on failure.
 */
static symbol_t *register_global_symbol(stmt_t *decl, symtable_t *globals)
{
    if (!compute_global_layout(decl, globals))
        return NULL;

    if (STMT_VAR_DECL(decl).align_expr) {
        long long aval;
        if (!eval_const_expr(STMT_VAR_DECL(decl).align_expr, globals,
                             semantic_get_x86_64(), &aval) ||
            aval <= 0 || (aval & (aval - 1))) {
            error_set(STMT_VAR_DECL(decl).align_expr->line,
                      STMT_VAR_DECL(decl).align_expr->column,
                      error_current_file, error_current_function);
            error_print("Invalid alignment");
            return NULL;
        }
        STMT_VAR_DECL(decl).alignment = (size_t)aval;
    }

    if (!symtable_add_global(globals, STMT_VAR_DECL(decl).name,
                             STMT_VAR_DECL(decl).name, STMT_VAR_DECL(decl).type,
                             STMT_VAR_DECL(decl).array_size,
                             STMT_VAR_DECL(decl).elem_size,
                             STMT_VAR_DECL(decl).alignment,
                             STMT_VAR_DECL(decl).is_static,
                             STMT_VAR_DECL(decl).is_register,
                             STMT_VAR_DECL(decl).is_const,
                             STMT_VAR_DECL(decl).is_volatile,
                             STMT_VAR_DECL(decl).is_restrict)) {
        error_set(decl->line, decl->column, error_current_file, error_current_function);
        return NULL;
    }

    symbol_t *sym = symtable_lookup_global(globals, STMT_VAR_DECL(decl).name);

    if (STMT_VAR_DECL(decl).init_list && STMT_VAR_DECL(decl).type == TYPE_ARRAY &&
        STMT_VAR_DECL(decl).array_size == 0) {
        STMT_VAR_DECL(decl).array_size = STMT_VAR_DECL(decl).init_count;
        sym->array_size = STMT_VAR_DECL(decl).array_size;
    }

    if (!copy_aggregate_metadata(decl, sym, globals))
        return NULL;

    sym->func_ret_type = STMT_VAR_DECL(decl).func_ret_type;
    sym->func_param_count = STMT_VAR_DECL(decl).func_param_count;
    sym->func_variadic = STMT_VAR_DECL(decl).func_variadic;
    if (STMT_VAR_DECL(decl).func_param_count) {
        sym->func_param_types = malloc(sym->func_param_count * sizeof(type_kind_t));
        if (!sym->func_param_types)
            return NULL;
        for (size_t i = 0; i < sym->func_param_count; i++)
            sym->func_param_types[i] = STMT_VAR_DECL(decl).func_param_types[i];
    }

    return sym;
}

/*
 * Emit IR to initialize a global variable.
 * Handles arrays, structs, unions and scalars using constant
 * expressions only. Returns non-zero on success.
 */
static int emit_global_initializer(stmt_t *decl, symbol_t *sym,
                                   symtable_t *globals, ir_builder_t *ir)
{
    if (STMT_VAR_DECL(decl).is_extern)
        return 1;

    if (STMT_VAR_DECL(decl).type == TYPE_ARRAY) {
        long long *vals;
        if (!expand_array_initializer(STMT_VAR_DECL(decl).init_list,
                                      STMT_VAR_DECL(decl).init_count,
                                      STMT_VAR_DECL(decl).array_size, globals,
                                      decl->line, decl->column, &vals))
            return 0;
        if (!ir_build_glob_array(ir, STMT_VAR_DECL(decl).name, vals,
                                 STMT_VAR_DECL(decl).array_size,
                                 STMT_VAR_DECL(decl).is_static,
                                 STMT_VAR_DECL(decl).alignment)) {
            free(vals);
            return 0;
        }
        free(vals);
        return 1;
    }

    if (STMT_VAR_DECL(decl).init_list) {
        if (STMT_VAR_DECL(decl).type == TYPE_STRUCT) {
            long long *vals;
            if (!expand_struct_initializer(STMT_VAR_DECL(decl).init_list,
                                           STMT_VAR_DECL(decl).init_count, sym,
                                           globals, decl->line,
                                           decl->column, &vals))
                return 0;
            ir_build_glob_struct(ir, STMT_VAR_DECL(decl).name,
                                 (int)STMT_VAR_DECL(decl).elem_size,
                                 STMT_VAR_DECL(decl).is_static,
                                 STMT_VAR_DECL(decl).alignment);
            free(vals);
            return 1;
        }
        error_set(decl->line, decl->column, error_current_file, error_current_function);
        return 0;
    }

    if (STMT_VAR_DECL(decl).init &&
        STMT_VAR_DECL(decl).init->kind == EXPR_UNARY &&
        STMT_VAR_DECL(decl).init->data.unary.op == UNOP_ADDR &&
        STMT_VAR_DECL(decl).init->data.unary.operand->kind == EXPR_IDENT) {
        ir_build_glob_addr(ir, STMT_VAR_DECL(decl).name,
                           STMT_VAR_DECL(decl).init->data.unary.operand->data.ident.name,
                           STMT_VAR_DECL(decl).is_static);
        return 1;
    }

    long long value = 0;
    if (STMT_VAR_DECL(decl).init) {
        if (!eval_const_expr(STMT_VAR_DECL(decl).init, globals,
                             semantic_get_x86_64(), &value)) {
            error_set(STMT_VAR_DECL(decl).init->line, STMT_VAR_DECL(decl).init->column, error_current_file, error_current_function);
            return 0;
        }
    }

    if (STMT_VAR_DECL(decl).type == TYPE_UNION)
        ir_build_glob_union(ir, STMT_VAR_DECL(decl).name,
                           (int)STMT_VAR_DECL(decl).elem_size,
                           STMT_VAR_DECL(decl).is_static,
                           STMT_VAR_DECL(decl).alignment);
    else if (STMT_VAR_DECL(decl).type == TYPE_STRUCT)
        ir_build_glob_struct(ir, STMT_VAR_DECL(decl).name,
                            (int)STMT_VAR_DECL(decl).elem_size,
                            STMT_VAR_DECL(decl).is_static,
                            STMT_VAR_DECL(decl).alignment);
    else
        ir_build_glob_var(ir, STMT_VAR_DECL(decl).name, value,
                          STMT_VAR_DECL(decl).is_static,
                          STMT_VAR_DECL(decl).alignment);

    return 1;
}

/*
 * Register a global variable symbol and emit its initializer.  Returns
 * non-zero on success.
 */
static int emit_global_symbol(stmt_t *decl, symtable_t *globals,
                              ir_builder_t *ir)
{
    symbol_t *sym = register_global_symbol(decl, globals);
    if (!sym)
        return 0;
    return emit_global_initializer(decl, sym, globals, ir);
}

/*
 * Validate a global declaration and emit the IR needed to initialize
 * it.  Depending on the kind of declaration the appropriate helper is
 * invoked.  Only constant expressions are permitted for globals.
 */
int check_global(stmt_t *decl, symtable_t *globals, ir_builder_t *ir)
{
    if (!decl)
        return 0;

    switch (decl->kind) {
    case STMT_ENUM_DECL:
        return check_enum_decl_global(decl, globals);
    case STMT_STRUCT_DECL:
        return check_struct_decl_global(decl, globals);
    case STMT_UNION_DECL:
        return check_union_decl_global(decl, globals);
    case STMT_STATIC_ASSERT:
        return check_static_assert_stmt(decl, globals);
    case STMT_TYPEDEF:
        if (!symtable_add_typedef_global(globals, STMT_TYPEDEF(decl).name,
                                         STMT_TYPEDEF(decl).type,
                                         STMT_TYPEDEF(decl).array_size,
                                         STMT_TYPEDEF(decl).elem_size)) {
            error_set(decl->line, decl->column, error_current_file, error_current_function);
            return 0;
        }
        return 1;
    case STMT_VAR_DECL:
        return emit_global_symbol(decl, globals, ir);
    default:
        break;
    }

    return 0;
}

void semantic_global_cleanup(void)
{
    semantic_inline_cleanup();
}

