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
#include "util.h"

typedef struct {
    const char *kw;
    token_type_t tok;
} keyword_t;

static const keyword_t keyword_table[] = {
    { "if",       TOK_KW_IF },
    { "else",     TOK_KW_ELSE },
    { "do",       TOK_KW_DO },
    { "while",    TOK_KW_WHILE },
    { "for",      TOK_KW_FOR },
    { "break",    TOK_KW_BREAK },
    { "continue", TOK_KW_CONTINUE },
    { "goto",     TOK_KW_GOTO },
    { "switch",   TOK_KW_SWITCH },
    { "case",     TOK_KW_CASE },
    { "default",  TOK_KW_DEFAULT },
    { "sizeof",   TOK_KW_SIZEOF },
    { "int",      TOK_KW_INT },
    { "char",     TOK_KW_CHAR },
    { "float",    TOK_KW_FLOAT },
    { "double",   TOK_KW_DOUBLE },
    { "short",    TOK_KW_SHORT },
    { "long",     TOK_KW_LONG },
    { "bool",     TOK_KW_BOOL },
    { "unsigned", TOK_KW_UNSIGNED },
    { "void",     TOK_KW_VOID },
    { "enum",     TOK_KW_ENUM },
    { "struct",   TOK_KW_STRUCT },
    { "union",    TOK_KW_UNION },
    { "typedef",  TOK_KW_TYPEDEF },
    { "static",   TOK_KW_STATIC },
    { "const",    TOK_KW_CONST },
    { "return",   TOK_KW_RETURN }
};

static token_type_t lookup_keyword(const char *str, size_t len)
{
    for (size_t i = 0; i < sizeof(keyword_table) / sizeof(keyword_table[0]); i++) {
        const keyword_t *kw = &keyword_table[i];
        if (strlen(kw->kw) == len && strncmp(kw->kw, str, len) == 0)
            return kw->tok;
    }
    return TOK_IDENT;
}

/* Helper to create and append a token to the vector */
static void append_token(vector_t *vec, token_type_t type, const char *lexeme,
                         size_t len, size_t line, size_t column)
{
    char *text = vc_alloc_or_exit(len + 1);
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
    type = lookup_keyword(src + start, len);
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
        } else if (c == '<' && src[i + 1] == '<' && src[i + 2] == '=') {
            append_token(&vec, TOK_SHLEQ, "<<=", 3, line, col);
            i += 3; col += 3;
        } else if (c == '>' && src[i + 1] == '>' && src[i + 2] == '=') {
            append_token(&vec, TOK_SHREQ, ">>=", 3, line, col);
            i += 3; col += 3;
        } else if (c == '<' && src[i + 1] == '<') {
            append_token(&vec, TOK_SHL, "<<", 2, line, col);
            i += 2; col += 2;
        } else if (c == '>' && src[i + 1] == '>') {
            append_token(&vec, TOK_SHR, ">>", 2, line, col);
            i += 2; col += 2;
        } else if (c == '<' && src[i + 1] == '=') {
            append_token(&vec, TOK_LE, "<=", 2, line, col);
            i += 2; col += 2;
        } else if (c == '>' && src[i + 1] == '=') {
            append_token(&vec, TOK_GE, ">=", 2, line, col);
            i += 2; col += 2;
        } else if (c == '-' && src[i + 1] == '>') {
            append_token(&vec, TOK_ARROW, "->", 2, line, col);
            i += 2; col += 2;
        } else if (c == '+' && src[i + 1] == '+') {
            append_token(&vec, TOK_INC, "++", 2, line, col);
            i += 2; col += 2;
        } else if (c == '-' && src[i + 1] == '-') {
            append_token(&vec, TOK_DEC, "--", 2, line, col);
            i += 2; col += 2;
        } else if (c == '+' && src[i + 1] == '=') {
            append_token(&vec, TOK_PLUSEQ, "+=", 2, line, col);
            i += 2; col += 2;
        } else if (c == '-' && src[i + 1] == '=') {
            append_token(&vec, TOK_MINUSEQ, "-=", 2, line, col);
            i += 2; col += 2;
        } else if (c == '*' && src[i + 1] == '=') {
            append_token(&vec, TOK_STAREQ, "*=", 2, line, col);
            i += 2; col += 2;
        } else if (c == '/' && src[i + 1] == '=') {
            append_token(&vec, TOK_SLASHEQ, "/=", 2, line, col);
            i += 2; col += 2;
        } else if (c == '%' && src[i + 1] == '=') {
            append_token(&vec, TOK_PERCENTEQ, "%=", 2, line, col);
            i += 2; col += 2;
        } else if (c == '&' && src[i + 1] == '=') {
            append_token(&vec, TOK_AMPEQ, "&=", 2, line, col);
            i += 2; col += 2;
        } else if (c == '|' && src[i + 1] == '=') {
            append_token(&vec, TOK_PIPEEQ, "|=", 2, line, col);
            i += 2; col += 2;
        } else if (c == '^' && src[i + 1] == '=') {
            append_token(&vec, TOK_CARETEQ, "^=", 2, line, col);
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

