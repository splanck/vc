#include <stdio.h>
#include <stdlib.h>
#include "util.h"
#include "cli.h"
#include "token.h"
#include "parser.h"
#include "vector.h"
#include "symtable.h"
#include "semantic.h"
#include "ir.h"
#include "opt.h"
#include "codegen.h"

int main(int argc, char **argv)
{
    cli_options_t cli;
    if (cli_parse_args(argc, argv, &cli) != 0)
        return 1;

    const char *source = cli.source;
    char *output = cli.output;
    opt_config_t opt_cfg = cli.opt_cfg;
    int use_x86_64 = cli.use_x86_64;
    int dump_asm = cli.dump_asm;
    int dump_ir = cli.dump_ir;

    char *src_text = vc_read_file(source);
    if (!src_text) {
        perror("vc_read_file");
        return 1;
    }

    size_t tok_count = 0;
    token_t *tokens = lexer_tokenize(src_text, &tok_count);
    if (!tokens) {
        fprintf(stderr, "Tokenization failed\n");
        free(src_text);
        return 1;
    }

    parser_t parser;
    parser_init(&parser, tokens, tok_count);
    symtable_t funcs;
    symtable_t globals;
    symtable_init(&funcs);
    symtable_init(&globals);
    ir_builder_t ir;
    ir_builder_init(&ir);

    int ok = 1;
    vector_t func_list_v, glob_list_v;
    vector_init(&func_list_v, sizeof(func_t *));
    vector_init(&glob_list_v, sizeof(stmt_t *));
    while (ok && !parser_is_eof(&parser)) {
        func_t *fn = NULL;
        stmt_t *g = NULL;
        if (!parser_parse_toplevel(&parser, &fn, &g)) {
            token_type_t expected[] = { TOK_KW_INT, TOK_KW_VOID };
            parser_print_error(&parser, expected, 2);
            ok = 0;
            break;
        }
        if (fn) {
            if (!vector_push(&func_list_v, &fn)) {
                ok = 0;
                ast_free_func(fn);
                break;
            }
        } else if (g) {
            if (!vector_push(&glob_list_v, &g)) {
                ok = 0;
                ast_free_stmt(g);
                break;
            }
        }
    }

    func_t **func_list = (func_t **)func_list_v.data;
    size_t fcount = func_list_v.count;
    stmt_t **glob_list = (stmt_t **)glob_list_v.data;
    size_t gcount = glob_list_v.count;

    for (size_t i = 0; i < fcount; i++)
        symtable_add_func(&funcs, func_list[i]->name,
                          func_list[i]->return_type,
                          func_list[i]->param_types,
                          func_list[i]->param_count);
    for (size_t i = 0; i < gcount; i++) {
        if (!check_global(glob_list[i], &globals, &ir)) {
            semantic_print_error("Semantic error");
            ok = 0;
        }
    }

    for (size_t i = 0; i < fcount && ok; i++) {
        if (!check_func(func_list[i], &funcs, &globals, &ir)) {
            semantic_print_error("Semantic error");
            ok = 0;
        }
    }

    /* Run optimizations on the generated IR */
    if (ok)
        opt_run(&ir, &opt_cfg);

    /* Generate output */
    if (ok && dump_ir) {
        char *text = ir_to_string(&ir);
        if (text) {
            printf("%s", text);
            free(text);
        }
    } else if (ok && dump_asm) {
        char *text = codegen_ir_to_string(&ir, use_x86_64);
        if (text) {
            printf("%s", text);
            free(text);
        }
    } else if (ok) {
        FILE *outf = fopen(output, "w");
        if (!outf) {
            perror("fopen");
            ok = 0;
        } else {
            codegen_emit_x86(outf, &ir, use_x86_64);
            fclose(outf);
        }
    }

    ir_builder_free(&ir);

    for (size_t i = 0; i < fcount; i++)
        ast_free_func(func_list[i]);
    for (size_t i = 0; i < gcount; i++)
        ast_free_stmt(glob_list[i]);
    free(func_list);
    free(glob_list);

    symtable_free(&funcs);
    symtable_free(&globals);
    lexer_free_tokens(tokens, tok_count);
    free(src_text);

    if (ok) {
        if (dump_ir)
            printf("Compiling %s (IR dumped to stdout)\n", source);
        else if (dump_asm)
            printf("Compiling %s (assembly dumped to stdout)\n", source);
        else
            printf("Compiling %s -> %s\n", source, output);
    }

    return ok ? 0 : 1;
}
