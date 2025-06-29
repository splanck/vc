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
#include "error.h"

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
    { "extern",   TOK_KW_EXTERN },
    { "const",    TOK_KW_CONST },
    { "volatile", TOK_KW_VOLATILE },
    { "restrict", TOK_KW_RESTRICT },
    { "register", TOK_KW_REGISTER },
    { "inline",   TOK_KW_INLINE },
    { "return",   TOK_KW_RETURN }
};

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
    char *text = vc_strndup(lexeme, len);
    token_t tok = { type, text, line, column };
    if (!vector_push(vec, &tok))
        exit(1);
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
        num = num * 10 + (src[j] - '0');
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
    int is_float = 0;

    if (src[*i] == '0' && (src[*i + 1] == 'x' || src[*i + 1] == 'X')) {
        (*i) += 2; /* consume 0x */
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
        (*i)++; /* consume '.' */
        while (isdigit((unsigned char)src[*i]))
            (*i)++;
    }

    if (src[*i] == 'e' || src[*i] == 'E') {
        is_float = 1;
        (*i)++; /* consume exponent marker */
        if (src[*i] == '+' || src[*i] == '-')
            (*i)++;
        while (isdigit((unsigned char)src[*i]))
            (*i)++;
    }

    if (src[*i] == 'f' || src[*i] == 'F' ||
        src[*i] == 'l' || src[*i] == 'L') {
        (*i)++;
    }

    size_t len = *i - start;
    (void)is_float; /* not used but kept for clarity */
    append_token(tokens, TOK_NUMBER, src + start, len, line, *col);
    *col += len;
}

/* Translate escape sequences within character and string literals */
/*
 * Translate escape sequences within character and string literals. The
 * index pointer is advanced past the consumed characters.
 */
static int unescape_char(const char *src, size_t *i)
{
    if (!src[*i])
        return 0;

    char c = src[*i];
    switch (c) {
    case 'n':
        (*i)++;
        return '\n';
    case 't':
        (*i)++;
        return '\t';
    case 'r':
        (*i)++;
        return '\r';
    case 'b':
        (*i)++;
        return '\b';
    case 'f':
        (*i)++;
        return '\f';
    case 'v':
        (*i)++;
        return '\v';
    case '\\':
        (*i)++;
        return '\\';
    case '\'':
        (*i)++;
        return '\''; /* single quote */
    case '"':
        (*i)++;
        return '"';
    case 'x': { /* hexadecimal */
        (*i)++; /* skip 'x' */
        int value = 0;
        int digits = 0;
        while (isxdigit((unsigned char)src[*i]) && digits < 2) {
            char d = src[*i];
            int hexval = (d >= '0' && d <= '9') ? d - '0' :
                         (d >= 'a' && d <= 'f') ? d - 'a' + 10 :
                         (d >= 'A' && d <= 'F') ? d - 'A' + 10 : 0;
            value = value * 16 + hexval;
            (*i)++;
            digits++;
        }
        return value;
    }
    default:
        if (c >= '0' && c <= '7') { /* octal */
            int value = 0;
            int digits = 0;
            while (digits < 3 && src[*i] >= '0' && src[*i] <= '7') {
                value = value * 8 + (src[*i] - '0');
                (*i)++;
                digits++;
            }
            return value;
        }
        (*i)++;
        return c;
    }
}

/* Parse a character constant like '\n' or 'a' */
static void read_char_const(const char *src, size_t *i, size_t *col,
                            vector_t *tokens, size_t line,
                            token_type_t tok_type)
{
    size_t column = *col;
    (*i)++; /* skip opening quote */
    (*col)++;
    if (!src[*i]) {
        error_set(line, column, error_current_file, error_current_function);
        error_print("Missing closing quote");
        append_token(tokens, TOK_UNKNOWN, "", 0, line, column);
        return;
    }

    char value = src[*i];
    if (value == '\\') {
        (*i)++; /* skip backslash */
        value = (char)unescape_char(src, i);
    } else {
        (*i)++; /* consume character */
    }
    (*col)++;

    if (src[*i] != '\'') {
        error_set(line, column, error_current_file, error_current_function);
        error_print("Missing closing quote");
        append_token(tokens, TOK_UNKNOWN, "", 0, line, column);
        return;
    }

    (*i)++;
    (*col)++;

    char buf[2] = {value, '\0'};
    append_token(tokens, tok_type, buf, 1, line, column);
}

