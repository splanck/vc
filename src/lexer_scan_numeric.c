#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "token.h"
#include "vector.h"
#include "util.h"
#include "error.h"
#include "lexer_internal.h"

/* Numeric and character/string scanning helpers */

static void read_number(const char *src, size_t *i, size_t *col,
                        vector_t *tokens, size_t line)
{
    size_t start = *i;
    int is_float = 0;

    if (src[*i] == '0' && (src[*i + 1] == 'x' || src[*i + 1] == 'X')) {
        (*i) += 2;
        while (isxdigit((unsigned char)src[*i]))
            (*i)++;
    } else if (src[*i] == '0') {
        (*i)++;
        while (src[*i] >= '0' && src[*i] <= '7')
            (*i)++;
    } else {
        while (isdigit((unsigned char)src[*i]))
            (*i)++;
    }

    if (src[*i] == '.') {
        is_float = 1;
        (*i)++;
        while (isdigit((unsigned char)src[*i]))
            (*i)++;
    }

    if (src[*i] == 'e' || src[*i] == 'E') {
        is_float = 1;
        (*i)++;
        if (src[*i] == '+' || src[*i] == '-')
            (*i)++;
        while (isdigit((unsigned char)src[*i]))
            (*i)++;
    }

    while (src[*i] == 'u' || src[*i] == 'U' ||
           src[*i] == 'l' || src[*i] == 'L')
        (*i)++;

    token_type_t type = TOK_NUMBER;
    if (src[*i] == 'i' || src[*i] == 'I') {
        (*i)++;
        type = TOK_IMAG_NUMBER;
    }

    size_t len = *i - start;
    (void)is_float;
    append_token(tokens, type, src + start, len, line, *col);
    *col += len;
}

typedef struct {
    char esc;
    char value;
} escape_entry_t;

static const escape_entry_t escape_table[] = {
    { 'n', '\n' },
    { 't', '\t' },
    { 'r', '\r' },
    { 'b', '\b' },
    { 'f', '\f' },
    { 'v', '\v' },
    { '\\', '\\' },
    { '\'', '\'' },
    { '"', '"' }
};

/* Return the numeric value of a hexadecimal digit or -1 when invalid */
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

/* Return the numeric value of an octal digit or -1 when invalid */
static int oct_digit_value(char c)
{
    if (c >= '0' && c <= '7')
        return c - '0';
    return -1;
}

/*
 * Parse up to three octal digits starting at *i. Values above 255 are
 * clamped and trigger a diagnostic.
 */
static int parse_octal(const char *src, size_t *i,
                       size_t line, size_t column)
{
    int value = 0;
    int digits = 0;
    int overflow = 0;
    int digit;
    while (digits < 3 && (digit = oct_digit_value(src[*i])) != -1) {
        int next = value * 8 + digit;
        if (next > 255) {
            overflow = 1;
            value = 255;
        } else {
            value = next;
        }
        (*i)++;
        digits++;
    }
    if (overflow) {
        error_set(&error_ctx,line, column, NULL, NULL);
        error_print(&error_ctx, "Escape sequence out of range");
    }
    return value;
}

/* Parse an \x escape. Up to two hexadecimal digits are consumed. */
static int parse_hex(const char *src, size_t *i)
{
    (*i)++;
    int value = 0;
    int digits = 0;
    int digit;
    while (digits < 2 && (digit = hex_digit_value(src[*i])) != -1) {
        value = value * 16 + digit;
        (*i)++;
        digits++;
    }
    return value;
}

static int unescape_char(const char *src, size_t *i,
                         size_t line, size_t column)
{
    if (!src[*i])
        return 0;

    char c = src[*i];
    for (size_t n = 0; n < sizeof(escape_table)/sizeof(escape_table[0]); n++) {
        if (c == escape_table[n].esc) {
            (*i)++;
            return escape_table[n].value;
        }
    }

    if (c == 'x')
        return parse_hex(src, i);
    if (c >= '0' && c <= '7')
        return parse_octal(src, i, line, column);

    (*i)++;
    return c;
}

