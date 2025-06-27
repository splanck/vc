#define _POSIX_C_SOURCE 200809L
/*
 * High level compilation and linking routines.
 *
 * This module contains helper functions used by main.c to run the
 * preprocessor, compile translation units and link executables.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "util.h"
#include "cli.h"
#include "token.h"
#include "parser.h"
#include "parser_core.h"
#include "ast_stmt.h"
#include "vector.h"
#include "strbuf.h"
#include "symtable.h"
#include "semantic.h"
#include "error.h"
#include "ir_core.h"
#include "ir_dump.h"
#include "opt.h"
#include "codegen.h"
#include "label.h"
#include "preproc.h"
#include "compile.h"

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
int compile_unit(const char *source, const cli_options_t *cli,
                 const char *output, int compile_obj);

/* Compile one source file into a temporary object file. */
static int compile_source_obj(const char *source, const cli_options_t *cli,
                              char **out_path);

/* Build and run the final linker command. */
static int run_link_command(const vector_t *objs, const char *output,
                            int use_x86_64);

/* Write the entry stub assembly to a temporary file. */
static int write_startup_asm(int use_x86_64, char **out_path);

/* Assemble the stub into an object file. */
static int assemble_startup_obj(const char *asm_path, int use_x86_64,
                               char **out_path);

/* Create an object file containing the entry stub for linking. */
static int create_startup_object(int use_x86_64, char **out_path);

/* Compile all input sources into temporary object files. */
static int compile_source_files(const cli_options_t *cli, vector_t *objs);

/* Build the startup stub and link all objects into the final executable. */
static int build_and_link_objects(vector_t *objs, const cli_options_t *cli);

/* Run only the preprocessor stage on each input source. */
int run_preprocessor(const cli_options_t *cli);

/* Link multiple object files into the final executable. */
int link_sources(const cli_options_t *cli);

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
int compile_unit(const char *source, const cli_options_t *cli,
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

/* Write the entry stub assembly to a temporary file. */
static int write_startup_asm(int use_x86_64, char **out_path)
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
    if (use_x86_64) {
        fputs(".globl _start\n_start:\n    call main\n    mov %rax, %rdi\n    mov $60, %rax\n    syscall\n", stub);
    } else {
        fputs(".globl _start\n_start:\n    call main\n    mov %eax, %ebx\n    mov $1, %eax\n    int $0x80\n", stub);
    }
    fclose(stub);

    *out_path = vc_strdup(asmname);
    return 1;
}

/* Assemble the entry stub into an object file. */
static int assemble_startup_obj(const char *asm_path, int use_x86_64,
                               char **out_path)
{
    char objname[] = "/tmp/vcobjXXXXXX";
    int objfd = mkstemp(objname);
    if (objfd < 0) {
        perror("mkstemp");
        return 0;
    }
    close(objfd);

    char cmd[512];
    const char *arch_flag = use_x86_64 ? "-m64" : "-m32";
    snprintf(cmd, sizeof(cmd), "cc -x assembler %s -c %s -o %s", arch_flag,
             asm_path, objname);
    int ret = system(cmd);
    if (ret != 0) {
        fprintf(stderr, "cc failed\n");
        unlink(objname);
        return 0;
    }

    *out_path = vc_strdup(objname);
    return 1;
}

/* Create object file with program entry point */
static int create_startup_object(int use_x86_64, char **out_path)
{
    char *asmfile = NULL;
    int ok = write_startup_asm(use_x86_64, &asmfile);
    if (ok)
        ok = assemble_startup_obj(asmfile, use_x86_64, out_path);
    if (asmfile) {
        unlink(asmfile);
        free(asmfile);
    }
    return ok;
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

/* Compile all sources to temporary object files. */
static int compile_source_files(const cli_options_t *cli, vector_t *objs)
{
    int ok = 1;
    vector_init(objs, sizeof(char *));

    for (size_t i = 0; i < cli->sources.count && ok; i++) {
        const char *src = ((const char **)cli->sources.data)[i];
        char *obj = NULL;
        ok = compile_source_obj(src, cli, &obj);
        if (ok) {
            if (!vector_push(objs, &obj)) {
                fprintf(stderr, "Out of memory\n");
                ok = 0;
                unlink(obj);
                free(obj);
            }
        }
    }

    return ok;
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

/* Create entry stub and link all objects into the final executable. */
static int build_and_link_objects(vector_t *objs, const cli_options_t *cli)
{
    char *stubobj = NULL;
    int ok = create_startup_object(cli->use_x86_64, &stubobj);
    if (ok) {
        if (!vector_push(objs, &stubobj)) {
            fprintf(stderr, "Out of memory\n");
            unlink(stubobj);
            free(stubobj);
            return 0;
        }
        ok = run_link_command(objs, cli->output, cli->use_x86_64);
    }
    return ok;
}

/* Run the preprocessor and print the result. */
int run_preprocessor(const cli_options_t *cli)
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
int link_sources(const cli_options_t *cli)
{
    vector_t objs;
    int ok = compile_source_files(cli, &objs);

    if (ok)
        ok = build_and_link_objects(&objs, cli);

    for (size_t i = 0; i < objs.count; i++) {
        unlink(((char **)objs.data)[i]);
        free(((char **)objs.data)[i]);
    }
    free(objs.data);

    return ok;
}
