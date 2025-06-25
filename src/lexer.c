/*
 * Lexical analyzer converting source to tokens.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "token.h"
#include "vector.h"

/* Helper to create and append a token to the vector */
static void append_token(vector_t *vec, token_type_t type, const char *lexeme,
                         size_t len, size_t line, size_t column)
{
    char *text = malloc(len + 1);
    if (!text)
        exit(1);
    memcpy(text, lexeme, len);
    text[len] = '\0';
    token_t tok = { type, text, line, column };
    if (!vector_push(vec, &tok))
        exit(1);
}

/* Skip comments and whitespace, updating position counters */
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

/* Read an identifier or keyword starting at src[*i] */
static void read_identifier(const char *src, size_t *i, size_t *col,
                            vector_t *tokens, size_t line)
{
    size_t start = *i;
    while (isalnum((unsigned char)src[*i]) || src[*i] == '_')
        (*i)++;
    size_t len = *i - start;
    token_type_t type = TOK_IDENT;
    if (src[*i] == ':') {
        (*i)++; /* consume ':' */
        append_token(tokens, TOK_LABEL, src + start, len, line, *col);
        *col += len + 1;
        return;
    }
    if (len == 2 && strncmp(src + start, "if", 2) == 0)
        type = TOK_KW_IF;
    else if (len == 4 && strncmp(src + start, "else", 4) == 0)
        type = TOK_KW_ELSE;
    else if (len == 2 && strncmp(src + start, "do", 2) == 0)
        type = TOK_KW_DO;
    else if (len == 5 && strncmp(src + start, "while", 5) == 0)
        type = TOK_KW_WHILE;
    else if (len == 3 && strncmp(src + start, "for", 3) == 0)
        type = TOK_KW_FOR;
    else if (len == 5 && strncmp(src + start, "break", 5) == 0)
        type = TOK_KW_BREAK;
    else if (len == 8 && strncmp(src + start, "continue", 8) == 0)
        type = TOK_KW_CONTINUE;
    else if (len == 4 && strncmp(src + start, "goto", 4) == 0)
        type = TOK_KW_GOTO;
    else if (len == 6 && strncmp(src + start, "switch", 6) == 0)
        type = TOK_KW_SWITCH;
    else if (len == 4 && strncmp(src + start, "case", 4) == 0)
        type = TOK_KW_CASE;
    else if (len == 7 && strncmp(src + start, "default", 7) == 0)
        type = TOK_KW_DEFAULT;
    else if (len == 6 && strncmp(src + start, "sizeof", 6) == 0)
        type = TOK_KW_SIZEOF;
    else if (len == 3 && strncmp(src + start, "int", 3) == 0)
        type = TOK_KW_INT;
    else if (len == 4 && strncmp(src + start, "char", 4) == 0)
        type = TOK_KW_CHAR;
    else if (len == 5 && strncmp(src + start, "float", 5) == 0)
        type = TOK_KW_FLOAT;
    else if (len == 6 && strncmp(src + start, "double", 6) == 0)
        type = TOK_KW_DOUBLE;
    else if (len == 4 && strncmp(src + start, "void", 4) == 0)
        type = TOK_KW_VOID;
    else if (len == 4 && strncmp(src + start, "enum", 4) == 0)
        type = TOK_KW_ENUM;
    else if (len == 6 && strncmp(src + start, "struct", 6) == 0)
        type = TOK_KW_STRUCT;
    else if (len == 5 && strncmp(src + start, "union", 5) == 0)
        type = TOK_KW_UNION;
    else if (len == 6 && strncmp(src + start, "return", 6) == 0)
        type = TOK_KW_RETURN;
    append_token(tokens, type, src + start, len, line, *col);
    *col += len;
}

