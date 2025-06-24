#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "token.h"

static void append_token(token_t **tokens, size_t *count, size_t *cap,
                         token_type_t type, const char *lexeme,
                         size_t len, size_t line, size_t column)
{
    if (*count >= *cap) {
        *cap *= 2;
        *tokens = realloc(*tokens, (*cap) * sizeof(**tokens));
        if (!*tokens)
            exit(1);
    }
    char *text = malloc(len + 1);
    if (!text)
        exit(1);
    memcpy(text, lexeme, len);
    text[len] = '\0';
    (*tokens)[(*count)++] = (token_t){ type, text, line, column };
}

static void skip_whitespace(const char *src, size_t *i, size_t *line,
                            size_t *col)
{
    while (src[*i]) {
        char c = src[*i];
        if (c == '\n') {
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

static void read_identifier(const char *src, size_t *i, size_t *col,
                            token_t **tokens, size_t *count, size_t *cap,
                            size_t line)
{
    size_t start = *i;
    while (isalnum((unsigned char)src[*i]) || src[*i] == '_')
        (*i)++;
    size_t len = *i - start;
    token_type_t type = TOK_IDENT;
    if (len == 3 && strncmp(src + start, "int", 3) == 0)
        type = TOK_KW_INT;
    else if (len == 6 && strncmp(src + start, "return", 6) == 0)
        type = TOK_KW_RETURN;
    append_token(tokens, count, cap, type, src + start, len, line, *col);
    *col += len;
}

static void read_number(const char *src, size_t *i, size_t *col,
                        token_t **tokens, size_t *count, size_t *cap,
                        size_t line)
{
    size_t start = *i;
    while (isdigit((unsigned char)src[*i]))
        (*i)++;
    size_t len = *i - start;
    append_token(tokens, count, cap, TOK_NUMBER, src + start, len, line, *col);
    *col += len;
}

static void read_punct(char c, token_t **tokens, size_t *count, size_t *cap,
                       size_t line, size_t column)
{
    token_type_t type = TOK_UNKNOWN;
    switch (c) {
    case '+': type = TOK_PLUS; break;
    case '-': type = TOK_MINUS; break;
    case '*': type = TOK_STAR; break;
    case '/': type = TOK_SLASH; break;
    case ';': type = TOK_SEMI; break;
    case ',': type = TOK_COMMA; break;
    case '(': type = TOK_LPAREN; break;
    case ')': type = TOK_RPAREN; break;
    case '{': type = TOK_LBRACE; break;
    case '}': type = TOK_RBRACE; break;
    default: type = TOK_UNKNOWN; break;
    }
    append_token(tokens, count, cap, type, &c, 1, line, column);
}

/* Public API */

token_t *lexer_tokenize(const char *src, size_t *out_count)
{
    size_t cap = 16;
    size_t count = 0;
    token_t *tokens = malloc(cap * sizeof(*tokens));
    if (!tokens)
        return NULL;

    size_t i = 0, line = 1, col = 1;
    while (src[i]) {
        skip_whitespace(src, &i, &line, &col);
        if (!src[i])
            break;
        char c = src[i];
        if (isalpha((unsigned char)c) || c == '_') {
            read_identifier(src, &i, &col, &tokens, &count, &cap, line);
        } else if (isdigit((unsigned char)c)) {
            read_number(src, &i, &col, &tokens, &count, &cap, line);
        } else {
            read_punct(c, &tokens, &count, &cap, line, col);
            i++;
            col++;
        }
    }

    append_token(&tokens, &count, &cap, TOK_EOF, "", 0, line, col);
    if (out_count)
        *out_count = count;
    return tokens;
}

void lexer_free_tokens(token_t *tokens, size_t count)
{
    for (size_t i = 0; i < count; i++)
        free(tokens[i].lexeme);
    free(tokens);
}

