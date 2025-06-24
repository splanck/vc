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

static void read_identifier(const char *src, size_t *i, size_t *col,
                            token_t **tokens, size_t *count, size_t *cap,
                            size_t line)
{
    size_t start = *i;
    while (isalnum((unsigned char)src[*i]) || src[*i] == '_')
        (*i)++;
    size_t len = *i - start;
    token_type_t type = TOK_IDENT;
    if (len == 2 && strncmp(src + start, "if", 2) == 0)
        type = TOK_KW_IF;
    else if (len == 4 && strncmp(src + start, "else", 4) == 0)
        type = TOK_KW_ELSE;
    else if (len == 5 && strncmp(src + start, "while", 5) == 0)
        type = TOK_KW_WHILE;
    else if (len == 3 && strncmp(src + start, "for", 3) == 0)
        type = TOK_KW_FOR;
    else if (len == 5 && strncmp(src + start, "break", 5) == 0)
        type = TOK_KW_BREAK;
    else if (len == 8 && strncmp(src + start, "continue", 8) == 0)
        type = TOK_KW_CONTINUE;
    else if (len == 3 && strncmp(src + start, "int", 3) == 0)
        type = TOK_KW_INT;
    else if (len == 4 && strncmp(src + start, "void", 4) == 0)
        type = TOK_KW_VOID;
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

static int unescape_char(char c)
{
    switch (c) {
    case 'n': return '\n';
    case 't': return '\t';
    case '\\': return '\\';
    case '\'': return '\''; /* single quote */
    case '"': return '"';
    default: return c;
    }
}

static void read_char_const(const char *src, size_t *i, size_t *col,
                            token_t **tokens, size_t *count, size_t *cap,
                            size_t line)
{
    size_t column = *col;
    (*i)++; /* skip opening quote */
    (*col)++;
    char value = src[*i];
    if (value == '\\') {
        (*i)++;
        value = (char)unescape_char(src[*i]);
    }
    (*i)++;
    (*col)++;
    if (src[*i] == '\'') {
        (*i)++;
        (*col)++;
    }
    char buf[2] = {value, '\0'};
    append_token(tokens, count, cap, TOK_CHAR, buf, 1, line, column);
}

static void read_string_lit(const char *src, size_t *i, size_t *col,
                            token_t **tokens, size_t *count, size_t *cap,
                            size_t line)
{
    size_t column = *col;
    (*i)++; /* skip opening quote */
    (*col)++;
    size_t cap_buf = 16, len = 0;
    char *buf = malloc(cap_buf);
    if (!buf)
        return;
    while (src[*i] && src[*i] != '"') {
        char c = src[*i];
        if (c == '\\') {
            (*i)++;
            c = (char)unescape_char(src[*i]);
        }
        if (len + 1 >= cap_buf) {
            cap_buf *= 2;
            char *tmp = realloc(buf, cap_buf);
            if (!tmp) { free(buf); return; }
            buf = tmp;
        }
        buf[len++] = c;
        (*i)++;
        (*col)++;
    }
    if (src[*i] == '"') {
        (*i)++;
        (*col)++;
    }
    append_token(tokens, count, cap, TOK_STRING, buf, len, line, column);
    free(buf);
}

static void read_punct(char c, token_t **tokens, size_t *count, size_t *cap,
                       size_t line, size_t column)
{
    token_type_t type = TOK_UNKNOWN;
    switch (c) {
    case '+': type = TOK_PLUS; break;
    case '-': type = TOK_MINUS; break;
    case '&': type = TOK_AMP; break;
    case '*': type = TOK_STAR; break;
    case '/': type = TOK_SLASH; break;
    case '=': type = TOK_ASSIGN; break;
    case '<': type = TOK_LT; break;
    case '>': type = TOK_GT; break;
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
        } else if (c == '"') {
            read_string_lit(src, &i, &col, &tokens, &count, &cap, line);
        } else if (c == '\'') {
            read_char_const(src, &i, &col, &tokens, &count, &cap, line);
        } else if (c == '=' && src[i + 1] == '=') {
            append_token(&tokens, &count, &cap, TOK_EQ, "==", 2, line, col);
            i += 2; col += 2;
        } else if (c == '!' && src[i + 1] == '=') {
            append_token(&tokens, &count, &cap, TOK_NEQ, "!=", 2, line, col);
            i += 2; col += 2;
        } else if (c == '<' && src[i + 1] == '=') {
            append_token(&tokens, &count, &cap, TOK_LE, "<=", 2, line, col);
            i += 2; col += 2;
        } else if (c == '>' && src[i + 1] == '=') {
            append_token(&tokens, &count, &cap, TOK_GE, ">=", 2, line, col);
            i += 2; col += 2;
        } else {
            read_punct(c, &tokens, &count, &cap, line, col);
            i++; col++;
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

