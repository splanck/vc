/*
 * Lexical analyzer converting source to tokens.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "token.h"
#include "vector.h"
#include "util.h"
#include "error.h"
#include "lexer_internal.h"

typedef struct {
    const char *op;
    token_type_t tok;
} punct_entry_t;

/* ordered longest to shortest for greedy matching */
static const punct_entry_t punct_table[] = {
    { "...", TOK_ELLIPSIS },
    { "<<=", TOK_SHLEQ },
    { ">>=", TOK_SHREQ },
    { "==",  TOK_EQ },
    { "!=",  TOK_NEQ },
    { "&&",  TOK_LOGAND },
    { "||",  TOK_LOGOR },
    { "<<",  TOK_SHL },
    { ">>",  TOK_SHR },
    { "<=",  TOK_LE },
    { ">=",  TOK_GE },
    { "->",  TOK_ARROW },
    { "++",  TOK_INC },
    { "--",  TOK_DEC },
    { "+=",  TOK_PLUSEQ },
    { "-=",  TOK_MINUSEQ },
    { "*=",  TOK_STAREQ },
    { "/=",  TOK_SLASHEQ },
    { "%=",  TOK_PERCENTEQ },
    { "&=",  TOK_AMPEQ },
    { "|=",  TOK_PIPEEQ },
    { "^=",  TOK_CARETEQ },
    { "!",   TOK_NOT }
};

/* Iterate the keyword table and return the matching token type, or
 * TOK_IDENT if the text is not a keyword.
 */

/* Helper to create and append a token to the vector */
int append_token(vector_t *vec, token_type_t type, const char *lexeme,
                 size_t len, size_t line, size_t column)
{
    char *text = vc_strndup(lexeme, len);
    if (!text) {
        vc_oom();
        return 0;
    }
    token_t tok = { type, text, line, column };
    if (!vector_push(vec, &tok)) {
        free(text);
        vc_oom();
        return 0;
    }
    return 1;
}

/* Parse a line marker of the form '# <num> "file"' and update counters */
static int consume_line_marker(const char *src, size_t *i,
                               size_t *line, size_t *col)
{
    size_t j = *i;
    if (*col != 1 || src[j] != '#')
        return 0;
    j++;
    if (src[j] != ' ')
        return 0;
    j++;
    if (!isdigit((unsigned char)src[j]))
        return 0;
    size_t num = 0;
    while (isdigit((unsigned char)src[j])) {
        num = num * 10 + (size_t)(src[j] - '0');
        j++;
    }
    while (src[j] == ' ' || src[j] == '\t')
        j++;
    if (src[j] == '"') {
        j++;
        while (src[j] && src[j] != '"')
            j++;
        if (src[j] == '"')
            j++;
    }
    while (src[j] && src[j] != '\n')
        j++;
    if (src[j] == '\n')
        j++;
    *i = j;
    *line = num;
    *col = 1;
    return 1;
}

/* Skip comments and whitespace, updating position counters */
static void skip_whitespace(const char *src, size_t *i, size_t *line,
                            size_t *col)
{
    while (src[*i]) {
        if (consume_line_marker(src, i, line, col))
            continue;
        char c = src[*i];
        if (c == '/' && src[*i + 1] == '/') { /* line comment */
            (*i) += 2;
            (*col) += 2;
            while (src[*i] && src[*i] != '\n') {
                (*i)++;
                (*col)++;
            }
        } else if (c == '/' && src[*i + 1] == '*') { /* block comment */
            (*i) += 2;
            (*col) += 2;
            while (src[*i]) {
                if (src[*i] == '\n') {
                    (*line)++;
                    *col = 1;
                    (*i)++;
                } else if (src[*i] == '*' && src[*i + 1] == '/') {
                    (*i) += 2;
                    (*col) += 2;
                    break;
                } else {
                    (*i)++;
                    (*col)++;
                }
            }
        } else if (c == '\n') {
            (*line)++;
            *col = 1;
            (*i)++;
        } else if (isspace((unsigned char)c)) {
            (*i)++;
            (*col)++;
        } else {
            break;
        }
    }
}

/* Convert punctuation characters to tokens */
static int read_punct(char c, vector_t *tokens, size_t line, size_t column)
{
    token_type_t type = TOK_UNKNOWN;
    switch (c) {
    case '+': type = TOK_PLUS; break;
    case '-': type = TOK_MINUS; break;
    case '.': type = TOK_DOT; break;
    case '&': type = TOK_AMP; break;
    case '|': type = TOK_PIPE; break;
    case '^': type = TOK_CARET; break;
    case '*': type = TOK_STAR; break;
    case '/': type = TOK_SLASH; break;
    case '%': type = TOK_PERCENT; break;
    case '=': type = TOK_ASSIGN; break;
    case '<': type = TOK_LT; break;
    case '>': type = TOK_GT; break;
    case ';': type = TOK_SEMI; break;
    case ',': type = TOK_COMMA; break;
    case '(': type = TOK_LPAREN; break;
    case ')': type = TOK_RPAREN; break;
    case '{': type = TOK_LBRACE; break;
    case '}': type = TOK_RBRACE; break;
    case '[': type = TOK_LBRACKET; break;
    case ']': type = TOK_RBRACKET; break;
    case '?': type = TOK_QMARK; break;
    case ':': type = TOK_COLON; break;
    default: type = TOK_UNKNOWN; break;
    }
    return append_token(tokens, type, &c, 1, line, column);
}


