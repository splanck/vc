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
    TOK_WIDE_STRING,
    TOK_WIDE_CHAR,
    TOK_KW_INT,
    TOK_KW_CHAR,
    TOK_KW_FLOAT,
    TOK_KW_DOUBLE,
    TOK_KW_SHORT,
    TOK_KW_LONG,
    TOK_KW_BOOL,
    TOK_KW_UNSIGNED,
    TOK_KW_VOID,
    TOK_KW_ENUM,
    TOK_KW_STRUCT,
    TOK_KW_UNION,
    TOK_KW_TYPEDEF,
    TOK_KW_STATIC,
    TOK_KW_EXTERN,
    TOK_KW_CONST,
    TOK_KW_VOLATILE,
    TOK_KW_RESTRICT,
    TOK_KW_REGISTER,
    TOK_KW_INLINE,
    TOK_KW_STATIC_ASSERT,
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
    TOK_KW_SIZEOF,
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
    TOK_PERCENT,
    TOK_PIPE,
    TOK_CARET,
    TOK_SHL,
    TOK_SHR,
    TOK_PLUSEQ,
    TOK_MINUSEQ,
    TOK_STAREQ,
    TOK_SLASHEQ,
    TOK_PERCENTEQ,
    TOK_AMPEQ,
    TOK_PIPEEQ,
    TOK_CARETEQ,
    TOK_SHLEQ,
    TOK_SHREQ,
    TOK_INC,
    TOK_DEC,
    TOK_ASSIGN,
    TOK_EQ,
    TOK_NEQ,
    TOK_LOGAND,
    TOK_LOGOR,
    TOK_NOT,
    TOK_LT,
    TOK_GT,
    TOK_LE,
    TOK_GE,
    TOK_LBRACKET,
    TOK_RBRACKET,
    TOK_QMARK,
    TOK_COLON,
    TOK_LABEL,
    TOK_ELLIPSIS,
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

/* Free an array of tokens returned by lexer_tokenize().  If \a tokens is NULL
 * or \a count is zero the function does nothing. */
void lexer_free_tokens(token_t *tokens, size_t count);

#endif /* VC_TOKEN_H */
