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
#include "symtable.h"
#include "ir_global.h"
#include "util.h"
#include "label.h"
#include "error.h"
#include <limits.h>

/*
 * Lay out union members sequentially and return the size of the largest
 * member.  Offsets are assigned in declaration order so that later code
 * can address the correct union fields.
 */
size_t layout_union_members(union_member_t *members, size_t count)
{
    size_t off = 0;
    size_t max = 0;
    for (size_t i = 0; i < count; i++) {
        members[i].offset = off;
        off += members[i].elem_size;
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
    size_t off = 0;
    for (size_t i = 0; i < count; i++) {
        members[i].offset = off;
        off += members[i].elem_size;
    }
    return off;
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

    symbol_t *decl = symtable_lookup(funcs, func->name);
    if (!decl) {
        error_set(0, 0);
        return 0;
    }
    int mismatch = decl->type != func->return_type ||
                   decl->param_count != func->param_count;
    for (size_t i = 0; i < decl->param_count && !mismatch; i++)
        if (decl->param_types[i] != func->param_types[i])
            mismatch = 1;
    if (mismatch) {
        error_set(0, 0);
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
            if (!eval_const_expr(e->value, globals, &val)) {
                error_set(e->value->line, e->value->column);
                return 0;
            }
        }
        if (!symtable_add_enum_global(globals, e->name, (int)val)) {
            error_set(decl->line, decl->column);
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
        error_set(decl->line, decl->column);
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
        error_set(decl->line, decl->column);
        return 0;
    }
    return 1;
}

/*
 * Process a global variable declaration and emit initialization IR.  The
 * variable is added to the global symbol table and its initializer is
 * translated into constant data where possible.
 */
static int check_var_decl_global(stmt_t *decl, symtable_t *globals,
                                 ir_builder_t *ir)
{
    if (decl->var_decl.type == TYPE_UNION) {
        size_t max = layout_union_members(decl->var_decl.members,
                                          decl->var_decl.member_count);
        decl->var_decl.elem_size = max;
    }
    if (decl->var_decl.type == TYPE_STRUCT) {
        size_t total = layout_struct_members(
            (struct_member_t *)decl->var_decl.members,
            decl->var_decl.member_count);
        if (decl->var_decl.member_count || decl->var_decl.tag)
            decl->var_decl.elem_size = total;
    }
    if (!symtable_add_global(globals, decl->var_decl.name, decl->var_decl.name,
                             decl->var_decl.type,
                             decl->var_decl.array_size,
                             decl->var_decl.elem_size,
                             decl->var_decl.is_static,
                             decl->var_decl.is_register,
                             decl->var_decl.is_const,
                             decl->var_decl.is_volatile,
                             decl->var_decl.is_restrict)) {
        error_set(decl->line, decl->column);
        return 0;
    }
    symbol_t *gsym = symtable_lookup_global(globals, decl->var_decl.name);
    if (decl->var_decl.init_list && decl->var_decl.type == TYPE_ARRAY &&
        decl->var_decl.array_size == 0) {
        decl->var_decl.array_size = decl->var_decl.init_count;
        gsym->array_size = decl->var_decl.array_size;
    }
    if (decl->var_decl.type == TYPE_UNION) {
        gsym->total_size = decl->var_decl.elem_size;
        if (decl->var_decl.member_count) {
            gsym->members = malloc(decl->var_decl.member_count *
                                   sizeof(*gsym->members));
            if (!gsym->members)
                return 0;
            gsym->member_count = decl->var_decl.member_count;
            for (size_t i = 0; i < gsym->member_count; i++) {
                union_member_t *m = &decl->var_decl.members[i];
                gsym->members[i].name = vc_strdup(m->name);
                gsym->members[i].type = m->type;
                gsym->members[i].elem_size = m->elem_size;
                gsym->members[i].offset = m->offset;
            }
        }
    }
    if (decl->var_decl.type == TYPE_STRUCT) {
        gsym->struct_total_size = decl->var_decl.elem_size;
        if (decl->var_decl.member_count) {
            gsym->struct_members = malloc(decl->var_decl.member_count *
                                          sizeof(*gsym->struct_members));
            if (!gsym->struct_members)
                return 0;
            gsym->struct_member_count = decl->var_decl.member_count;
            for (size_t i = 0; i < gsym->struct_member_count; i++) {
                struct_member_t *m =
                    (struct_member_t *)&decl->var_decl.members[i];
                gsym->struct_members[i].name = vc_strdup(m->name);
                gsym->struct_members[i].type = m->type;
                gsym->struct_members[i].elem_size = m->elem_size;
                gsym->struct_members[i].offset = m->offset;
            }
        }
    }
    if (decl->var_decl.is_extern)
        return 1;

    if (decl->var_decl.type == TYPE_ARRAY) {
        size_t count = decl->var_decl.array_size;
        long long *vals = calloc(count, sizeof(long long));
        if (!vals)
            return 0;
        size_t init_count = decl->var_decl.init_count;
        if (init_count > count) {
            free(vals);
            error_set(decl->line, decl->column);
            return 0;
        }
        size_t cur = 0;
        for (size_t i = 0; i < init_count; i++) {
            init_entry_t *ent = &decl->var_decl.init_list[i];
            size_t idx = cur;
            if (ent->kind == INIT_INDEX) {
                long long cidx;
                if (!eval_const_expr(ent->index, globals, &cidx) ||
                    cidx < 0 || (size_t)cidx >= count) {
                    free(vals);
                    error_set(ent->index->line, ent->index->column);
                    return 0;
                }
                idx = (size_t)cidx;
                cur = idx;
            } else if (ent->kind == INIT_FIELD) {
                free(vals);
                error_set(decl->line, decl->column);
                return 0;
            }
            long long val;
            if (!eval_const_expr(ent->value, globals, &val)) {
                free(vals);
                error_set(ent->value->line, ent->value->column);
                return 0;
            }
            if (idx >= count) {
                free(vals);
                error_set(decl->line, decl->column);
                return 0;
            }
            vals[idx] = val;
            cur = idx + 1;
        }
        ir_build_glob_array(ir, decl->var_decl.name, vals, count,
                           decl->var_decl.is_static);
        free(vals);
    } else {
        long long value = 0;
        if (decl->var_decl.init) {
            if (!eval_const_expr(decl->var_decl.init, globals, &value)) {
                error_set(decl->var_decl.init->line,
                          decl->var_decl.init->column);
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
    }
    return 1;
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
    case STMT_TYPEDEF:
        if (!symtable_add_typedef_global(globals, decl->typedef_decl.name,
                                         decl->typedef_decl.type,
                                         decl->typedef_decl.array_size,
                                         decl->typedef_decl.elem_size)) {
            error_set(decl->line, decl->column);
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

