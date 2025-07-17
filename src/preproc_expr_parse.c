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

typedef long long (*binary_fn_t)(long long, long long);

typedef struct {
    const char *op;
    binary_fn_t fn;
} binary_op_t;

static long long parse_binary(expr_ctx_t *ctx,
                              long long (*next_fn)(expr_ctx_t *),
                              const binary_op_t *ops, size_t op_count)
{
    long long val = next_fn(ctx);
    ctx->s = skip_ws((char *)ctx->s);

    while (1) {
        size_t idx;
        size_t len = 0;
        binary_fn_t fn = NULL;

        for (idx = 0; idx < op_count; idx++) {
            len = strlen(ops[idx].op);
            if (strncmp(ctx->s, ops[idx].op, len) == 0) {
                fn = ops[idx].fn;
                break;
            }
        }

        if (!fn)
            break;

        ctx->s += len;
        long long rhs = next_fn(ctx);
        val = fn(val, rhs);
        ctx->s = skip_ws((char *)ctx->s);
    }

    return val;
}

/* Binary operator implementations */
static long long op_mul(long long a, long long b) { return a * b; }
static long long op_div(long long a, long long b) { return b ? a / b : 0; }
static long long op_mod(long long a, long long b) { return b ? a % b : 0; }
static long long op_add(long long a, long long b) { return a + b; }
static long long op_sub(long long a, long long b) { return a - b; }
static long long op_shl(long long a, long long b)
{
    if (b < 0)
        b = 0;
    else if (b >= (long long)(sizeof(long long) * CHAR_BIT))
        b = (long long)(sizeof(long long) * CHAR_BIT) - 1;
    return a << b;
}
static long long op_shr(long long a, long long b)
{
    if (b < 0)
        b = 0;
    else if (b >= (long long)(sizeof(long long) * CHAR_BIT))
        b = (long long)(sizeof(long long) * CHAR_BIT) - 1;
    return a >> b;
}
static long long op_lt(long long a, long long b)  { return a < b; }
static long long op_gt(long long a, long long b)  { return a > b; }
static long long op_le(long long a, long long b)  { return a <= b; }
static long long op_ge(long long a, long long b)  { return a >= b; }
static long long op_eq(long long a, long long b)  { return a == b; }
static long long op_ne(long long a, long long b)  { return a != b; }
static long long op_band(long long a, long long b){ return a & b; }
static long long op_xor(long long a, long long b) { return a ^ b; }
static long long op_bor(long long a, long long b) { return a | b; }
static long long op_land(long long a, long long b){ return a && b; }
static long long op_lor(long long a, long long b) { return a || b; }

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
    if (!tok) {
        vc_oom();
        ctx->error = 1;
        return 0;
    }
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
    static const binary_op_t ops[] = {
        {"*", op_mul}, {"/", op_div}, {"%", op_mod}
    };
    return parse_binary(ctx, parse_unary, ops, sizeof(ops) / sizeof(ops[0]));
}

static long long parse_add(expr_ctx_t *ctx)
{
    static const binary_op_t ops[] = {
        {"+", op_add}, {"-", op_sub}
    };
    return parse_binary(ctx, parse_mul, ops, sizeof(ops) / sizeof(ops[0]));
}

static long long parse_shift(expr_ctx_t *ctx)
{
    static const binary_op_t ops[] = {
        {"<<", op_shl}, {">>", op_shr}
    };
    return parse_binary(ctx, parse_add, ops, sizeof(ops) / sizeof(ops[0]));
}

static long long parse_rel(expr_ctx_t *ctx)
{
    static const binary_op_t ops[] = {
        {"<=", op_le}, {">=", op_ge}, {"<", op_lt}, {">", op_gt}
    };
    return parse_binary(ctx, parse_shift, ops, sizeof(ops) / sizeof(ops[0]));
}

static long long parse_eq(expr_ctx_t *ctx)
{
    static const binary_op_t ops[] = {
        {"==", op_eq}, {"!=", op_ne}
    };
    return parse_binary(ctx, parse_rel, ops, sizeof(ops) / sizeof(ops[0]));
}

static long long parse_band(expr_ctx_t *ctx)
{
    static const binary_op_t ops[] = {
        {"&", op_band}
    };
    return parse_binary(ctx, parse_eq, ops, sizeof(ops) / sizeof(ops[0]));
}

static long long parse_xor(expr_ctx_t *ctx)
{
    static const binary_op_t ops[] = {
        {"^", op_xor}
    };
    return parse_binary(ctx, parse_band, ops, sizeof(ops) / sizeof(ops[0]));
}

static long long parse_bor(expr_ctx_t *ctx)
{
    static const binary_op_t ops[] = {
        {"|", op_bor}
    };
    return parse_binary(ctx, parse_xor, ops, sizeof(ops) / sizeof(ops[0]));
}

static long long parse_and(expr_ctx_t *ctx)
{
    static const binary_op_t ops[] = {
        {"&&", op_land}
    };
    return parse_binary(ctx, parse_bor, ops, sizeof(ops) / sizeof(ops[0]));
}

static long long parse_or(expr_ctx_t *ctx)
{
    static const binary_op_t ops[] = {
        {"||", op_lor}
    };
    return parse_binary(ctx, parse_and, ops, sizeof(ops) / sizeof(ops[0]));
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

