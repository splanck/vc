/*
 * Top-level global variable parsing helpers.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include "parser.h"
#include "parser_types.h"
#include "ast_expr.h"
#include "ast_stmt.h"
#include "error.h"
#include "util.h"

static int parse_array_size(parser_t *p, type_kind_t *type, size_t *arr_size,
                            expr_t **size_expr)
{
    size_t start = p->pos;
    *arr_size = 0;
    *size_expr = NULL;

    if (match(p, TOK_LBRACKET)) {
        if (match(p, TOK_RBRACKET)) {
            *type = TYPE_ARRAY;
        } else {
            *size_expr = parser_parse_expr(p);
            if (!*size_expr || !match(p, TOK_RBRACKET)) {
                ast_free_expr(*size_expr);
                p->pos = start;
                return 0;
            }
            if ((*size_expr)->kind == EXPR_NUMBER) {
                if (!vc_strtoul_size((*size_expr)->number.value, arr_size)) {
                    error_set((*size_expr)->line, (*size_expr)->column,
                              error_current_file, error_current_function);
                    error_print("Integer constant out of range");
                    ast_free_expr(*size_expr);
                    *size_expr = NULL;
                    p->pos = start;
                    return 0;
                }
                ast_free_expr(*size_expr);
                *size_expr = NULL;
            }
            *type = TYPE_ARRAY;
        }
    }
    return 1;
}

/* Parse an initializer list enclosed in braces followed by a semicolon. */
static int parse_braced_initializer(parser_t *p, init_entry_t **init_list,
                                    size_t *init_count)
{
    *init_list = parser_parse_init_list(p, init_count);
    if (!*init_list || !match(p, TOK_SEMI)) {
        if (*init_list) {
            for (size_t i = 0; i < *init_count; i++) {
                ast_free_expr((*init_list)[i].index);
                ast_free_expr((*init_list)[i].value);
                free((*init_list)[i].field);
            }
            free(*init_list);
        }
        return 0;
    }
    return 1;
}

/* Parse an initializer expression followed by a semicolon. */
static int parse_expr_initializer(parser_t *p, expr_t **init)
{
    *init = parser_parse_expr(p);
    if (!*init || !match(p, TOK_SEMI)) {
        ast_free_expr(*init);
        return 0;
    }
    return 1;
}

/* Parse an initializer expression or initializer list followed by a
 * terminating semicolon.  On failure the parser position is restored to
 * start and any allocated expressions are freed. */
static int parse_initializer(parser_t *p, type_kind_t type, expr_t **init,
                             init_entry_t **init_list, size_t *init_count)
{
    size_t start = p->pos;
    *init = NULL;
    *init_list = NULL;
    *init_count = 0;

    if (match(p, TOK_ASSIGN)) {
        int ok;
        if ((type == TYPE_ARRAY || type == TYPE_STRUCT) &&
            peek(p) && peek(p)->type == TOK_LBRACE)
            ok = parse_braced_initializer(p, init_list, init_count);
        else
            ok = parse_expr_initializer(p, init);
        if (!ok) {
            p->pos = start;
            return 0;
        }
    } else {
        if (!match(p, TOK_SEMI)) {
            p->pos = start;
            return 0;
        }
    }
    return 1;
}

/* Parse a global variable after its name.  This routine delegates parsing of
 * the optional array size and initializer to helpers so it mostly manages
 * control flow and error recovery.  The parser must start immediately after
 * the identifier.  On success out_global receives the declaration and the
 * parser is positioned after the terminating semicolon. */
int parse_global_var_init(parser_t *p, const char *name, type_kind_t type,
                                 size_t elem_size, int is_static, int is_register,
                                 int is_extern, int is_const, int is_volatile,
                                 int is_restrict, const char *tag,
                                 size_t line, size_t column,
                                 stmt_t **out_global)
{
    size_t start = p->pos;
    size_t arr_size;
    expr_t *size_expr;

    if (!parse_array_size(p, &type, &arr_size, &size_expr))
        goto fail;

    if (type == TYPE_VOID) {
        ast_free_expr(size_expr);
        goto fail;
    }

    expr_t *init;
    init_entry_t *init_list;
    size_t init_count;

    if (!parse_initializer(p, type, &init, &init_list, &init_count)) {
        ast_free_expr(size_expr);
        goto fail;
    }

    if (out_global)
        *out_global = ast_make_var_decl(name, type, arr_size, size_expr,
                                        NULL, elem_size, is_static, is_register,
                                        is_extern, is_const, is_volatile,
                                        is_restrict, init, init_list,
                                        init_count, tag, NULL, 0,
                                        line, column);
    return 1;

fail:
    p->pos = start;
    return 0;
}
