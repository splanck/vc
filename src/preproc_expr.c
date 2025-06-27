/*
 * Tiny expression parser used by the preprocessor.
 *
 * This module implements a very small recursive descent parser for the
 * arithmetic used in `#if` and related directives.  The grammar supports the
 * `defined` operator, integer constants and the logical `!`, `&&` and `||`
 * operators.  Expressions are evaluated to an integer result used to control
 * conditional blocks during preprocessing.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <ctype.h>
#include <stdlib.h>
#include "preproc_expr.h"
#include <string.h>
#include "util.h"

/* Parser context */
typedef struct {
    const char *s;
    vector_t *macros;
} expr_ctx_t;

/* Advance the parser position past any spaces or tabs */
static void skip_ws(expr_ctx_t *ctx)
{
    while (*ctx->s == ' ' || *ctx->s == '\t')
        ctx->s++;
}

/* Parse an identifier token from the input and return a new string */
static char *parse_ident(expr_ctx_t *ctx)
{
    skip_ws(ctx);
    if (!isalpha((unsigned char)*ctx->s) && *ctx->s != '_')
        return NULL;
    const char *start = ctx->s;
    size_t len = 1;
    ctx->s++;
    while (isalnum((unsigned char)*ctx->s) || *ctx->s == '_') {
        ctx->s++;
        len++;
    }
    return vc_strndup(start, len);
}

static int parse_expr(expr_ctx_t *ctx);

/*
 * Parse a primary expression: literals, parentheses or defined().
 * Returns the integer value of the parsed expression.
 */
static int parse_primary(expr_ctx_t *ctx)
{
    skip_ws(ctx);
    if (strncmp(ctx->s, "defined", 7) == 0 &&
        (ctx->s[7] == '(' || ctx->s[7] == ' ' || ctx->s[7] == '\t')) {
        ctx->s += 7;
        skip_ws(ctx);
        if (*ctx->s == '(') {
            ctx->s++;
            char *id = parse_ident(ctx);
            skip_ws(ctx);
            if (*ctx->s == ')')
                ctx->s++;
            int val = id ? is_macro_defined(ctx->macros, id) : 0;
            free(id);
            return val;
        } else {
            char *id = parse_ident(ctx);
            int val = id ? is_macro_defined(ctx->macros, id) : 0;
            free(id);
            return val;
        }
    } else if (*ctx->s == '(') {
        ctx->s++;
        int val = parse_expr(ctx);
        skip_ws(ctx);
        if (*ctx->s == ')')
            ctx->s++;
        return val;
    } else if (isdigit((unsigned char)*ctx->s)) {
        int val = 0;
        while (isdigit((unsigned char)*ctx->s)) {
            val = val * 10 + (*ctx->s - '0');
            ctx->s++;
        }
        return val;
    } else {
        char *id = parse_ident(ctx);
        free(id);
        return 0;
    }
}

/* Handle unary negation in the expression grammar */
static int parse_not(expr_ctx_t *ctx)
{
    skip_ws(ctx);
    if (*ctx->s == '!') {
        ctx->s++;
        return !parse_not(ctx);
    }
    return parse_primary(ctx);
}

/* Parse left-associative '&&' operations */
static int parse_and(expr_ctx_t *ctx)
{
    int val = parse_not(ctx);
    skip_ws(ctx);
    while (strncmp(ctx->s, "&&", 2) == 0) {
        ctx->s += 2;
        int rhs = parse_not(ctx);
        val = val && rhs;
        skip_ws(ctx);
    }
    return val;
}

/* Parse left-associative '||' operations */
static int parse_or(expr_ctx_t *ctx)
{
    int val = parse_and(ctx);
    skip_ws(ctx);
    while (strncmp(ctx->s, "||", 2) == 0) {
        ctx->s += 2;
        int rhs = parse_and(ctx);
        val = val || rhs;
        skip_ws(ctx);
    }
    return val;
}

/* Entry point of the recursive descent parser */
static int parse_expr(expr_ctx_t *ctx)
{
    return parse_or(ctx) != 0;
}

/* Public wrapper used by the preprocessor to evaluate expressions */
int eval_expr(const char *s, vector_t *macros)
{
    expr_ctx_t ctx = { s, macros };
    return parse_expr(&ctx);
}

