#include <stdlib.h>
#include "token.h"
#include "parser.h"
#include "parser_core.h"
#include "ast_stmt.h"
#include "vector.h"
#include "symtable.h"
#include "error.h"

int compile_parse_impl(token_t *toks, size_t count,
                       vector_t *funcs_v, vector_t *globs_v,
                       symtable_t *funcs)
{
    parser_t parser;
    parser_init(&parser, toks, count);
    symtable_init(funcs);
    vector_init(funcs_v, sizeof(func_t *));
    vector_init(globs_v, sizeof(stmt_t *));

    int ok = 1;
    func_t *err_fn = NULL;
    stmt_t *err_g = NULL;
    while (ok && !parser_is_eof(&parser)) {
        func_t *fn = NULL;
        stmt_t *g = NULL;
        if (!parser_parse_toplevel(&parser, funcs, &fn, &g)) {
            token_type_t expected[] = { TOK_KW_INT, TOK_KW_VOID };
            parser_print_error(&parser, expected, 2);
            err_fn = fn;
            err_g = g;
            ok = 0;
            break;
        }
        if (fn) {
            if (!vector_push(funcs_v, &fn)) {
                ok = 0;
                ast_free_func(fn);
            }
        } else if (g) {
            if (!vector_push(globs_v, &g)) {
                ok = 0;
                ast_free_stmt(g);
            }
        }
    }
    if (!ok) {
        if (err_fn)
            ast_free_func(err_fn);
        if (err_g)
            ast_free_stmt(err_g);
    }
    return ok;
}