static void read_char_const(const char *src, size_t *i, size_t *col,
                            vector_t *tokens, size_t line,
                            token_type_t tok_type)
{
    size_t column = *col;
    (*i)++;
    (*col)++;
    if (!src[*i]) {
        error_set(&error_ctx,line, column, NULL, NULL);
        error_print(&error_ctx, "Missing closing quote");
        append_token(tokens, TOK_UNKNOWN, "", 0, line, column);
        return;
    }

    unsigned char value = (unsigned char)src[*i];
    if (value == '\\') {
        (*i)++;
        value = (unsigned char)unescape_char(src, i, line, column);
    } else {
        (*i)++;
    }
    (*col)++;

    if (src[*i] != '\'') {
        error_set(&error_ctx,line, column, NULL, NULL);
        error_print(&error_ctx, "Missing closing quote");
        append_token(tokens, TOK_UNKNOWN, "", 0, line, column);
        return;
    }

    (*i)++;
    (*col)++;

    char buf[2] = {(char)value, '\0'};
    append_token(tokens, tok_type, buf, 1, line, column);
}

static int read_string_lit(const char *src, size_t *i, size_t *col,
                           vector_t *tokens, size_t line,
                           token_type_t tok_type)
{
    size_t column = *col;
    (*i)++;
    (*col)++;

    vector_t buf_v;
    vector_init(&buf_v, sizeof(char));

    while (src[*i] && src[*i] != '"') {
        char c = src[*i];
        if (c == '\\') {
            (*i)++;
            c = (char)unescape_char(src, i, line, column);
        } else {
            (*i)++;
        }
        if (!vector_push(&buf_v, &c)) {
            vc_oom();
            vector_free(&buf_v);
            return 0;
        }
        (*col)++;
    }
    char nul = '\0';
    if (!vector_push(&buf_v, &nul)) {
        vc_oom();
        vector_free(&buf_v);
        return 0;
    }
    if (src[*i] == '"') {
        (*i)++;
        (*col)++;

        append_token(tokens, tok_type, buf_v.data, buf_v.count - 1,
                     line, column);
        vector_free(&buf_v);
        return 1;
    } else {
        error_set(&error_ctx,line, column, NULL, NULL);
        error_print(&error_ctx, "Missing closing quote");
        vector_free(&buf_v);
        append_token(tokens, TOK_UNKNOWN, "", 0, line, column);
        return 1;
    }
}

int scan_number(const char *src, size_t *i, size_t *col,
                vector_t *tokens, size_t line)
{
    if (!isdigit((unsigned char)src[*i]))
        return 0;
    read_number(src, i, col, tokens, line);
    return 1;
}

int scan_string(const char *src, size_t *i, size_t *col,
                vector_t *tokens, size_t line)
{
    if (src[*i] != '"')
        return 0;
    if (!read_string_lit(src, i, col, tokens, line, TOK_STRING))
        return -1;
    return 1;
}

int scan_char(const char *src, size_t *i, size_t *col,
              vector_t *tokens, size_t line)
{
    if (src[*i] != '\'')
        return 0;
    read_char_const(src, i, col, tokens, line, TOK_CHAR);
    return 1;
}

int scan_wstring(const char *src, size_t *i, size_t *col,
                 vector_t *tokens, size_t line)
{
    if (src[*i] != 'L' || src[*i + 1] != '"')
        return 0;
    (*i)++; (*col)++;
    if (!read_string_lit(src, i, col, tokens, line, TOK_WIDE_STRING))
        return -1;
    return 1;
}

int scan_wchar(const char *src, size_t *i, size_t *col,
               vector_t *tokens, size_t line)
{
    if (src[*i] != 'L' || src[*i + 1] != '\'')
        return 0;
    (*i)++; (*col)++;
    read_char_const(src, i, col, tokens, line, TOK_WIDE_CHAR);
    return 1;
}

