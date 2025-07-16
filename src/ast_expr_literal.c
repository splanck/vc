/*
 * Literal expression constructors.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include <string.h>
#include "ast_expr.h"
#include "util.h"

static expr_t *new_expr(expr_kind_t kind, size_t line, size_t column)
{
    expr_t *expr = malloc(sizeof(*expr));
    if (!expr)
        return NULL;
    expr->kind = kind;
    expr->line = line;
    expr->column = column;
    return expr;
}

static char *strip_suffix(const char *tok, int *is_unsigned, int *long_count)
{
    *is_unsigned = 0;
    *long_count = 0;
    size_t len = strlen(tok);
    size_t i = len;
    while (i > 0) {
        char c = tok[i-1];
        if (c == 'u' || c == 'U') {
            *is_unsigned = 1;
            i--;
        } else if (c == 'l' || c == 'L') {
            i--;
            (*long_count)++;
            if (i > 0 && (tok[i-1] == 'l' || tok[i-1] == 'L')) {
                (*long_count)++;
                i--;
            }
        } else {
            break;
        }
    }
    return vc_strndup(tok, i);
}

static expr_t *make_string(const char *value, size_t line, size_t column, int is_wide)
{
    expr_t *expr = new_expr(EXPR_STRING, line, column);
    if (!expr)
        return NULL;
    expr->data.string.value = vc_strdup(value ? value : "");
    if (!expr->data.string.value) {
        free(expr);
        return NULL;
    }
    expr->data.string.is_wide = is_wide;
    return expr;
}

static expr_t *make_char(char value, size_t line, size_t column, int is_wide)
{
    expr_t *expr = new_expr(EXPR_CHAR, line, column);
    if (!expr)
        return NULL;
    expr->data.ch.value = value;
    expr->data.ch.is_wide = is_wide;
    return expr;
}

expr_t *ast_make_number(const char *value, size_t line, size_t column)
{
    int is_unsigned, long_count;
    char *val = strip_suffix(value ? value : "", &is_unsigned, &long_count);
    if (!val)
        return NULL;
    expr_t *expr = new_expr(EXPR_NUMBER, line, column);
    if (!expr) {
        free(val);
        return NULL;
    }
    expr->data.number.value = val;
    expr->data.number.is_unsigned = is_unsigned;
    expr->data.number.long_count = long_count;
    return expr;
}

expr_t *ast_make_ident(const char *name, size_t line, size_t column)
{
    expr_t *expr = new_expr(EXPR_IDENT, line, column);
    if (!expr)
        return NULL;
    expr->data.ident.name = vc_strdup(name ? name : "");
    if (!expr->data.ident.name) {
        free(expr);
        return NULL;
    }
    return expr;
}

expr_t *ast_make_string(const char *value, size_t line, size_t column)
{
    return make_string(value, line, column, 0);
}

expr_t *ast_make_wstring(const char *value, size_t line, size_t column)
{
    return make_string(value, line, column, 1);
}

expr_t *ast_make_char(char value, size_t line, size_t column)
{
    return make_char(value, line, column, 0);
}

expr_t *ast_make_wchar(char value, size_t line, size_t column)
{
    return make_char(value, line, column, 1);
}

expr_t *ast_make_complex_literal(double real, double imag,
                                 size_t line, size_t column)
{
    expr_t *expr = new_expr(EXPR_COMPLEX_LITERAL, line, column);
    if (!expr)
        return NULL;
    expr->data.complex_lit.real = real;
    expr->data.complex_lit.imag = imag;
    return expr;
}

