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
#include "preproc_path.h"
#include "preproc_file_io.h"

/* Parser context */
typedef struct {
    const char *s;
    vector_t *macros;
    const char *dir;
    const vector_t *incdirs;
    vector_t *stack;
    int error;
} expr_ctx_t;

/* Advance the parser position past any whitespace */
static void skip_ws(expr_ctx_t *ctx)
{
    while (isspace((unsigned char)*ctx->s))
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

/* Parse a header name argument used by __has_include */
static char *parse_header_name(expr_ctx_t *ctx, char *endc)
{
    skip_ws(ctx);
    if (*ctx->s == '"') {
        *endc = '"';
        ctx->s++;
        const char *start = ctx->s;
        while (*ctx->s && *ctx->s != '"')
            ctx->s++;
        if (*ctx->s != '"') {
            ctx->error = 1;
            return NULL;
        }
        char *name = vc_strndup(start, (size_t)(ctx->s - start));
        ctx->s++;
        return name;
    } else if (*ctx->s == '<') {
        *endc = '>';
        ctx->s++;
        const char *start = ctx->s;
        while (*ctx->s && *ctx->s != '>')
            ctx->s++;
        if (*ctx->s != '>') {
            ctx->error = 1;
            return NULL;
        }
        char *name = vc_strndup(start, (size_t)(ctx->s - start));
        ctx->s++;
        return name;
    }
    ctx->error = 1;
    return NULL;
}

static long long parse_has_include(expr_ctx_t *ctx, int is_next)
{
    ctx->s += is_next ? 18 : 13; /* skip directive name */
    skip_ws(ctx);
    if (*ctx->s != '(') {
        ctx->error = 1;
        return 0;
    }
    ctx->s++;

    /* extract the token between parentheses */
    const char *start = ctx->s;
    while (*ctx->s && *ctx->s != ')')
        ctx->s++;
    size_t len = (size_t)(ctx->s - start);
    char *tok = vc_strndup(start, len);
    skip_ws(ctx);
    if (*ctx->s == ')')
        ctx->s++;
    else
        ctx->error = 1;

    if (ctx->error) {
        free(tok);
        return 0;
    }

    /* expand macros inside the argument */
    strbuf_t exp;
    strbuf_init(&exp);
    preproc_context_t dummy = {0};
    if (!expand_line(tok, ctx->macros, &exp, 0, 0, &dummy)) {
        ctx->error = 1;
        free(tok);
        strbuf_free(&exp);
        return 0;
    }
    free(tok);

    /* parse the expanded token as a header name */
    expr_ctx_t tmp = { exp.data ? exp.data : "", ctx->macros, ctx->dir,
                       ctx->incdirs, ctx->stack, 0 };
    char endc = '"';
    char *fname = parse_header_name(&tmp, &endc);
    if (tmp.error) {
        ctx->error = 1;
        strbuf_free(&exp);
        free(fname);
        return 0;
    }
    strbuf_free(&exp);

    int found = 0;
    if (fname && ctx->incdirs) {
        size_t cur = (size_t)-1;
        if (is_next && ctx->stack && ctx->stack->count) {
            const include_entry_t *e =
                &((include_entry_t *)ctx->stack->data)[ctx->stack->count - 1];
            cur = e->dir_index;
        }
        size_t start_idx = is_next ? ((cur == (size_t)-1) ? 0 : cur + 1) : 0;
        size_t idx;
        char *inc = find_include_path(fname, endc,
                                      is_next ? NULL : ctx->dir,
                                      ctx->incdirs, start_idx, &idx);
        if (inc) {
            found = 1;
            free(inc);
        }
    }
    free(fname);
    return found;
}
/* Parse escape sequences in character literals */
static int parse_char_escape(const char **s)
{
    char c = **s;
    if (c == 'n') { (*s)++; return '\n'; }
    if (c == 't') { (*s)++; return '\t'; }
    if (c == 'r') { (*s)++; return '\r'; }
    if (c == 'b') { (*s)++; return '\b'; }
    if (c == 'f') { (*s)++; return '\f'; }
    if (c == 'v') { (*s)++; return '\v'; }
    if (c == '\\') { (*s)++; return '\\'; }
    if (c == '\'') { (*s)++; return '\''; }
    if (c == '"') { (*s)++; return '"'; }
    if (c == 'x') {
        (*s)++; int val = 0;
        while (isxdigit((unsigned char)**s)) {
            char d = **s;
            int hex = (d >= '0' && d <= '9') ? d - '0' :
                       (d >= 'a' && d <= 'f') ? d - 'a' + 10 :
                       (d >= 'A' && d <= 'F') ? d - 'A' + 10 : 0;
            val = val * 16 + hex;
            (*s)++;
        }
        return val;
    }
    if (c >= '0' && c <= '7') {
        int val = 0, digits = 0;
        while (digits < 3 && **s >= '0' && **s <= '7') {
            val = val * 8 + (**s - '0');
            (*s)++; digits++;
        }
        return val;
    }
    (*s)++; return (unsigned char)c;
}

/*
 * Parse a primary expression: literals, parentheses or defined().
 * Returns the integer value of the parsed expression.
 */
static long long parse_primary(expr_ctx_t *ctx)
{
    skip_ws(ctx);
    if (strncmp(ctx->s, "__has_include_next", 18) == 0 && ctx->s[18] == '(')
        return parse_has_include(ctx, 1);
    if (strncmp(ctx->s, "__has_include", 13) == 0 && ctx->s[13] == '(')
        return parse_has_include(ctx, 0);
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
    } else if (*ctx->s == '\'') {
        ctx->s++;
        int value = 0;
        if (*ctx->s == '\\') {
            ctx->s++;
            value = parse_char_escape(&ctx->s);
        } else if (*ctx->s) {
            value = (unsigned char)*ctx->s++;
        } else {
            ctx->error = 1;
        }
        if (*ctx->s == '\'')
            ctx->s++;
        else
            ctx->error = 1;
        return value;
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
static long long eval_internal(const char *s, vector_t *macros,
                               const char *dir, const vector_t *incdirs,
                               vector_t *stack)
{
    expr_ctx_t ctx = { s, macros, dir, incdirs, stack, 0 };
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

long long eval_expr_full(const char *s, vector_t *macros,
                         const char *dir, const vector_t *incdirs,
                         vector_t *stack)
{
    return eval_internal(s, macros, dir, incdirs, stack);
}

long long eval_expr(const char *s, vector_t *macros)
{
    return eval_internal(s, macros, NULL, NULL, NULL);
}