static int scan_punct_table(const char *src, size_t *i, size_t *col,
                            vector_t *tokens, size_t line)
{
    for (size_t p = 0; p < sizeof(punct_table) / sizeof(punct_table[0]); p++) {
        const punct_entry_t *entry = &punct_table[p];
        size_t len = strlen(entry->op);
        if (strncmp(src + *i, entry->op, len) == 0) {
            if (!append_token(tokens, entry->tok, entry->op, len, line, *col))
                return -1;
            *i += len;
            *col += len;
            return 1;
        }
    }
    return 0;
}

/* wrapper to scan numeric literals */
static int scan_number_token(const char *src, size_t *i, size_t *col,
                             vector_t *tokens, size_t line)
{
    return scan_number(src, i, col, tokens, line);
}

/* wrapper to scan string literals including wide strings */
static int scan_string_tokens(const char *src, size_t *i, size_t *col,
                              vector_t *tokens, size_t line)
{
    int res = scan_wstring(src, i, col, tokens, line);
    if (res)
        return res;
    return scan_string(src, i, col, tokens, line);
}

/* wrapper to scan character literals including wide chars */
static int scan_char_tokens(const char *src, size_t *i, size_t *col,
                            vector_t *tokens, size_t line)
{
    int res = scan_wchar(src, i, col, tokens, line);
    if (res)
        return res;
    return scan_char(src, i, col, tokens, line);
}

/* handle punctuation tokens */
static int scan_punctuation(const char *src, size_t *i, size_t *col,
                            vector_t *tokens, size_t line)
{
    int res = scan_punct_table(src, i, col, tokens, line);
    if (res)
        return res;
    char c = src[*i];
    if (!read_punct(c, tokens, line, *col))
        return -1;
    (*i)++; (*col)++;
    return 1;
}

/* Scan and append the next token from the source. Returns 1 on success,
 * 0 when end of input is reached and -1 on error. */
static int scan_strings(const char *src, size_t *i, size_t *col,
                        vector_t *tokens, size_t line)
{
    int res = scan_string_tokens(src, i, col, tokens, line);
    if (res)
        return res;
    return scan_char_tokens(src, i, col, tokens, line);
}

static int scan_ident_or_number(const char *src, size_t *i, size_t *col,
                                vector_t *tokens, size_t line)
{
    if (scan_identifier(src, i, col, tokens, line))
        return 1;
    return scan_number_token(src, i, col, tokens, line);
}

/*
 * Scan and append the next token from the source string.
 *
 * Returns 1 on success, 0 when end of input is reached and -1 on error.
 */
static int scan_next_token(const char *src, size_t *i, size_t *line,
                           size_t *col, vector_t *tokens)
{
    skip_whitespace(src, i, line, col);
    if (!src[*i])
        return 0;

    int res;

    res = scan_strings(src, i, col, tokens, *line);
    if (res)
        return res;

    res = scan_ident_or_number(src, i, col, tokens, *line);
    if (res)
        return res;

    return scan_punctuation(src, i, col, tokens, *line);
}

/* free tokens stored in the vector and return NULL on error */
static token_t *cleanup_error_tokens(vector_t *vec)
{
    token_t *tokens = (token_t *)vec->data;
    for (size_t j = 0; j < vec->count; j++)
        free(tokens[j].lexeme);
    vector_free(vec);
    return NULL;
}

/* Public API */

/* Tokenize the entire source string */
token_t *lexer_tokenize(const char *src, size_t *out_count)
{
    vector_t vec;
    vector_init(&vec, sizeof(token_t));

    /*
     * Scan tokens sequentially until scan_next_token reports end of
     * input. A negative return value indicates a lexing error and
     * triggers cleanup of any tokens already produced.
     */

    size_t i = 0, line = 1, col = 1;
    /*
     * Repeatedly scan tokens. The loop stops when scan_next_token
     * returns 0 (no more input) or a negative value for an error.
     */
    while (1) {
        int res = scan_next_token(src, &i, &line, &col, &vec);
        if (res < 0)
            return cleanup_error_tokens(&vec); /* propagate scanning error */
        if (res == 0)
            break; /* reached end of input */
    }

    /* add explicit EOF marker after the last real token */
    if (!append_token(&vec, TOK_EOF, "", 0, line, col))
        return cleanup_error_tokens(&vec);
    if (out_count)
        *out_count = vec.count;
    return (token_t *)vec.data;
}

/* Free an array of tokens produced by lexer_tokenize */
void lexer_free_tokens(token_t *tokens, size_t count)
{
    if (!tokens || count == 0)
        return;

    for (size_t i = 0; i < count; i++)
        free(tokens[i].lexeme);
    free(tokens);
}

