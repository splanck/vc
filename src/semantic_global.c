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
#include "semantic_switch.h"
#include "consteval.h"
#include "semantic_expr.h"
#include "semantic_init.h"
#include "symtable.h"
#include "ir_global.h"
#include "util.h"
#include "label.h"
#include "error.h"
#include "preproc_macros.h"
#include <limits.h>
#include <stdint.h>

/* Active struct packing alignment (0 means natural) */
size_t semantic_pack_alignment = 0;

void semantic_set_pack(size_t align)
{
    semantic_pack_alignment = align;
}

/* Track inline functions already emitted */
static const char **inline_emitted = NULL;
static size_t inline_emitted_count = 0;

/*
 * Lay out union members sequentially and return the size of the largest
 * member.  Offsets are assigned in declaration order so that later code
 * can address the correct union fields.
 */
size_t layout_union_members(union_member_t *members, size_t count)
{
    size_t max = 0;
    for (size_t i = 0; i < count; i++) {
        members[i].offset = 0;
        members[i].bit_offset = 0;
        if (members[i].elem_size > max)
            max = members[i].elem_size;
    }
    return max;
}

/*
 * Compute byte offsets for struct members sequentially and return the
 * total size of the struct.  Each member's offset accumulates the size
 * of the previous ones.
 */
size_t layout_struct_members(struct_member_t *members, size_t count)
{
    size_t byte_off = 0;
    unsigned bit_off = 0;
    size_t pack = semantic_pack_alignment ? semantic_pack_alignment : SIZE_MAX;

    for (size_t i = 0; i < count; i++) {
        if (members[i].bit_width == 0) {
            size_t align = members[i].elem_size;
            if (align > pack)
                align = pack;
            if (bit_off)
                byte_off++, bit_off = 0;
            if (align > 1) {
                size_t rem = byte_off % align;
                if (rem)
                    byte_off += align - rem;
            }
            members[i].offset = byte_off;
            members[i].bit_offset = 0;
            if (!members[i].is_flexible)
                byte_off += members[i].elem_size;
        } else {
            members[i].offset = byte_off;
            members[i].bit_offset = bit_off;
            bit_off += members[i].bit_width;
            byte_off += bit_off / 8;
            bit_off %= 8;
        }
    }

    if (bit_off)
        byte_off++;

    if (pack != SIZE_MAX && pack > 1) {
        size_t rem = byte_off % pack;
        if (rem)
            byte_off += pack - rem;
    }

    return byte_off;
}

static int inline_already_emitted(const char *name)
{
    for (size_t i = 0; i < inline_emitted_count; i++)
        if (strcmp(inline_emitted[i], name) == 0)
            return 1;
    return 0;
}

