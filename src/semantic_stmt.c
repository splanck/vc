/*
 * Statement dispatch and unreachable warning helpers.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include "semantic_stmt.h"
#include "semantic_loops.h"
#include "semantic_control.h"
#include "semantic_decl_stmt.h"
#include "label.h"
#include "error.h"
#include <string.h>

bool semantic_warn_unreachable = true;

/* Handlers implemented in dedicated modules */
extern int stmt_expr_handler(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
                             label_table_t *labels, ir_builder_t *ir,
                             type_kind_t func_ret_type,
                             const char *break_label,
                             const char *continue_label);
extern int stmt_return_handler(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
                               label_table_t *labels, ir_builder_t *ir,
                               type_kind_t func_ret_type,
                               const char *break_label,
                               const char *continue_label);
extern int stmt_if_handler(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
                           label_table_t *labels, ir_builder_t *ir,
                           type_kind_t func_ret_type,
                           const char *break_label,
                           const char *continue_label);
extern int stmt_while_handler(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
                              label_table_t *labels, ir_builder_t *ir,
                              type_kind_t func_ret_type,
                              const char *break_label,
                              const char *continue_label);
extern int stmt_do_while_handler(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
                                 label_table_t *labels, ir_builder_t *ir,
                                 type_kind_t func_ret_type,
                                 const char *break_label,
                                 const char *continue_label);
extern int stmt_for_handler(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
                            label_table_t *labels, ir_builder_t *ir,
                            type_kind_t func_ret_type,
                            const char *break_label,
                            const char *continue_label);
extern int stmt_switch_handler(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
                               label_table_t *labels, ir_builder_t *ir,
                               type_kind_t func_ret_type,
                               const char *break_label,
                               const char *continue_label);
extern int stmt_break_handler(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
                              label_table_t *labels, ir_builder_t *ir,
                              type_kind_t func_ret_type,
                              const char *break_label,
                              const char *continue_label);
extern int stmt_continue_handler(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
                                 label_table_t *labels, ir_builder_t *ir,
                                 type_kind_t func_ret_type,
                                 const char *break_label,
                                 const char *continue_label);
extern int stmt_label_handler(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
                              label_table_t *labels, ir_builder_t *ir,
                              type_kind_t func_ret_type,
                              const char *break_label,
                              const char *continue_label);
extern int stmt_goto_handler(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
                             label_table_t *labels, ir_builder_t *ir,
                             type_kind_t func_ret_type,
                             const char *break_label,
                             const char *continue_label);
extern int stmt_static_assert_handler(stmt_t *stmt, symtable_t *vars,
                                      symtable_t *funcs, label_table_t *labels,
                                      ir_builder_t *ir,
                                      type_kind_t func_ret_type,
                                      const char *break_label,
                                      const char *continue_label);
extern int stmt_typedef_handler(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
                                label_table_t *labels, ir_builder_t *ir,
                                type_kind_t func_ret_type,
                                const char *break_label,
                                const char *continue_label);
extern int stmt_block_handler(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
                              label_table_t *labels, ir_builder_t *ir,
                              type_kind_t func_ret_type,
                              const char *break_label,
                              const char *continue_label);

typedef int (*stmt_handler_fn)(stmt_t *, symtable_t *, symtable_t *,
                               label_table_t *, ir_builder_t *,
                               type_kind_t, const char *, const char *);

static stmt_handler_fn stmt_handlers[] = {
    [STMT_EXPR]        = stmt_expr_handler,
    [STMT_RETURN]      = stmt_return_handler,
    [STMT_VAR_DECL]    = stmt_var_decl_handler,
    [STMT_IF]          = stmt_if_handler,
    [STMT_WHILE]       = stmt_while_handler,
    [STMT_DO_WHILE]    = stmt_do_while_handler,
    [STMT_FOR]         = stmt_for_handler,
    [STMT_SWITCH]      = stmt_switch_handler,
    [STMT_BREAK]       = stmt_break_handler,
    [STMT_CONTINUE]    = stmt_continue_handler,
    [STMT_LABEL]       = stmt_label_handler,
    [STMT_GOTO]        = stmt_goto_handler,
    [STMT_STATIC_ASSERT] = stmt_static_assert_handler,
    [STMT_TYPEDEF]     = stmt_typedef_handler,
    [STMT_ENUM_DECL]   = stmt_enum_decl_handler,
    [STMT_STRUCT_DECL] = stmt_struct_decl_handler,
    [STMT_UNION_DECL]  = stmt_union_decl_handler,
    [STMT_BLOCK]       = stmt_block_handler,
};

int check_stmt(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
               void *label_tab, ir_builder_t *ir, type_kind_t func_ret_type,
               const char *break_label, const char *continue_label)
{
    label_table_t *labels = (label_table_t *)label_tab;
    if (!stmt)
        return 0;
    ir_builder_set_loc(ir, error_current_file, stmt->line, stmt->column);
    if ((size_t)stmt->kind >= sizeof(stmt_handlers) / sizeof(stmt_handlers[0]))
        return 0;
    stmt_handler_fn fn = stmt_handlers[stmt->kind];
    if (!fn)
        return 0;
    return fn(stmt, vars, funcs, labels, ir, func_ret_type,
              break_label, continue_label);
}

/* Issue warnings for unreachable statements after return or goto end */
static const char *find_end_label(func_t *func)
{
    for (size_t i = func->body_count; i-- > 0;) {
        stmt_t *s = func->body[i];
        if (s->kind == STMT_LABEL)
            return s->label.name;
        if (s->kind != STMT_RETURN)
            break;
    }
    return NULL;
}

void warn_unreachable_function(func_t *func, symtable_t *funcs)
{
    if (!semantic_warn_unreachable || !func)
        return;

    const char *end_label = find_end_label(func);
    int reachable = 1;
    for (size_t i = 0; i < func->body_count; i++) {
        stmt_t *s = func->body[i];
        if (!reachable) {
            if (!(s->kind == STMT_LABEL && end_label &&
                  strcmp(s->label.name, end_label) == 0)) {
                error_set(s->line, s->column, error_current_file,
                          error_current_function);
                error_print("warning: unreachable statement");
            }
        }
        if (s->kind == STMT_RETURN)
            reachable = 0;
        else if (s->kind == STMT_GOTO && end_label &&
                 strcmp(s->goto_stmt.name, end_label) == 0)
            reachable = 0;
        else if (s->kind == STMT_LABEL && end_label &&
                 strcmp(s->label.name, end_label) == 0)
            reachable = 1;
        else if (s->kind == STMT_EXPR && s->expr.expr &&
                 s->expr.expr->kind == EXPR_CALL) {
            symbol_t *fs = symtable_lookup(funcs, s->expr.expr->data.call.name);
            if (fs && fs->is_noreturn) {
                if (i + 1 < func->body_count &&
                    !(func->body[i+1]->kind == STMT_LABEL && end_label &&
                      strcmp(func->body[i+1]->label.name, end_label) == 0)) {
                    error_set(s->line, s->column, error_current_file,
                              error_current_function);
                    error_print("warning: non-returning call without terminator");
                }
                reachable = 0;
            }
        }
    }
}

