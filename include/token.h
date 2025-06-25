/*
 * Token definitions for the lexer.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_TOKEN_H
#define VC_TOKEN_H

#include <stddef.h>

/* Token types used by the lexer */
typedef enum {
    TOK_EOF = 0,
    TOK_IDENT,
    TOK_NUMBER,
    TOK_STRING,
    TOK_CHAR,
    TOK_KW_INT,
    TOK_KW_CHAR,
    TOK_KW_VOID,
    TOK_KW_ENUM,
    TOK_KW_STRUCT,
    TOK_KW_UNION,
    TOK_KW_RETURN,
    TOK_KW_IF,
    TOK_KW_ELSE,
    TOK_KW_DO,
    TOK_KW_WHILE,
    TOK_KW_FOR,
    TOK_KW_BREAK,
    TOK_KW_CONTINUE,
    TOK_KW_GOTO,
    TOK_KW_SWITCH,
    TOK_KW_CASE,
    TOK_KW_DEFAULT,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_LBRACE,
    TOK_RBRACE,
    TOK_SEMI,
    TOK_COMMA,
    TOK_PLUS,
    TOK_MINUS,
    TOK_DOT,
    TOK_ARROW,
    TOK_AMP,
    TOK_STAR,
    TOK_SLASH,
    TOK_ASSIGN,
    TOK_EQ,
    TOK_NEQ,
    TOK_LT,
    TOK_GT,
    TOK_LE,
    TOK_GE,
    TOK_LBRACKET,
    TOK_RBRACKET,
    TOK_COLON,
    TOK_LABEL,
    TOK_UNKNOWN
} token_type_t;

/* Representation of a single lexical token */
typedef struct {
    token_type_t type;
    char *lexeme; /* NUL terminated string */
    size_t line;
    size_t column;
} token_t;

/* Tokenize the given C source string. The returned array must be freed
 * with lexer_free_tokens(). If out_count is not NULL it is set to the
 * number of tokens returned. */
token_t *lexer_tokenize(const char *src, size_t *out_count);

/* Free an array of tokens returned by lexer_tokenize(). */
void lexer_free_tokens(token_t *tokens, size_t count);

#endif /* VC_TOKEN_H */
