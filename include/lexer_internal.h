#ifndef VC_LEXER_INTERNAL_H
#define VC_LEXER_INTERNAL_H

#include <stddef.h>
#include "token.h"
#include "vector.h"

int append_token(vector_t *vec, token_type_t type, const char *lexeme,
                 size_t len, size_t line, size_t column);

int scan_identifier(const char *src, size_t *i, size_t *col,
                    vector_t *tokens, size_t line);

int scan_number(const char *src, size_t *i, size_t *col,
                vector_t *tokens, size_t line);
int scan_string(const char *src, size_t *i, size_t *col,
                vector_t *tokens, size_t line);
int scan_char(const char *src, size_t *i, size_t *col,
              vector_t *tokens, size_t line);
int scan_wstring(const char *src, size_t *i, size_t *col,
                 vector_t *tokens, size_t line);
int scan_wchar(const char *src, size_t *i, size_t *col,
               vector_t *tokens, size_t line);

#endif /* VC_LEXER_INTERNAL_H */
