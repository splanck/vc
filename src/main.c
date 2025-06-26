#define _POSIX_C_SOURCE 200809L
/*
 * Entry point of the vc compiler.
 *
 * This file drives the entire compilation pipeline:
 *  1. Source code is tokenized by the lexer.
 *  2. The parser builds an AST from those tokens.
 *  3. Semantic analysis creates an intermediate representation (IR).
 *  4. Optimization passes run on the IR.
 *  5. Code generation emits x86 assembly or an object file.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "util.h"
#include "cli.h"
#include "token.h"
#include "parser.h"
#include "vector.h"
#include "symtable.h"
#include "semantic.h"
#include "error.h"
#include "ir.h"
#include "ir_dump.h"
#include "opt.h"
#include "codegen.h"
#include "label.h"
#include "preproc.h"

/* Compilation stage helpers */
static int tokenize_stage(const char *source, const vector_t *incdirs,
                          char **out_src, token_t **out_toks,
                          size_t *out_count);
static int parse_stage(token_t *toks, size_t count,
                       vector_t *funcs_v, vector_t *globs_v,
                       symtable_t *funcs);
static int semantic_stage(func_t **func_list, size_t fcount,
                          stmt_t **glob_list, size_t gcount,
                          symtable_t *funcs, symtable_t *globals,
                          ir_builder_t *ir);
static void optimize_stage(ir_builder_t *ir, const opt_config_t *cfg);
static int output_stage(ir_builder_t *ir, const char *output,
                        int dump_ir, int dump_asm, int use_x86_64, int compile);

/* Tokenize the preprocessed source file */
static int tokenize_stage(const char *source, const vector_t *incdirs,
                          char **out_src, token_t **out_toks,
                          size_t *out_count)
{
    char *text = preproc_run(source, incdirs);
    if (!text) {
        perror("preproc_run");
        return 0;
    }
    size_t count = 0;
    token_t *toks = lexer_tokenize(text, &count);
    if (!toks) {
        fprintf(stderr, "Tokenization failed\n");
        free(text);
        return 0;
    }
    *out_src = text;
    *out_toks = toks;
    if (out_count)
        *out_count = count;
    return 1;
}

