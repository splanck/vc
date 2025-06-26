/*
 * Expression parsing for conditional directives.
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

static void skip_ws(expr_ctx_t *ctx)
{
    while (*ctx->s == ' ' || *ctx->s == '\t')
        ctx->s++;
}

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

static int parse_not(expr_ctx_t *ctx)
{
    skip_ws(ctx);
    if (*ctx->s == '!') {
        ctx->s++;
        return !parse_not(ctx);
    }
    return parse_primary(ctx);
}

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

static int parse_expr(expr_ctx_t *ctx)
{
    return parse_or(ctx) != 0;
}

int eval_expr(const char *s, vector_t *macros)
{
    expr_ctx_t ctx = { s, macros };
    return parse_expr(&ctx);
}

