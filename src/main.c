#define _POSIX_C_SOURCE 200809L
/*
 * Entry point of the vc compiler.
 *
 * This module orchestrates command line parsing and drives the
 * entire compilation pipeline:
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
#include "parser_core.h"
#include "ast_stmt.h"
#include "vector.h"
#include "strbuf.h"
#include <string.h>
#include "symtable.h"
#include "semantic.h"
#include "error.h"
#include "ir_core.h"
#include "ir_dump.h"
#include "opt.h"
#include "codegen.h"
#include "label.h"
#include "preproc.h"

/* Compilation stage helpers */
static int run_tokenize_stage(const char *source, const vector_t *incdirs,
                              char **out_src, token_t **out_toks,
                              size_t *out_count);
static int run_parse_stage(token_t *toks, size_t count,
                           vector_t *funcs_v, vector_t *globs_v,
                           symtable_t *funcs);
static int run_semantic_stage(func_t **func_list, size_t fcount,
                              stmt_t **glob_list, size_t gcount,
                              symtable_t *funcs, symtable_t *globals,
                              ir_builder_t *ir);
static int register_function_prototypes(func_t **func_list, size_t fcount,
                                        symtable_t *funcs);
static int check_global_decls(stmt_t **glob_list, size_t gcount,
                              symtable_t *globals, ir_builder_t *ir);
static int check_function_defs(func_t **func_list, size_t fcount,
                               symtable_t *funcs, symtable_t *globals,
                               ir_builder_t *ir);
static void run_optimize_stage(ir_builder_t *ir, const opt_config_t *cfg);
static int run_output_stage(ir_builder_t *ir, const char *output,
                            int dump_ir, int dump_asm, int use_x86_64,
                            int compile);
static int emit_output_file(ir_builder_t *ir, const char *output,
                            int use_x86_64, int compile_obj);
static void cleanup_compile_unit(vector_t *funcs_v, vector_t *globs_v,
                                 symtable_t *funcs, symtable_t *globals,
                                 ir_builder_t *ir);

/* Compile one translation unit to the given output path. */
static int compile_unit(const char *source, const cli_options_t *cli,
                        const char *output, int compile_obj);

/* Compile one source file into a temporary object file. */
static int compile_source_obj(const char *source, const cli_options_t *cli,
                              char **out_path);

/* Build and run the final linker command. */
static int run_link_command(const vector_t *objs, const char *output,
                            int use_x86_64);

/* Create an object file containing the entry stub for linking. */
static int create_startup_object(int use_x86_64, char **out_path);

/* Run only the preprocessor stage on each input source. */
static int run_preprocessor(const cli_options_t *cli);

/* Link multiple object files into the final executable. */
static int link_sources(const cli_options_t *cli);