/* Parse tokens into AST lists */
static int parse_stage(token_t *toks, size_t count,
                       vector_t *funcs_v, vector_t *globs_v,
                       symtable_t *funcs)
{
    parser_t parser;
    parser_init(&parser, toks, count);
    symtable_init(funcs);
    vector_init(funcs_v, sizeof(func_t *));
    vector_init(globs_v, sizeof(stmt_t *));

    int ok = 1;
    while (ok && !parser_is_eof(&parser)) {
        func_t *fn = NULL;
        stmt_t *g = NULL;
        if (!parser_parse_toplevel(&parser, funcs, &fn, &g)) {
            token_type_t expected[] = { TOK_KW_INT, TOK_KW_VOID };
            parser_print_error(&parser, expected, 2);
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
    return ok;
}

/* Perform semantic analysis and IR generation */
static int semantic_stage(func_t **func_list, size_t fcount,
                          stmt_t **glob_list, size_t gcount,
                          symtable_t *funcs, symtable_t *globals,
                          ir_builder_t *ir)
{
    symtable_init(globals);
    ir_builder_init(ir);
    int ok = 1;

    for (size_t i = 0; i < fcount; i++) {
        symbol_t *existing = symtable_lookup(funcs, func_list[i]->name);
        if (existing) {
            int mismatch = existing->type != func_list[i]->return_type ||
                           existing->param_count != func_list[i]->param_count;
            for (size_t j = 0; j < existing->param_count && !mismatch; j++)
                if (existing->param_types[j] != func_list[i]->param_types[j])
                    mismatch = 1;
            if (mismatch) {
                ok = 0;
                error_set(0, 0);
                break;
            }
            existing->is_prototype = 0;
        } else {
            symtable_add_func(funcs, func_list[i]->name,
                              func_list[i]->return_type,
                              func_list[i]->param_types,
                              func_list[i]->param_count,
                              0);
        }
    }

    for (size_t i = 0; i < gcount && ok; i++) {
        if (!check_global(glob_list[i], globals, ir)) {
            error_print("Semantic error");
            ok = 0;
        }
    }

    for (size_t i = 0; i < fcount && ok; i++) {
        if (!check_func(func_list[i], funcs, globals, ir)) {
            error_print("Semantic error");
            ok = 0;
        }
    }

    return ok;
}

/* Run IR optimizations */
static void optimize_stage(ir_builder_t *ir, const opt_config_t *cfg)
{
    if (cfg)
        opt_run(ir, cfg);
}

/* Emit the requested output */
static int output_stage(ir_builder_t *ir, const char *output,
                        int dump_ir, int dump_asm, int use_x86_64, int compile)
{
    if (dump_ir) {
        char *text = ir_to_string(ir);
        if (text) {
            printf("%s", text);
            free(text);
        }
        return 1;
    }
    if (dump_asm) {
        char *text = codegen_ir_to_string(ir, use_x86_64);
        if (text) {
            printf("%s", text);
            free(text);
        }
        return 1;
    }

    if (compile) {
        char tmpname[] = "/tmp/vcXXXXXX";
        int fd = mkstemp(tmpname);
        if (fd < 0) {
            perror("mkstemp");
            return 0;
        }
        FILE *tmpf = fdopen(fd, "w");
        if (!tmpf) {
            perror("fdopen");
            close(fd);
            unlink(tmpname);
            return 0;
        }
        codegen_emit_x86(tmpf, ir, use_x86_64);
        fclose(tmpf);

        char cmd[512];
        const char *arch_flag = use_x86_64 ? "-m64" : "-m32";
        snprintf(cmd, sizeof(cmd), "cc -x assembler %s -c %s -o %s", arch_flag, tmpname, output);
        int ret = system(cmd);
        unlink(tmpname);
        if (ret != 0) {
            fprintf(stderr, "cc failed\n");
            return 0;
        }
        return 1;
    } else {
        FILE *outf = fopen(output, "w");
        if (!outf) {
            perror("fopen");
            return 0;
        }
        codegen_emit_x86(outf, ir, use_x86_64);
        fclose(outf);
        return 1;
    }
}

int main(int argc, char **argv)
{
    cli_options_t cli;
    if (cli_parse_args(argc, argv, &cli) != 0)
        return 1;

    const char *source = cli.source;
    char *output = cli.output;
    opt_config_t opt_cfg = cli.opt_cfg;
    int use_x86_64 = cli.use_x86_64;
    c_std_t std = cli.std;
    int dump_asm = cli.dump_asm;
    int dump_ir = cli.dump_ir;
    int link = cli.link;

    if (cli.preprocess) {
        char *text = preproc_run(source, &cli.include_dirs);
        if (!text) {
            perror("preproc_run");
            return 1;
        }
        printf("%s", text);
        free(text);
        return 0;
    }

    label_init();

    char *src_text = NULL;
    token_t *tokens = NULL;
    size_t tok_count = 0;
    vector_t func_list_v, glob_list_v;
    symtable_t funcs, globals;
    ir_builder_t ir;

    int ok = tokenize_stage(source, &cli.include_dirs, &src_text,
                            &tokens, &tok_count);
    if (ok)
        ok = parse_stage(tokens, tok_count, &func_list_v, &glob_list_v, &funcs);
    if (ok)
        ok = semantic_stage((func_t **)func_list_v.data, func_list_v.count,
                            (stmt_t **)glob_list_v.data, glob_list_v.count,
                            &funcs, &globals, &ir);
    if (ok)
        optimize_stage(&ir, &opt_cfg);
    if (ok) {
        if (link) {
            char asmname[] = "/tmp/vcXXXXXX";
            int asmfd = mkstemp(asmname);
            if (asmfd < 0) {
                perror("mkstemp");
                ok = 0;
            } else {
                close(asmfd);
                ok = output_stage(&ir, asmname, dump_ir, dump_asm, use_x86_64, 0);
                if (ok) {
                    FILE *stub = fopen(asmname, "a");
                    if (!stub) {
                        perror("fopen");
                        ok = 0;
                    } else {
                        if (use_x86_64) {
                            fputs(".globl _start\n_start:\n    call main\n    mov %rax, %rdi\n    mov $60, %rax\n    syscall\n", stub);
                        } else {
                            fputs(".globl _start\n_start:\n    call main\n    mov %eax, %ebx\n    mov $1, %eax\n    int $0x80\n", stub);
                        }
                        fclose(stub);
                        char objtmp[] = "/tmp/vcobjXXXXXX";
                        int objfd = mkstemp(objtmp);
                        if (objfd < 0) {
                            perror("mkstemp");
                            ok = 0;
                        } else {
                            close(objfd);
                            char cmd[512];
                            const char *arch_flag = use_x86_64 ? "-m64" : "-m32";
                            snprintf(cmd, sizeof(cmd), "cc -x assembler %s -c %s -o %s", arch_flag, asmname, objtmp);
                            int ret = system(cmd);
                            if (ret == 0) {
                                snprintf(cmd, sizeof(cmd), "cc %s %s -nostdlib -o %s", arch_flag, objtmp, output);
                                ret = system(cmd);
                            }
                            if (ret != 0) {
                                fprintf(stderr, "cc failed\n");
                                ok = 0;
                            }
                            unlink(objtmp);
                        }
                    }
                }
                unlink(asmname);
            }
        } else {
            ok = output_stage(&ir, output, dump_ir, dump_asm, use_x86_64, cli.compile);
        }
    }

    for (size_t i = 0; i < func_list_v.count; i++)
        ast_free_func(((func_t **)func_list_v.data)[i]);
    for (size_t i = 0; i < glob_list_v.count; i++)
        ast_free_stmt(((stmt_t **)glob_list_v.data)[i]);
    free(func_list_v.data);
    free(glob_list_v.data);

    ir_builder_free(&ir);
    symtable_free(&funcs);
    symtable_free(&globals);
    lexer_free_tokens(tokens, tok_count);
    free(src_text);

    label_reset();

    if (ok) {
        if (dump_ir)
            printf("Compiling %s (IR dumped to stdout)\n", source);
        else if (dump_asm)
            printf("Compiling %s (assembly dumped to stdout)\n", source);
        else if (link)
            printf("Compiling %s -> %s (executable)\n", source, output);
        else if (cli.compile)
            printf("Compiling %s -> %s (object)\n", source, output);
        else
            printf("Compiling %s -> %s\n", source, output);
    }

    return ok ? 0 : 1;
}