/* Parse a double-quoted string literal */
static void read_string_lit(const char *src, size_t *i, size_t *col,
                            vector_t *tokens, size_t line,
                            token_type_t tok_type)
{
    size_t column = *col;
    (*i)++; /* skip opening quote */
    (*col)++;

    vector_t buf_v;
    vector_init(&buf_v, sizeof(char));

    while (src[*i] && src[*i] != '"') {
        char c = src[*i];
        if (c == '\\') {
            (*i)++; /* skip backslash */
            c = (char)unescape_char(src, i);
        } else {
            (*i)++; /* consume character */
        }
        if (!vector_push(&buf_v, &c)) {
            vector_free(&buf_v);
            return;
        }
        (*col)++;
    }
    /* NUL-terminate the buffer for convenience */
    char nul = '\0';
    if (!vector_push(&buf_v, &nul)) {
        vector_free(&buf_v);
        return;
    }
    if (src[*i] == '"') {
        (*i)++;
        (*col)++;

        append_token(tokens, tok_type, buf_v.data, buf_v.count - 1,
                     line, column);
        vector_free(&buf_v);
    } else {
        error_set(line, column, error_current_file, error_current_function);
        error_print("Missing closing quote");
        vector_free(&buf_v);
        append_token(tokens, TOK_UNKNOWN, "", 0, line, column);
    }
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

/* Shallow scanning helpers */
static int scan_identifier(const char *src, size_t *i, size_t *col,
                           vector_t *tokens, size_t line)
{
    char c = src[*i];
    if (!isalpha((unsigned char)c) && c != '_')
        return 0;
    read_identifier(src, i, col, tokens, line);
    return 1;
}

static int scan_number(const char *src, size_t *i, size_t *col,
                       vector_t *tokens, size_t line)
{
    if (!isdigit((unsigned char)src[*i]))
        return 0;
    read_number(src, i, col, tokens, line);
    return 1;
}

static int scan_string(const char *src, size_t *i, size_t *col,
                       vector_t *tokens, size_t line)
{
    if (src[*i] != '"')
        return 0;
    read_string_lit(src, i, col, tokens, line, TOK_STRING);
    return 1;
}

static int scan_char(const char *src, size_t *i, size_t *col,
                     vector_t *tokens, size_t line)
{
    if (src[*i] != '\'')
        return 0;
    read_char_const(src, i, col, tokens, line, TOK_CHAR);
    return 1;
}

static int scan_wstring(const char *src, size_t *i, size_t *col,
                        vector_t *tokens, size_t line)
{
    if (src[*i] != 'L' || src[*i + 1] != '"')
        return 0;
    (*i)++; (*col)++;
    read_string_lit(src, i, col, tokens, line, TOK_WIDE_STRING);
    return 1;
}

static int scan_wchar(const char *src, size_t *i, size_t *col,
                      vector_t *tokens, size_t line)
{
    if (src[*i] != 'L' || src[*i + 1] != '\'')
        return 0;
    (*i)++; (*col)++;
    read_char_const(src, i, col, tokens, line, TOK_WIDE_CHAR);
    return 1;
}

static int scan_punct_table(const char *src, size_t *i, size_t *col,
                            vector_t *tokens, size_t line)
{
    for (size_t p = 0; p < sizeof(punct_table) / sizeof(punct_table[0]); p++) {
        const punct_entry_t *entry = &punct_table[p];
        size_t len = strlen(entry->op);
        if (strncmp(src + *i, entry->op, len) == 0) {
            append_token(tokens, entry->tok, entry->op, len, line, *col);
            *i += len;
            *col += len;
            return 1;
        }
    }
    return 0;
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
        if (scan_wstring(src, &i, &col, &vec, line) ||
            scan_wchar(src, &i, &col, &vec, line) ||
            scan_identifier(src, &i, &col, &vec, line) ||
            scan_number(src, &i, &col, &vec, line) ||
            scan_string(src, &i, &col, &vec, line) ||
            scan_char(src, &i, &col, &vec, line)) {
            continue;
        }

        if (scan_punct_table(src, &i, &col, &vec, line))
            continue;

        read_punct(c, &vec, line, col);
        i++; col++;
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