/* Parse a numeric literal */
static void read_number(const char *src, size_t *i, size_t *col,
                        vector_t *tokens, size_t line)
{
    size_t start = *i;
    while (isdigit((unsigned char)src[*i]))
        (*i)++;
    size_t len = *i - start;
    append_token(tokens, TOK_NUMBER, src + start, len, line, *col);
    *col += len;
}

/* Translate escape sequences within character and string literals */
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

/* Parse a character constant like '\n' or 'a' */
static void read_char_const(const char *src, size_t *i, size_t *col,
                            vector_t *tokens, size_t line)
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
    append_token(tokens, TOK_CHAR, buf, 1, line, column);
}

/* Parse a double-quoted string literal */
static void read_string_lit(const char *src, size_t *i, size_t *col,
                            vector_t *tokens, size_t line)
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
    append_token(tokens, TOK_STRING, buf, len, line, column);
    free(buf);
}

/* Convert punctuation characters to tokens */
static void read_punct(char c, vector_t *tokens, size_t line, size_t column)
{
    token_type_t type = TOK_UNKNOWN;
    switch (c) {
    case '+': type = TOK_PLUS; break;
    case '-': type = TOK_MINUS; break;
    case '.': type = TOK_DOT; break;
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
    case '[': type = TOK_LBRACKET; break;
    case ']': type = TOK_RBRACKET; break;
    case ':': type = TOK_COLON; break;
    default: type = TOK_UNKNOWN; break;
    }
    append_token(tokens, type, &c, 1, line, column);
}

/* Public API */

/* Tokenize the entire source string */
token_t *lexer_tokenize(const char *src, size_t *out_count)
{
    vector_t vec;
    vector_init(&vec, sizeof(token_t));

    size_t i = 0, line = 1, col = 1;
    while (src[i]) {
        skip_whitespace(src, &i, &line, &col);
        if (!src[i])
            break;
        char c = src[i];
        if (isalpha((unsigned char)c) || c == '_') {
            read_identifier(src, &i, &col, &vec, line);
        } else if (isdigit((unsigned char)c)) {
            read_number(src, &i, &col, &vec, line);
        } else if (c == '"') {
            read_string_lit(src, &i, &col, &vec, line);
        } else if (c == '\'') {
            read_char_const(src, &i, &col, &vec, line);
        } else if (c == '=' && src[i + 1] == '=') {
            append_token(&vec, TOK_EQ, "==", 2, line, col);
            i += 2; col += 2;
        } else if (c == '!' && src[i + 1] == '=') {
            append_token(&vec, TOK_NEQ, "!=", 2, line, col);
            i += 2; col += 2;
        } else if (c == '&' && src[i + 1] == '&') {
            append_token(&vec, TOK_LOGAND, "&&", 2, line, col);
            i += 2; col += 2;
        } else if (c == '|' && src[i + 1] == '|') {
            append_token(&vec, TOK_LOGOR, "||", 2, line, col);
            i += 2; col += 2;
        } else if (c == '!') {
            append_token(&vec, TOK_NOT, "!", 1, line, col);
            i++; col++;
        } else if (c == '<' && src[i + 1] == '=') {
            append_token(&vec, TOK_LE, "<=", 2, line, col);
            i += 2; col += 2;
        } else if (c == '>' && src[i + 1] == '=') {
            append_token(&vec, TOK_GE, ">=", 2, line, col);
            i += 2; col += 2;
        } else if (c == '-' && src[i + 1] == '>') {
            append_token(&vec, TOK_ARROW, "->", 2, line, col);
            i += 2; col += 2;
        } else {
            read_punct(c, &vec, line, col);
            i++; col++;
        }
    }

    append_token(&vec, TOK_EOF, "", 0, line, col);
    if (out_count)
        *out_count = vec.count;
    return (token_t *)vec.data;
}

/* Free an array of tokens produced by lexer_tokenize */
void lexer_free_tokens(token_t *tokens, size_t count)
{
    for (size_t i = 0; i < count; i++)
        free(tokens[i].lexeme);
    free(tokens);
}

