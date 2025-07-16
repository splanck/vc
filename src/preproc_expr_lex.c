#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"
#include "preproc_expr_lex.h"
#include "preproc_utils.h"

/* Return the numeric value of a hexadecimal digit or -1 if invalid */
static int hex_digit_value(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

/* Return the numeric value of an octal digit or -1 if invalid */
static int oct_digit_value(char c)
{
    if (c >= '0' && c <= '7')
        return c - '0';
    return -1;
}

char *expr_parse_ident(expr_ctx_t *ctx)
{
    ctx->s = skip_ws((char *)ctx->s);
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

char *expr_parse_header_name(expr_ctx_t *ctx, char *endc)
{
    ctx->s = skip_ws((char *)ctx->s);
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

int expr_parse_char_escape(const char **s)
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
        (*s)++; int val = 0; int digits = 0; int d;
        while (digits < 2 && (d = hex_digit_value(**s)) != -1) {
            val = val * 16 + d;
            (*s)++; digits++;
        }
        return val;
    }
    if (c >= '0' && c <= '7') {
        int val = 0, digits = 0, d, next;
        while (digits < 3 && (d = oct_digit_value(**s)) != -1) {
            next = val * 8 + d;
            if (next > 255) {
                val = 255;
                while (digits < 3 && oct_digit_value(**s) != -1) {
                    (*s)++; digits++;
                }
                return val;
            }
            val = next;
            (*s)++; digits++;
        }
        return val;
    }
    (*s)++; return (unsigned char)c;
}