static int mark_inline_emitted(const char *name)
{
    const char **tmp = realloc((void *)inline_emitted,
                               (inline_emitted_count + 1) * sizeof(*tmp));
    if (!tmp)
        return 0;
    inline_emitted = tmp;
    inline_emitted[inline_emitted_count++] = vc_strdup(name ? name : "");
    return 1;
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
    preproc_set_function(func->name);

    symbol_t *decl = symtable_lookup(funcs, func->name);
    if (!decl) {
        error_set(0, 0, error_current_file, error_current_function);
        return 0;
    }
    if (decl->is_inline && inline_already_emitted(func->name)) {
        preproc_set_function(NULL);
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

    for (size_t i = 0; i < func->param_count; i++)
        symtable_add_param(&locals, func->param_names[i],
                           func->param_types[i],
                           func->param_elem_sizes ? func->param_elem_sizes[i] : 4,
                           (int)i,
                           func->param_is_restrict ? func->param_is_restrict[i] : 0);

    ir_build_func_begin(ir, func->name);

    label_table_t labels;
    label_table_init(&labels);

    int ok = 1;
    for (size_t i = 0; i < func->body_count && ok; i++)
        ok = check_stmt(func->body[i], &locals, funcs, &labels, ir, func->return_type,
                        NULL, NULL);

    ir_build_func_end(ir);

    label_table_free(&labels);
    locals.globals = NULL;
    symtable_free(&locals);
    if (decl->is_inline && !mark_inline_emitted(func->name)) {
        error_set(0, 0, error_current_file, error_current_function);
        preproc_set_function(NULL);
        error_current_function = NULL;
        return 0;
    }
    preproc_set_function(NULL);
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
    for (size_t i = 0; i < decl->enum_decl.count; i++) {
        enumerator_t *e = &decl->enum_decl.items[i];
        long long val = next;
        if (e->value) {
            if (!eval_const_expr(e->value, globals, 0, &val)) {
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
    if (decl->enum_decl.tag && decl->enum_decl.tag[0])
        symtable_add_enum_tag_global(globals, decl->enum_decl.tag);
    return 1;
}

/*
 * Register a struct type globally and compute its total size.  Member
 * offsets are laid out, the type is inserted into the global symbol
 * table and its computed size is stored.
 */
static int check_struct_decl_global(stmt_t *decl, symtable_t *globals)
{
    size_t total = layout_struct_members(decl->struct_decl.members,
                                         decl->struct_decl.count);
    if (!symtable_add_struct_global(globals, decl->struct_decl.tag,
                                    decl->struct_decl.members,
                                    decl->struct_decl.count)) {
        error_set(decl->line, decl->column, error_current_file, error_current_function);
        return 0;
    }
    symbol_t *stype =
        symtable_lookup_struct(globals, decl->struct_decl.tag);
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
    layout_union_members(decl->union_decl.members, decl->union_decl.count);
    if (!symtable_add_union_global(globals, decl->union_decl.tag,
                                   decl->union_decl.members,
                                   decl->union_decl.count)) {
        error_set(decl->line, decl->column, error_current_file, error_current_function);
        return 0;
    }
    return 1;
}

/* Evaluate a _Static_assert at global scope */
static int check_static_assert_stmt(stmt_t *stmt, symtable_t *globals)
{
    long long val;
    if (!eval_const_expr(stmt->static_assert.expr, globals, 0, &val)) {
        error_set(stmt->static_assert.expr->line, stmt->static_assert.expr->column,
                  error_current_file, error_current_function);
        return 0;
    }
    if (val == 0) {
        error_set(stmt->line, stmt->column, error_current_file, error_current_function);
        error_print(stmt->static_assert.message);
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
    if (decl->var_decl.type == TYPE_UNION) {
        if (decl->var_decl.member_count) {
            size_t max = layout_union_members(decl->var_decl.members,
                                              decl->var_decl.member_count);
            decl->var_decl.elem_size = max;
        } else if (decl->var_decl.tag) {
            symbol_t *utype = symtable_lookup_union(globals, decl->var_decl.tag);
            if (!utype) {
                error_set(decl->line, decl->column, error_current_file,
                          error_current_function);
                return 0;
            }
            decl->var_decl.elem_size = utype->total_size;
        }
    }

    if (decl->var_decl.type == TYPE_STRUCT) {
        if (decl->var_decl.member_count) {
            size_t total =
                layout_struct_members((struct_member_t *)decl->var_decl.members,
                                      decl->var_decl.member_count);
            decl->var_decl.elem_size = total;
        } else if (decl->var_decl.tag) {
            symbol_t *stype = symtable_lookup_struct(globals, decl->var_decl.tag);
            if (!stype) {
                error_set(decl->line, decl->column, error_current_file,
                          error_current_function);
                return 0;
            }
            decl->var_decl.elem_size = stype->struct_total_size;
        }
    }

    return 1;
}

/*
 * Copy union member metadata from the parsed declaration to the newly created
 * symbol.  Allocates arrays and duplicates member names.  Returns non-zero on
 * success.
 */
static int copy_union_metadata(symbol_t *sym, union_member_t *members,
                               size_t count, size_t total)
{
    sym->total_size = total;
    if (!count)
        return 1;
    sym->members = malloc(count * sizeof(*sym->members));
    if (!sym->members)
        return 0;
    sym->member_count = count;
    for (size_t i = 0; i < count; i++) {
        union_member_t *m = &members[i];
        sym->members[i].name = vc_strdup(m->name);
        sym->members[i].type = m->type;
        sym->members[i].elem_size = m->elem_size;
        sym->members[i].offset = m->offset;
        sym->members[i].bit_width = m->bit_width;
        sym->members[i].bit_offset = m->bit_offset;
        sym->members[i].is_flexible = m->is_flexible;
    }
    return 1;
}

/*
 * Copy struct member metadata from the parsed declaration to the symbol.  The
 * member array is duplicated and offsets preserved.  Returns non-zero on
 * success.
 */
static int copy_struct_metadata(symbol_t *sym, struct_member_t *members,
                                size_t count, size_t total)
{
    sym->struct_total_size = total;
    if (!count)
        return 1;
    sym->struct_members = malloc(count * sizeof(*sym->struct_members));
    if (!sym->struct_members)
        return 0;
    sym->struct_member_count = count;
    for (size_t i = 0; i < count; i++) {
        struct_member_t *m = &members[i];
        sym->struct_members[i].name = vc_strdup(m->name);
        sym->struct_members[i].type = m->type;
        sym->struct_members[i].elem_size = m->elem_size;
        sym->struct_members[i].offset = m->offset;
        sym->struct_members[i].bit_width = m->bit_width;
        sym->struct_members[i].bit_offset = m->bit_offset;
        sym->struct_members[i].is_flexible = m->is_flexible;
    }
    return 1;
}

/*
 * Copy aggregate member metadata from the declaration to the inserted symbol
 * record.  Dispatches to the struct or union helper and returns non-zero on
 * success.
 */
static int copy_aggregate_metadata(stmt_t *decl, symbol_t *sym)
{
    if (decl->var_decl.type == TYPE_UNION)
        return copy_union_metadata(sym, decl->var_decl.members,
                                   decl->var_decl.member_count,
                                   decl->var_decl.elem_size);

    if (decl->var_decl.type == TYPE_STRUCT)
        return copy_struct_metadata(sym,
                                    (struct_member_t *)decl->var_decl.members,
                                    decl->var_decl.member_count,
                                    decl->var_decl.elem_size);

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

    if (!symtable_add_global(globals, decl->var_decl.name,
                             decl->var_decl.name, decl->var_decl.type,
                             decl->var_decl.array_size,
                             decl->var_decl.elem_size,
                             decl->var_decl.is_static,
                             decl->var_decl.is_register,
                             decl->var_decl.is_const,
                             decl->var_decl.is_volatile,
                             decl->var_decl.is_restrict)) {
        error_set(decl->line, decl->column, error_current_file, error_current_function);
        return NULL;
    }

    symbol_t *sym = symtable_lookup_global(globals, decl->var_decl.name);

    if (decl->var_decl.init_list && decl->var_decl.type == TYPE_ARRAY &&
        decl->var_decl.array_size == 0) {
        decl->var_decl.array_size = decl->var_decl.init_count;
        sym->array_size = decl->var_decl.array_size;
    }

    if (!copy_aggregate_metadata(decl, sym))
        return NULL;

    sym->func_ret_type = decl->var_decl.func_ret_type;
    sym->func_param_count = decl->var_decl.func_param_count;
    sym->func_variadic = decl->var_decl.func_variadic;
    if (decl->var_decl.func_param_count) {
        sym->func_param_types = malloc(sym->func_param_count * sizeof(type_kind_t));
        if (!sym->func_param_types)
            return NULL;
        for (size_t i = 0; i < sym->func_param_count; i++)
            sym->func_param_types[i] = decl->var_decl.func_param_types[i];
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
    if (decl->var_decl.is_extern)
        return 1;

    if (decl->var_decl.type == TYPE_ARRAY) {
        long long *vals;
        if (!expand_array_initializer(decl->var_decl.init_list,
                                      decl->var_decl.init_count,
                                      decl->var_decl.array_size, globals,
                                      decl->line, decl->column, &vals))
            return 0;
        if (!ir_build_glob_array(ir, decl->var_decl.name, vals,
                                 decl->var_decl.array_size,
                                 decl->var_decl.is_static)) {
            free(vals);
            return 0;
        }
        free(vals);
        return 1;
    }

    if (decl->var_decl.init_list) {
        if (decl->var_decl.type == TYPE_STRUCT) {
            long long *vals;
            if (!expand_struct_initializer(decl->var_decl.init_list,
                                           decl->var_decl.init_count, sym,
                                           globals, decl->line,
                                           decl->column, &vals))
                return 0;
            ir_build_glob_struct(ir, decl->var_decl.name,
                                 (int)decl->var_decl.elem_size,
                                 decl->var_decl.is_static);
            free(vals);
            return 1;
        }
        error_set(decl->line, decl->column, error_current_file, error_current_function);
        return 0;
    }

    if (decl->var_decl.init &&
        decl->var_decl.init->kind == EXPR_UNARY &&
        decl->var_decl.init->unary.op == UNOP_ADDR &&
        decl->var_decl.init->unary.operand->kind == EXPR_IDENT) {
        ir_build_glob_addr(ir, decl->var_decl.name,
                           decl->var_decl.init->unary.operand->ident.name,
                           decl->var_decl.is_static);
        return 1;
    }

    long long value = 0;
    if (decl->var_decl.init) {
        if (!eval_const_expr(decl->var_decl.init, globals, 0, &value)) {
            error_set(decl->var_decl.init->line, decl->var_decl.init->column, error_current_file, error_current_function);
            return 0;
        }
    }

    if (decl->var_decl.type == TYPE_UNION)
        ir_build_glob_union(ir, decl->var_decl.name,
                           (int)decl->var_decl.elem_size,
                           decl->var_decl.is_static);
    else if (decl->var_decl.type == TYPE_STRUCT)
        ir_build_glob_struct(ir, decl->var_decl.name,
                            (int)decl->var_decl.elem_size,
                            decl->var_decl.is_static);
    else
        ir_build_glob_var(ir, decl->var_decl.name, value,
                          decl->var_decl.is_static);

    return 1;
}

static int check_var_decl_global(stmt_t *decl, symtable_t *globals,
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
        if (!symtable_add_typedef_global(globals, decl->typedef_decl.name,
                                         decl->typedef_decl.type,
                                         decl->typedef_decl.array_size,
                                         decl->typedef_decl.elem_size)) {
            error_set(decl->line, decl->column, error_current_file, error_current_function);
            return 0;
        }
        return 1;
    case STMT_VAR_DECL:
        return check_var_decl_global(decl, globals, ir);
    default:
        break;
    }

    return 0;
}

void semantic_global_cleanup(void)
{
    for (size_t i = 0; i < inline_emitted_count; i++)
        free((void *)inline_emitted[i]);
    free((void *)inline_emitted);
    inline_emitted = NULL;
    inline_emitted_count = 0;
}

