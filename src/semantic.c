#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "semantic.h"
#include "symtable.h"
#include "util.h"
#include "label.h"

static size_t error_line = 0;
static size_t error_column = 0;

void semantic_set_error(size_t line, size_t column)
{
    error_line = line;
    error_column = column;
}

void semantic_print_error(const char *msg)
{
    fprintf(stderr, "%s at line %zu, column %zu\n", msg, error_line, error_column);
}

int check_func(func_t *func, symtable_t *funcs, symtable_t *globals,
               ir_builder_t *ir)
{
    if (!func)
        return 0;

    symtable_t locals;
    symtable_init(&locals);
    locals.globals = globals ? globals->globals : NULL;

    for (size_t i = 0; i < func->param_count; i++)
        symtable_add_param(&locals, func->param_names[i],
                           func->param_types[i], (int)i);

    ir_build_func_begin(ir, func->name);

    int ok = 1;
    for (size_t i = 0; i < func->body_count && ok; i++)
        ok = check_stmt(func->body[i], &locals, funcs, ir, func->return_type,
                        NULL, NULL);

    ir_build_func_end(ir);

    locals.globals = NULL;
    symtable_free(&locals);
    return ok;
}

int check_global(stmt_t *decl, symtable_t *globals, ir_builder_t *ir)
{
    if (!decl || decl->kind != STMT_VAR_DECL)
        return 0;
    if (!symtable_add_global(globals, decl->var_decl.name,
                             decl->var_decl.type,
                             decl->var_decl.array_size)) {
        semantic_set_error(decl->line, decl->column);
        return 0;
    }
    if (decl->var_decl.type == TYPE_ARRAY) {
        size_t count = decl->var_decl.array_size;
        int *vals = calloc(count, sizeof(int));
        if (!vals)
            return 0;
        size_t init_count = decl->var_decl.init_count;
        if (init_count > count) {
            free(vals);
            semantic_set_error(decl->line, decl->column);
            return 0;
        }
        for (size_t i = 0; i < init_count; i++) {
            if (!eval_const_expr(decl->var_decl.init_list[i], &vals[i])) {
                free(vals);
                semantic_set_error(decl->var_decl.init_list[i]->line,
                                  decl->var_decl.init_list[i]->column);
                return 0;
            }
        }
        ir_build_glob_array(ir, decl->var_decl.name, vals, count);
        free(vals);
    } else {
        int value = 0;
        if (decl->var_decl.init) {
            if (!eval_const_expr(decl->var_decl.init, &value)) {
                semantic_set_error(decl->var_decl.init->line, decl->var_decl.init->column);
                return 0;
            }
        }
        ir_build_glob_var(ir, decl->var_decl.name, value);
    }
    return 1;
}
