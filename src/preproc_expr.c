/*
 * Tiny expression parser used by the preprocessor.
 *
 * This module implements a very small recursive descent parser for the
 * arithmetic used in `#if` and related directives.  The grammar supports the
 * `defined` operator, integer constants and a subset of the C operators used in
 * glibc headers.  Supported operators include unary `!` and `~`, the
 * arithmetic `+`, `-`, `*`, `/`, `%`, bit shifts `<<` and `>>`, comparisons,
 * bitwise `&`, `|`, `^` as well as the logical `&&` and `||` operators.
 * Expressions are evaluated to an integer result used to control
 * conditional blocks during preprocessing.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <ctype.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include "preproc_expr.h"
#include <string.h>
#include "util.h"
#include <limits.h>

/* Parser context */
typedef struct {
    const char *s;
    vector_t *macros;
    int error;
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

static long long parse_expr(expr_ctx_t *ctx);
static long long parse_conditional(expr_ctx_t *ctx);

/*
 * Parse a primary expression: literals, parentheses or defined().
 * Returns the integer value of the parsed expression.
 */
static long long parse_primary(expr_ctx_t *ctx)
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
            else
                ctx->error = 1;
            long long val = id ? is_macro_defined(ctx->macros, id) : 0;
            free(id);
            return val;
        } else {
            char *id = parse_ident(ctx);
            long long val = id ? is_macro_defined(ctx->macros, id) : 0;
            free(id);
            return val;
        }
    } else if (*ctx->s == '(') {
        ctx->s++;
        long long val = parse_expr(ctx);
        skip_ws(ctx);
        if (*ctx->s == ')')
            ctx->s++;
        else
            ctx->error = 1;
        return val;
    } else if (isdigit((unsigned char)*ctx->s)) {
        errno = 0;
        char *end;
        long long val = strtoll(ctx->s, &end, 0);
        if (errno == ERANGE) {
            val = val < 0 ? LLONG_MIN : LLONG_MAX;
        }
        ctx->s = end;
        while (*ctx->s == 'u' || *ctx->s == 'U' ||
               *ctx->s == 'l' || *ctx->s == 'L')
            ctx->s++;
        return val;
    } else {
        char *id = parse_ident(ctx);
        free(id);
        return 0;
    }
}

/*
 * Unary operators '!', '~', '+', and '-'.
 *
 * Supporting '+' and '-' here ensures that literals with a leading
 * sign are parsed correctly.  Without this a leading '-' would be
 * treated as a binary operator with an implicit left operand of zero.
 */
static long long parse_unary(expr_ctx_t *ctx)
{
    skip_ws(ctx);
    if (*ctx->s == '!') {
        ctx->s++;
        return !parse_unary(ctx);
    } else if (*ctx->s == '~') {
        ctx->s++;
        return ~parse_unary(ctx);
    } else if (*ctx->s == '+') {
        ctx->s++;
        return parse_unary(ctx);
    } else if (*ctx->s == '-') {
        ctx->s++;
        return -parse_unary(ctx);
    }
    return parse_primary(ctx);
}

/* Multiplicative */
static long long parse_mul(expr_ctx_t *ctx)
{
    long long val = parse_unary(ctx);
    skip_ws(ctx);
    while (*ctx->s == '*' || *ctx->s == '/' || *ctx->s == '%') {
        char op = *ctx->s++;
        long long rhs = parse_unary(ctx);
        switch (op) {
        case '*': val = val * rhs; break;
        case '/': val = rhs ? val / rhs : 0; break;
        case '%': val = rhs ? val % rhs : 0; break;
        }
        skip_ws(ctx);
    }
    return val;
}

/* Additive */
static long long parse_add(expr_ctx_t *ctx)
{
    long long val = parse_mul(ctx);
    skip_ws(ctx);
    while (*ctx->s == '+' || *ctx->s == '-') {
        char op = *ctx->s++;
        long long rhs = parse_mul(ctx);
        if (op == '+')
            val += rhs;
        else
            val -= rhs;
        skip_ws(ctx);
    }
    return val;
}

/* Shifts */
static long long parse_shift(expr_ctx_t *ctx)
{
    long long val = parse_add(ctx);
    skip_ws(ctx);
    while (strncmp(ctx->s, "<<", 2) == 0 || strncmp(ctx->s, ">>", 2) == 0) {
        int left = (ctx->s[0] == '<');
        ctx->s += 2;
        long long rhs = parse_add(ctx);
        if (left)
            val <<= rhs;
        else
            val >>= rhs;
        skip_ws(ctx);
    }
    return val;
}

