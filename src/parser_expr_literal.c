#include <stdlib.h>
#include <string.h>
#include "parser.h"
#include "parser_types.h"
#include "ast_expr.h"
#include "util.h"
#include "parser_expr_literal.h"

/* Parse a possibly concatenated string literal. */
static expr_t *parse_string_literal(parser_t *p)
{
    token_t *tok = peek(p);
    if (!tok || (tok->type != TOK_STRING && tok->type != TOK_WIDE_STRING))
        return NULL;

    int is_wide = (tok->type == TOK_WIDE_STRING);
    size_t line = tok->line;
    size_t column = tok->column;

    size_t len = strlen(tok->lexeme);
    char *buf = vc_strdup(tok->lexeme);
    if (!buf)
        return NULL;
    p->pos++;

    while (p->pos < p->count) {
        token_t *next = &p->tokens[p->pos];
        if (next->type != tok->type)
            break;
        size_t nlen = strlen(next->lexeme);
        buf = vc_realloc_or_exit(buf, len + nlen + 1);
        memcpy(buf + len, next->lexeme, nlen + 1);
        len += nlen;
        p->pos++;
    }

    expr_t *res = is_wide ?
        ast_make_wstring(buf, line, column) :
        ast_make_string(buf, line, column);
    free(buf);
    return res;
}

expr_t *parse_literal(parser_t *p)
{
    token_t *tok = peek(p);
    if (!tok)
        return NULL;
    if (tok->type == TOK_NUMBER || tok->type == TOK_FLOAT) {
        size_t save = p->pos;
        token_t *num_tok = tok;
        p->pos++; /* consume number */
        token_t *op_tok = peek(p);
        if (op_tok && (op_tok->type == TOK_PLUS || op_tok->type == TOK_MINUS)) {
            p->pos++; /* consume +/- */
            token_t *imag_tok = peek(p);
            if (imag_tok && imag_tok->type == TOK_IMAG_NUMBER) {
                p->pos++; /* consume imag */
                double real = strtod(num_tok->lexeme, NULL);
                double imag = strtod(imag_tok->lexeme, NULL);
                if (op_tok->type == TOK_MINUS)
                    imag = -imag;
                return ast_make_complex_literal(real, imag,
                                               num_tok->line, num_tok->column);
            }
        }
        p->pos = save;
    }
    if (match(p, TOK_IMAG_NUMBER)) {
        double imag = strtod(tok->lexeme, NULL);
        return ast_make_complex_literal(0.0, imag, tok->line, tok->column);
    }
    if (match(p, TOK_FLOAT))
        return ast_make_number(tok->lexeme, tok->line, tok->column);
    if (match(p, TOK_NUMBER))
        return ast_make_number(tok->lexeme, tok->line, tok->column);
    expr_t *s = parse_string_literal(p);
    if (s)
        return s;
    if (match(p, TOK_CHAR))
        return ast_make_char(tok->lexeme[0], tok->line, tok->column);
    if (match(p, TOK_WIDE_CHAR))
        return ast_make_wchar(tok->lexeme[0], tok->line, tok->column);
    return NULL;
}