/* Tokenize the preprocessed source file */
static int run_tokenize_stage(const char *source, const vector_t *incdirs,
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
static int run_parse_stage(token_t *toks, size_t count,
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

/* Register function prototypes and definitions in the symbol table */
static int register_function_prototypes(func_t **func_list, size_t fcount,
                                        symtable_t *funcs)
{
    for (size_t i = 0; i < fcount; i++) {
        symbol_t *existing = symtable_lookup(funcs, func_list[i]->name);
        if (existing) {
            int mismatch = existing->type != func_list[i]->return_type ||
                           existing->param_count != func_list[i]->param_count;
            for (size_t j = 0; j < existing->param_count && !mismatch; j++)
                if (existing->param_types[j] != func_list[i]->param_types[j])
                    mismatch = 1;
            if (mismatch) {
                error_set(0, 0);
                return 0;
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
    return 1;
}

/* Validate and emit IR for global declarations */
static int check_global_decls(stmt_t **glob_list, size_t gcount,
                              symtable_t *globals, ir_builder_t *ir)
{
    for (size_t i = 0; i < gcount; i++) {
        if (!check_global(glob_list[i], globals, ir)) {
            error_print("Semantic error");
            return 0;
        }
    }
    return 1;
}

/* Validate function definitions and build IR for each body */
static int check_function_defs(func_t **func_list, size_t fcount,
                               symtable_t *funcs, symtable_t *globals,
                               ir_builder_t *ir)
{
    for (size_t i = 0; i < fcount; i++) {
        if (!check_func(func_list[i], funcs, globals, ir)) {
            error_print("Semantic error");
            return 0;
        }
    }
    return 1;
}

/* Perform semantic analysis and IR generation */
static int run_semantic_stage(func_t **func_list, size_t fcount,
                              stmt_t **glob_list, size_t gcount,
                              symtable_t *funcs, symtable_t *globals,
                              ir_builder_t *ir)
{
    symtable_init(globals);
    ir_builder_init(ir);
    int ok = register_function_prototypes(func_list, fcount, funcs);
    if (ok)
        ok = check_global_decls(glob_list, gcount, globals, ir);
    if (ok)
        ok = check_function_defs(func_list, fcount, funcs, globals, ir);

    return ok;
}

/* Run IR optimizations */
static void run_optimize_stage(ir_builder_t *ir, const opt_config_t *cfg)
{
    if (cfg)
        opt_run(ir, cfg);
}

/* Emit the requested output */
static int run_output_stage(ir_builder_t *ir, const char *output,
                            int dump_ir, int dump_asm, int use_x86_64,
                            int compile)
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

    return emit_output_file(ir, output, use_x86_64, compile);
}

/* Emit assembly or an object file */
static int emit_output_file(ir_builder_t *ir, const char *output,
                            int use_x86_64, int compile_obj)
{
    if (compile_obj) {
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
        snprintf(cmd, sizeof(cmd), "cc -x assembler %s -c %s -o %s", arch_flag,
                 tmpname, output);
        int ret = system(cmd);
        unlink(tmpname);
        if (ret != 0) {
            fprintf(stderr, "cc failed\n");
            return 0;
        }
        return 1;
    }

    FILE *outf = fopen(output, "w");
    if (!outf) {
        perror("fopen");
        return 0;
    }
    codegen_emit_x86(outf, ir, use_x86_64);
    fclose(outf);
    return 1;
}

/* Free resources allocated during compilation */
static void cleanup_compile_unit(vector_t *funcs_v, vector_t *globs_v,
                                 symtable_t *funcs, symtable_t *globals,
                                 ir_builder_t *ir)
{
    for (size_t i = 0; i < funcs_v->count; i++)
        ast_free_func(((func_t **)funcs_v->data)[i]);
    for (size_t i = 0; i < globs_v->count; i++)
        ast_free_stmt(((stmt_t **)globs_v->data)[i]);
    free(funcs_v->data);
    free(globs_v->data);
    symtable_free(funcs);

    ir_builder_free(ir);
    symtable_free(globals);
}

/* Compile a single translation unit */
static int compile_unit(const char *source, const cli_options_t *cli,
                        const char *output, int compile_obj)
{
    label_init();
    codegen_set_export(cli->link);

    int ok = 1;

    /* Tokenization stage */
    char *src_text = NULL;
    token_t *tokens = NULL;
    size_t tok_count = 0;
    ok = run_tokenize_stage(source, &cli->include_dirs, &src_text,
                            &tokens, &tok_count);

    /* Parsing stage */
    vector_t func_list_v, glob_list_v;
    symtable_t funcs;
    if (ok)
        ok = run_parse_stage(tokens, tok_count, &func_list_v, &glob_list_v,
                             &funcs);
    lexer_free_tokens(tokens, tok_count);
    free(src_text);

    /* Semantic analysis */
    symtable_t globals;
    ir_builder_t ir;
    if (ok)
        ok = run_semantic_stage((func_t **)func_list_v.data, func_list_v.count,
                                (stmt_t **)glob_list_v.data, glob_list_v.count,
                                &funcs, &globals, &ir);

    /* Optimization and output */
    if (ok) {
        run_optimize_stage(&ir, &cli->opt_cfg);
        ok = run_output_stage(&ir, output, cli->dump_ir, cli->dump_asm,
                              cli->use_x86_64, compile_obj);
    }

    cleanup_compile_unit(&func_list_v, &glob_list_v, &funcs, &globals, &ir);

    label_reset();

    return ok;
}

/* Create object file with program entry point */
static int create_startup_object(int use_x86_64, char **out_path)
{
    char asmname[] = "/tmp/vcstubXXXXXX";
    int asmfd = mkstemp(asmname);
    if (asmfd < 0) {
        perror("mkstemp");
        return 0;
    }
    FILE *stub = fdopen(asmfd, "w");
    if (!stub) {
        perror("fdopen");
        close(asmfd);
        unlink(asmname);
        return 0;
    }
    if (use_x86_64)
        fputs(".globl _start\n_start:\n    call main\n    mov %rax, %rdi\n    mov $60, %rax\n    syscall\n", stub);
    else
        fputs(".globl _start\n_start:\n    call main\n    mov %eax, %ebx\n    mov $1, %eax\n    int $0x80\n", stub);
    fclose(stub);

    char objname[] = "/tmp/vcobjXXXXXX";
    int objfd = mkstemp(objname);
    if (objfd < 0) {
        perror("mkstemp");
        unlink(asmname);
        return 0;
    }
    close(objfd);
    char cmd[512];
    const char *arch_flag = use_x86_64 ? "-m64" : "-m32";
    snprintf(cmd, sizeof(cmd), "cc -x assembler %s -c %s -o %s", arch_flag,
             asmname, objname);
    int ret = system(cmd);
    unlink(asmname);
    if (ret != 0) {
        fprintf(stderr, "cc failed\n");
        unlink(objname);
        return 0;
    }
    *out_path = vc_strdup(objname);
    return 1;
}

/* Compile a single source file to a temporary object. */
static int compile_source_obj(const char *source, const cli_options_t *cli,
                              char **out_path)
{
    char objname[] = "/tmp/vcobjXXXXXX";
    int fd = mkstemp(objname);
    if (fd < 0) {
        perror("mkstemp");
        return 0;
    }
    close(fd);

    int ok = compile_unit(source, cli, objname, 1);
    if (!ok) {
        unlink(objname);
        return 0;
    }

    *out_path = vc_strdup(objname);
    return 1;
}

/* Construct and run the final cc link command. */
static int run_link_command(const vector_t *objs, const char *output,
                            int use_x86_64)
{
    strbuf_t cmd;
    strbuf_init(&cmd);
    const char *arch_flag = use_x86_64 ? "-m64" : "-m32";
    strbuf_appendf(&cmd, "cc %s", arch_flag);
    for (size_t i = 0; i < objs->count; i++)
        strbuf_appendf(&cmd, " %s", ((char **)objs->data)[i]);
    strbuf_appendf(&cmd, " -nostdlib -o %s", output);

    int ret = system(cmd.data);
    strbuf_free(&cmd);
    if (ret != 0) {
        fprintf(stderr, "cc failed\n");
        return 0;
    }
    return 1;
}

/* Run the preprocessor and print the result. */
static int run_preprocessor(const cli_options_t *cli)
{
    for (size_t i = 0; i < cli->sources.count; i++) {
        const char *src = ((const char **)cli->sources.data)[i];
        char *text = preproc_run(src, &cli->include_dirs);
        if (!text) {
            perror("preproc_run");
            return 1;
        }
        printf("%s", text);
        free(text);
    }
    return 0;
}

/* Compile all sources and link them into the final executable. */
static int link_sources(const cli_options_t *cli)
{
    int ok = 1;
    vector_t objs;
    vector_init(&objs, sizeof(char *));

    for (size_t i = 0; i < cli->sources.count && ok; i++) {
        const char *src = ((const char **)cli->sources.data)[i];
        char *obj = NULL;
        ok = compile_source_obj(src, cli, &obj);
        if (ok) {
            if (!vector_push(&objs, &obj)) {
                fprintf(stderr, "Out of memory\n");
                ok = 0;
                unlink(obj);
                free(obj);
            }
        }
    }

    char *stubobj = NULL;
    if (ok)
        ok = create_startup_object(cli->use_x86_64, &stubobj);
    if (ok) {
        vector_push(&objs, &stubobj);
        ok = run_link_command(&objs, cli->output, cli->use_x86_64);
    }

    for (size_t i = 0; i < objs.count; i++) {
        unlink(((char **)objs.data)[i]);
        free(((char **)objs.data)[i]);
    }
    free(objs.data);

    return ok;
}

/*
 * Program entry point. Parses command line options and coordinates the
 * preprocessing, compilation and linking stages. The preprocessor and
 * linker logic are delegated to helper functions.
 */
int main(int argc, char **argv)
{
    cli_options_t cli;
    if (cli_parse_args(argc, argv, &cli) != 0)
        return 1;

    if (!cli.link && cli.sources.count != 1) {
        fprintf(stderr, "Error: multiple input files require --link\n");
        return 1;
    }

    /* Only run the preprocessor when -E/--preprocess is supplied */
    if (cli.preprocess)
        return run_preprocessor(&cli);

    int ok = 1;

    if (cli.link) {
        ok = link_sources(&cli);
    } else {
        const char *src = ((const char **)cli.sources.data)[0];
        ok = compile_unit(src, &cli, cli.output, cli.compile);
    }

    if (ok) {
        if (cli.link)
            printf("Linking %zu files -> %s (executable)\n", cli.sources.count,
                   cli.output);
        else if (cli.dump_ir)
            printf("Compiling %s (IR dumped to stdout)\n",
                   ((const char **)cli.sources.data)[0]);
        else if (cli.dump_asm)
            printf("Compiling %s (assembly dumped to stdout)\n",
                   ((const char **)cli.sources.data)[0]);
        else if (cli.compile)
            printf("Compiling %s -> %s (object)\n",
                   ((const char **)cli.sources.data)[0], cli.output);
        else
            printf("Compiling %s -> %s\n",
                   ((const char **)cli.sources.data)[0], cli.output);
    }

    return ok ? 0 : 1;
}
