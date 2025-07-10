#include <ctype.h>
#include <string.h>
#include "token.h"
#include "vector.h"
#include "util.h"
#include "error.h"
#include "lexer_internal.h"

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
    { "_Bool",    TOK_KW_BOOL },
    { "_Complex", TOK_KW_COMPLEX },
    { "alignas",  TOK_KW_ALIGNAS },
    { "_Alignof", TOK_KW_ALIGNOF },
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
    { "_Noreturn",       TOK_KW_NORETURN },
    { "_Static_assert",  TOK_KW_STATIC_ASSERT },
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

int scan_identifier(const char *src, size_t *i, size_t *col,
                    vector_t *tokens, size_t line)
{
    char c = src[*i];
    if (!isalpha((unsigned char)c) && c != '_')
        return 0;
    read_identifier(src, i, col, tokens, line);
    return 1;
}
