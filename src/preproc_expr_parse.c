#include <ctype.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include "util.h"
#include "strbuf.h"
#include "preproc_expr_parse.h"
#include "preproc_expr_lex.h"
#include "preproc_path.h"
#include "preproc_file_io.h"
#include "preproc_utils.h"

static long long parse_has_include(expr_ctx_t *ctx, int is_next)
{
    ctx->s += is_next ? 18 : 13; /* skip directive name */
    ctx->s = skip_ws((char *)ctx->s);
    if (*ctx->s != '(') {
        ctx->error = 1;
        return 0;
    }
    ctx->s++;

    const char *start = ctx->s;
    while (*ctx->s && *ctx->s != ')')
        ctx->s++;
    size_t len = (size_t)(ctx->s - start);
    char *tok = vc_strndup(start, len);
    ctx->s = skip_ws((char *)ctx->s);
    if (*ctx->s == ')')
        ctx->s++;
    else
        ctx->error = 1;

    if (ctx->error) {
        free(tok);
        return 0;
    }

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

    expr_ctx_t tmp = { exp.data ? exp.data : "", ctx->macros, ctx->dir,
                       ctx->incdirs, ctx->stack, 0 };
    char endc = '"';
    char *fname = expr_parse_header_name(&tmp, &endc);
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

static long long parse_primary(expr_ctx_t *ctx)
{
    ctx->s = skip_ws((char *)ctx->s);
    if (strncmp(ctx->s, "__has_include_next", 18) == 0 && ctx->s[18] == '(')
        return parse_has_include(ctx, 1);
    if (strncmp(ctx->s, "__has_include", 13) == 0 && ctx->s[13] == '(')
        return parse_has_include(ctx, 0);
    if (strncmp(ctx->s, "defined", 7) == 0 &&
        (ctx->s[7] == '(' || ctx->s[7] == ' ' || ctx->s[7] == '\t')) {
        ctx->s += 7;
        ctx->s = skip_ws((char *)ctx->s);
        if (*ctx->s == '(') {
            ctx->s++;
            char *id = expr_parse_ident(ctx);
            ctx->s = skip_ws((char *)ctx->s);
            if (*ctx->s == ')')
                ctx->s++;
            else
                ctx->error = 1;
            long long val = id ? is_macro_defined(ctx->macros, id) : 0;
            free(id);
            return val;
        } else {
            char *id = expr_parse_ident(ctx);
            long long val = id ? is_macro_defined(ctx->macros, id) : 0;
            free(id);
            return val;
        }
    } else if (*ctx->s == '(') {
        ctx->s++;
        long long val = parse_expr(ctx);
        ctx->s = skip_ws((char *)ctx->s);
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
            value = expr_parse_char_escape(&ctx->s);
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
        char *id = expr_parse_ident(ctx);
        free(id);
        return 0;
    }
}

static long long parse_unary(expr_ctx_t *ctx)
{
    ctx->s = skip_ws((char *)ctx->s);
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

static long long parse_mul(expr_ctx_t *ctx)
{
    long long val = parse_unary(ctx);
    ctx->s = skip_ws((char *)ctx->s);
    while (*ctx->s == '*' || *ctx->s == '/' || *ctx->s == '%') {
        char op = *ctx->s++;
        long long rhs = parse_unary(ctx);
        switch (op) {
        case '*': val = val * rhs; break;
        case '/': val = rhs ? val / rhs : 0; break;
        case '%': val = rhs ? val % rhs : 0; break;
        }
        ctx->s = skip_ws((char *)ctx->s);
    }
    return val;
}

static long long parse_add(expr_ctx_t *ctx)
{
    long long val = parse_mul(ctx);
    ctx->s = skip_ws((char *)ctx->s);
    while (*ctx->s == '+' || *ctx->s == '-') {
        char op = *ctx->s++;
        long long rhs = parse_mul(ctx);
        if (op == '+')
            val += rhs;
        else
            val -= rhs;
        ctx->s = skip_ws((char *)ctx->s);
    }
    return val;
}

static long long parse_shift(expr_ctx_t *ctx)
{
    long long val = parse_add(ctx);
    ctx->s = skip_ws((char *)ctx->s);
    while (strncmp(ctx->s, "<<", 2) == 0 || strncmp(ctx->s, ">>", 2) == 0) {
        int left = (ctx->s[0] == '<');
        ctx->s += 2;
        long long rhs = parse_add(ctx);
        /*
         * Clamp the shift count so undefined behaviour does not occur when
         * shifting by a value outside the width of long long. Negative counts
         * are treated as zero while counts greater than or equal to the type
         * width (64 on most hosts) are reduced to width - 1.  This mirrors the
         * behaviour of many hosts and keeps evaluation well-defined.
         */
        if (rhs < 0)
            rhs = 0;
        else if (rhs >= (long long)(sizeof(long long) * CHAR_BIT))
            rhs = (long long)(sizeof(long long) * CHAR_BIT) - 1;
        if (left)
            val <<= rhs;
        else
            val >>= rhs;
        ctx->s = skip_ws((char *)ctx->s);
    }
    return val;
}

static long long parse_rel(expr_ctx_t *ctx)
{
    long long val = parse_shift(ctx);
    ctx->s = skip_ws((char *)ctx->s);
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
        ctx->s = skip_ws((char *)ctx->s);
    }
    return val;
}

static long long parse_eq(expr_ctx_t *ctx)
{
    long long val = parse_rel(ctx);
    ctx->s = skip_ws((char *)ctx->s);
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
        ctx->s = skip_ws((char *)ctx->s);
    }
    return val;
}