/* Relational */
static long long parse_rel(expr_ctx_t *ctx)
{
    long long val = parse_shift(ctx);
    skip_ws(ctx);
    while (1) {
        if (strncmp(ctx->s, "<=", 2) == 0) {
            ctx->s += 2;
            long long rhs = parse_shift(ctx);
            val = val <= rhs;
        } else if (strncmp(ctx->s, ">=", 2) == 0) {
            ctx->s += 2;
            long long rhs = parse_shift(ctx);
            val = val >= rhs;
        } else if (*ctx->s == '<' && ctx->s[1] != '<') {
            ctx->s++;
            long long rhs = parse_shift(ctx);
            val = val < rhs;
        } else if (*ctx->s == '>' && ctx->s[1] != '>') {
            ctx->s++;
            long long rhs = parse_shift(ctx);
            val = val > rhs;
        } else {
            break;
        }
        skip_ws(ctx);
    }
    return val;
}

/* Equality */
static long long parse_eq(expr_ctx_t *ctx)
{
    long long val = parse_rel(ctx);
    skip_ws(ctx);
    while (1) {
        if (strncmp(ctx->s, "==", 2) == 0) {
            ctx->s += 2;
            long long rhs = parse_rel(ctx);
            val = val == rhs;
        } else if (strncmp(ctx->s, "!=", 2) == 0) {
            ctx->s += 2;
            long long rhs = parse_rel(ctx);
            val = val != rhs;
        } else {
            break;
        }
        skip_ws(ctx);
    }
    return val;
}

/* Bitwise AND */
static long long parse_band(expr_ctx_t *ctx)
{
    long long val = parse_eq(ctx);
    skip_ws(ctx);
    while (*ctx->s == '&' && ctx->s[1] != '&') {
        ctx->s++;
        long long rhs = parse_eq(ctx);
        val &= rhs;
        skip_ws(ctx);
    }
    return val;
}

/* Bitwise XOR */
static long long parse_xor(expr_ctx_t *ctx)
{
    long long val = parse_band(ctx);
    skip_ws(ctx);
    while (*ctx->s == '^') {
        ctx->s++;
        long long rhs = parse_band(ctx);
        val ^= rhs;
        skip_ws(ctx);
    }
    return val;
}

/* Bitwise OR */
static long long parse_bor(expr_ctx_t *ctx)
{
    long long val = parse_xor(ctx);
    skip_ws(ctx);
    while (*ctx->s == '|' && ctx->s[1] != '|') {
        ctx->s++;
        long long rhs = parse_xor(ctx);
        val |= rhs;
        skip_ws(ctx);
    }
    return val;
}

/* Logical AND */
static long long parse_and(expr_ctx_t *ctx)
{
    long long val = parse_bor(ctx);
    skip_ws(ctx);
    while (strncmp(ctx->s, "&&", 2) == 0) {
        ctx->s += 2;
        long long rhs = parse_bor(ctx);
        val = val && rhs;
        skip_ws(ctx);
    }
    return val;
}

/* Logical OR */
static long long parse_or(expr_ctx_t *ctx)
{
    long long val = parse_and(ctx);
    skip_ws(ctx);
    while (strncmp(ctx->s, "||", 2) == 0) {
        ctx->s += 2;
        long long rhs = parse_and(ctx);
        val = val || rhs;
        skip_ws(ctx);
    }
    return val;
}

/* Conditional operator */
static long long parse_conditional(expr_ctx_t *ctx)
{
    long long val = parse_or(ctx);
    skip_ws(ctx);
    if (*ctx->s == '?') {
        ctx->s++;
        long long then_val = parse_conditional(ctx);
        skip_ws(ctx);
        if (*ctx->s == ':')
            ctx->s++;
        long long else_val = parse_conditional(ctx);
        val = val ? then_val : else_val;
    }
    return val;
}

/* Entry point of the recursive descent parser */
static long long parse_expr(expr_ctx_t *ctx)
{
    return parse_conditional(ctx);
}

/* Public wrapper used by the preprocessor to evaluate expressions */
long long eval_expr(const char *s, vector_t *macros)
{
    expr_ctx_t ctx = { s, macros, 0 };
    long long val = parse_expr(&ctx);
    skip_ws(&ctx);
    if (*ctx.s != '\0')
        ctx.error = 1;
    if (ctx.error) {
        fprintf(stderr, "Invalid preprocessor expression\n");
        return 0;
    }
    return val;
}

