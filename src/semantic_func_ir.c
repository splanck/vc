/*
 * Function body IR generation helpers.
 * Contains the routines responsible for emitting IR
 * for validated function definitions.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include <string.h>
#include "semantic_global.h"
#include "semantic_stmt.h"
#include "semantic_control.h"
#include "symtable.h"
#include "label.h"
#include "error.h"

int emit_func_ir(func_t *func, symtable_t *funcs, symtable_t *globals,
                 ir_builder_t *ir)
{
    if (!func)
        return 0;

    symtable_t locals;
    symtable_init(&locals);
    locals.globals = globals ? globals->globals : NULL;
    semantic_stack_offset = 0;
    semantic_stack_zero = 1;

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
        ok = check_stmt(func->body[i], &locals, funcs, &labels, ir,
                        func->return_type, NULL, NULL);

    if (func_begin && !semantic_stack_zero)
        func_begin->imm = semantic_stack_offset;
    ir_build_func_end(ir);

    label_table_free(&labels);
    locals.globals = NULL;
    symtable_free(&locals);
    return ok;
}