static long long parse_band(expr_ctx_t *ctx)
{
    long long val = parse_eq(ctx);
    ctx->s = skip_ws((char *)ctx->s);
    while (*ctx->s == '&' && ctx->s[1] != '&') {
        ctx->s++;
        long long rhs = parse_eq(ctx);
        val &= rhs;
        ctx->s = skip_ws((char *)ctx->s);
    }
    return val;
}

static long long parse_xor(expr_ctx_t *ctx)
{
    long long val = parse_band(ctx);
    ctx->s = skip_ws((char *)ctx->s);
    while (*ctx->s == '^') {
        ctx->s++;
        long long rhs = parse_band(ctx);
        val ^= rhs;
        ctx->s = skip_ws((char *)ctx->s);
    }
    return val;
}

static long long parse_bor(expr_ctx_t *ctx)
{
    long long val = parse_xor(ctx);
    ctx->s = skip_ws((char *)ctx->s);
    while (*ctx->s == '|' && ctx->s[1] != '|') {
        ctx->s++;
        long long rhs = parse_xor(ctx);
        val |= rhs;
        ctx->s = skip_ws((char *)ctx->s);
    }
    return val;
}

static long long parse_and(expr_ctx_t *ctx)
{
    long long val = parse_bor(ctx);
    ctx->s = skip_ws((char *)ctx->s);
    while (strncmp(ctx->s, "&&", 2) == 0) {
        ctx->s += 2;
        long long rhs = parse_bor(ctx);
        val = val && rhs;
        ctx->s = skip_ws((char *)ctx->s);
    }
    return val;
}

static long long parse_or(expr_ctx_t *ctx)
{
    long long val = parse_and(ctx);
    ctx->s = skip_ws((char *)ctx->s);
    while (strncmp(ctx->s, "||", 2) == 0) {
        ctx->s += 2;
        long long rhs = parse_and(ctx);
        val = val || rhs;
        ctx->s = skip_ws((char *)ctx->s);
    }
    return val;
}

long long parse_conditional(expr_ctx_t *ctx)
{
    long long val = parse_or(ctx);
    ctx->s = skip_ws((char *)ctx->s);
    if (*ctx->s == '?') {
        ctx->s++;
        long long then_val = parse_conditional(ctx);
        ctx->s = skip_ws((char *)ctx->s);
        if (*ctx->s == ':')
            ctx->s++;
        long long else_val = parse_conditional(ctx);
        val = val ? then_val : else_val;
    }
    return val;
}

long long parse_expr(expr_ctx_t *ctx)
{
    return parse_conditional(ctx);
}

