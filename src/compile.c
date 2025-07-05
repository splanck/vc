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
#include <limits.h>
#include <stdint.h>
#ifndef PATH_MAX
# include <sys/param.h>
#endif
#ifndef PATH_MAX
# define PATH_MAX 4096
#endif
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <spawn.h>
#include <sys/wait.h>

#include "util.h"
#include "cli.h"
#include "token.h"
#include "parser.h"
#include "parser_core.h"
#include "ast_stmt.h"
#include "vector.h"
#include "symtable.h"
#include "semantic.h"
#include "error.h"
#include "ir_core.h"
#include "ir_dump.h"
#include "opt.h"
#include "codegen.h"
#include "label.h"
#include "preproc.h"
#include "command.h"
#include "compile.h"
#include "startup.h"

/* Use binary mode for temporary files on platforms that require it */
#if defined(_WIN32)
# define TEMP_FOPEN_MODE "wb"
#else
# define TEMP_FOPEN_MODE "w"
#endif

/* Active diagnostic context */
extern const char *error_current_file;
extern const char *error_current_function;
extern char **environ;

typedef struct compile_context {
    char       *src_text;
    token_t    *tokens;
    size_t      tok_count;
    char       *stdin_tmp;
    vector_t    func_list_v;
    vector_t    glob_list_v;
    symtable_t  funcs;
    symtable_t  globals;
    ir_builder_t ir;
} compile_context_t;

/* Stage implementations */
static int compile_tokenize_impl(const char *source, const vector_t *incdirs,
                                 const vector_t *defines,
                                 const vector_t *undefines,
                                 char **out_src, token_t **out_toks,
                                 size_t *out_count, char **tmp_path);
static int compile_parse_impl(token_t *toks, size_t count,
                              vector_t *funcs_v, vector_t *globs_v,
                              symtable_t *funcs);
static int compile_semantic_impl(func_t **func_list, size_t fcount,
                                 stmt_t **glob_list, size_t gcount,
                                 symtable_t *funcs, symtable_t *globals,
                                 ir_builder_t *ir);
static int compile_optimize_impl(ir_builder_t *ir, const opt_config_t *cfg);
static int compile_output_impl(ir_builder_t *ir, const char *output,
                               int dump_ir, int dump_asm, int use_x86_64,
                               int compile, const cli_options_t *cli);

/* Stage helpers */
static void compile_ctx_init(compile_context_t *ctx);
static void compile_ctx_cleanup(compile_context_t *ctx);
static int compile_tokenize_stage(compile_context_t *ctx, const char *source,
                                  const vector_t *incdirs,
                                  const vector_t *defines,
                                  const vector_t *undefines);
static int compile_parse_stage(compile_context_t *ctx);
static int compile_semantic_stage(compile_context_t *ctx);
static int compile_optimize_stage(compile_context_t *ctx,
                                  const opt_config_t *cfg);
static int compile_output_stage(compile_context_t *ctx, const char *output,
                                int dump_ir, int dump_asm, int use_x86_64,
                                int compile_obj, const cli_options_t *cli);
static int register_function_prototypes(func_t **func_list, size_t fcount,
                                        symtable_t *funcs);
static int check_global_decls(stmt_t **glob_list, size_t gcount,
                              symtable_t *globals, ir_builder_t *ir);
static int check_function_defs(func_t **func_list, size_t fcount,
                               symtable_t *funcs, symtable_t *globals,
                               ir_builder_t *ir);
static int emit_output_file(ir_builder_t *ir, const char *output,
                            int use_x86_64, int compile_obj,
                            const cli_options_t *cli);
static int read_stdin_source(const vector_t *incdirs, const vector_t *defines,
                             const vector_t *undefines,
                             char **out_path, char **out_text);

/* Compile one translation unit to the given output path. */
int compile_unit(const char *source, const cli_options_t *cli,
                 const char *output, int compile_obj);

/* Compile one source file into a temporary object file. */
#ifdef UNIT_TESTING
int compile_source_obj(const char *source, const cli_options_t *cli,
                       char **out_path);
#else
static int compile_source_obj(const char *source, const cli_options_t *cli,
                              char **out_path);
#endif

/* Build and run the final linker command. */
static int run_link_command(const vector_t *objs, const vector_t *lib_dirs,
                            const vector_t *libs, const char *output,
                            int use_x86_64);

/* Allocate and populate argv array for the linker. */
static char **build_linker_args(const vector_t *objs,
                                const vector_t *lib_dirs,
                                const vector_t *libs,
                                const char *output,
                                int use_x86_64);

/* Free argv array returned by build_linker_args. */
static void free_linker_args(char **argv);

/* Spawn a command and wait for completion */

static const char nasm_macros[] =
    "%macro movl 2\n    mov %1, %2\n%endmacro\n"
    "%macro movq 2\n    mov %1, %2\n%endmacro\n"
    "%macro addl 2\n    add %1, %2\n%endmacro\n"
    "%macro addq 2\n    add %1, %2\n%endmacro\n"
    "%macro subl 2\n    sub %1, %2\n%endmacro\n"
    "%macro subq 2\n    sub %1, %2\n%endmacro\n"
    "%macro imull 2\n    imul %1, %2\n%endmacro\n"
    "%macro imulq 2\n    imul %1, %2\n%endmacro\n"
    "%macro cmpl 2\n    cmp %1, %2\n%endmacro\n"
    "%macro cmpq 2\n    cmp %1, %2\n%endmacro\n"
    "%macro pushl 1\n    push %1\n%endmacro\n"
    "%macro pushq 1\n    push %1\n%endmacro\n"
    "%macro popl 1\n    pop %1\n%endmacro\n"
    "%macro popq 1\n    pop %1\n%endmacro\n";

/* Create a temporary file and return its descriptor. */
int create_temp_file(const cli_options_t *cli, const char *prefix,
                     char **out_path);

/* Assemble the template path for a temporary file. */
static char *create_temp_template(const cli_options_t *cli,
                                  const char *prefix);

/* Wrapper around mkstemp that sets FD_CLOEXEC on the returned descriptor. */
static int open_temp_file(char *tmpl);

/* Create an object file containing the entry stub for linking. */
static int create_startup_object(const cli_options_t *cli, int use_x86_64,
                                 char **out_path);

/* Compile all input sources into temporary object files. */
static int compile_source_files(const cli_options_t *cli, vector_t *objs);

/* Build the startup stub and link all objects into the final executable. */
static int build_and_link_objects(vector_t *objs, const cli_options_t *cli);

/* Run only the preprocessor stage on each input source. */
int run_preprocessor(const cli_options_t *cli);

/* Link multiple object files into the final executable. */
int link_sources(const cli_options_t *cli);

/* Read source from stdin into a temporary file and run the preprocessor */
static int read_stdin_source(const vector_t *incdirs, const vector_t *defines,
                             const vector_t *undefines,
                             char **out_path, char **out_text)
{
    char tmpl[] = "/tmp/vcstdinXXXXXX";
    int fd = mkstemp(tmpl);
    if (fd < 0) {
        perror("mkstemp");
        return 0;
    }
    FILE *f = fdopen(fd, TEMP_FOPEN_MODE);
    if (!f) {
        perror("fdopen");
        close(fd);
        unlink(tmpl);
        return 0;
    }

    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), stdin)) > 0) {
        if (fwrite(buf, 1, n, f) != n) {
            perror("fwrite");
            if (fclose(f) == EOF)
                perror("fclose");
            unlink(tmpl);
            return 0;
        }
    }
    if (ferror(stdin)) {
        perror("fread");
        if (fclose(f) == EOF)
            perror("fclose");
        unlink(tmpl);
        return 0;
    }
    if (fclose(f) == EOF) {
        perror("fclose");
        unlink(tmpl);
        return 0;
    }

    char *path = vc_strdup(tmpl);
    if (!path) {
        fprintf(stderr, "Out of memory\n");
        unlink(tmpl);
        return 0;
    }

    char *text = preproc_run(path, incdirs, defines, undefines);
    if (!text) {
        perror("preproc_run");
        unlink(path);
        free(path);
        return 0;
    }

    *out_path = path;
    *out_text = text;
    return 1;
}

/* Tokenize the preprocessed source file */
static int compile_tokenize_impl(const char *source, const vector_t *incdirs,
                            const vector_t *defines,
                            const vector_t *undefines,
                            char **out_src, token_t **out_toks,
                            size_t *out_count, char **tmp_path)
{
    if (tmp_path)
        *tmp_path = NULL;

    char *stdin_path = NULL;
    char *text = NULL;
    if (source && strcmp(source, "-") == 0) {
        if (!read_stdin_source(incdirs, defines, undefines, &stdin_path, &text))
            return 0;
        if (tmp_path)
            *tmp_path = stdin_path;
        else {
            unlink(stdin_path);
            free(stdin_path);
            stdin_path = NULL;
        }
    } else {
        text = preproc_run(source, incdirs, defines, undefines);
        if (!text) {
            perror("preproc_run");
            return 0;
        }
    }

    size_t count = 0;
    token_t *toks = lexer_tokenize(text, &count);
    if (!toks) {
        fprintf(stderr, "Tokenization failed\n");
        free(text);
        if (stdin_path) {
            unlink(stdin_path);
            free(stdin_path);
        }
        return 0;
    }
    *out_src = text;
    *out_toks = toks;
    if (out_count)
        *out_count = count;
    return 1;
}

/* Parse tokens into AST lists */
static int compile_parse_impl(token_t *toks, size_t count,
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

/* Register function prototypes and definitions in the symbol table */
static int register_function_prototypes(func_t **func_list, size_t fcount,
                                        symtable_t *funcs)
{
    for (size_t i = 0; i < fcount; i++) {
        symbol_t *existing = symtable_lookup(funcs, func_list[i]->name);
        if (existing) {
            int mismatch = existing->type != func_list[i]->return_type ||
                           existing->param_count != func_list[i]->param_count ||
                           existing->is_variadic != func_list[i]->is_variadic;
            for (size_t j = 0; j < existing->param_count && !mismatch; j++)
                if (existing->param_types[j] != func_list[i]->param_types[j])
                    mismatch = 1;
            if (mismatch) {
                error_set(0, 0, error_current_file, error_current_function);
                error_print("Semantic error");
                return 0;
            }
            existing->is_prototype = 0;
            if (func_list[i]->is_inline)
                existing->is_inline = 1;
        } else {
            size_t rsz = (func_list[i]->return_type == TYPE_STRUCT ||
                          func_list[i]->return_type == TYPE_UNION) ? 4 : 0;
            symtable_add_func(funcs, func_list[i]->name,
                              func_list[i]->return_type,
                              rsz,
                              func_list[i]->param_elem_sizes,
                              func_list[i]->param_types,
                              func_list[i]->param_count,
                              func_list[i]->is_variadic,
                              0,
                              func_list[i]->is_inline);
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
static int compile_semantic_impl(func_t **func_list, size_t fcount,
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
static int compile_optimize_impl(ir_builder_t *ir, const opt_config_t *cfg)
{
    if (cfg)
        opt_run(ir, cfg);
    return 1;
}

/* Emit the requested output */
static int compile_output_impl(ir_builder_t *ir, const char *output,
                          int dump_ir, int dump_asm, int use_x86_64,
                          int compile, const cli_options_t *cli)
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
        char *text = codegen_ir_to_string(ir, use_x86_64,
                                          cli->asm_syntax);
        if (text) {
            printf("%s", text);
            free(text);
        }
        return 1;
    }

    return emit_output_file(ir, output, use_x86_64, compile, cli);
}

/* Initialize compilation context */
static void compile_ctx_init(compile_context_t *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    vector_init(&ctx->func_list_v, sizeof(func_t *));
    vector_init(&ctx->glob_list_v, sizeof(stmt_t *));
    symtable_init(&ctx->funcs);
    symtable_init(&ctx->globals);
    ir_builder_init(&ctx->ir);
}

/* Free resources allocated during compilation */
static void compile_ctx_cleanup(compile_context_t *ctx)
{
    for (size_t i = 0; i < ctx->func_list_v.count; i++)
        ast_free_func(((func_t **)ctx->func_list_v.data)[i]);
    for (size_t i = 0; i < ctx->glob_list_v.count; i++)
        ast_free_stmt(((stmt_t **)ctx->glob_list_v.data)[i]);
    vector_free(&ctx->func_list_v);
    vector_free(&ctx->glob_list_v);
    symtable_free(&ctx->funcs);
    ir_builder_free(&ctx->ir);
    symtable_free(&ctx->globals);

    lexer_free_tokens(ctx->tokens, ctx->tok_count);
    free(ctx->src_text);
}

/* Run tokenization stage */
static int compile_tokenize_stage(compile_context_t *ctx, const char *source,
                                  const vector_t *incdirs,
                                  const vector_t *defines,
                                  const vector_t *undefines)
{
    return compile_tokenize_impl(source, incdirs, defines, undefines,
                                 &ctx->src_text,
                                 &ctx->tokens, &ctx->tok_count,
                                 &ctx->stdin_tmp);
}

/* Run parsing stage */
static int compile_parse_stage(compile_context_t *ctx)
{
    int ok = compile_parse_impl(ctx->tokens, ctx->tok_count,
                                &ctx->func_list_v, &ctx->glob_list_v,
                                &ctx->funcs);
    lexer_free_tokens(ctx->tokens, ctx->tok_count);
    free(ctx->src_text);
    ctx->tokens = NULL;
    ctx->src_text = NULL;
    ctx->tok_count = 0;
    return ok;
}

/* Run semantic analysis stage */
static int compile_semantic_stage(compile_context_t *ctx)
{
    return compile_semantic_impl((func_t **)ctx->func_list_v.data,
                                 ctx->func_list_v.count,
                                 (stmt_t **)ctx->glob_list_v.data,
                                 ctx->glob_list_v.count,
                                 &ctx->funcs, &ctx->globals,
                                 &ctx->ir);
}

/* Run optimization stage */
static int compile_optimize_stage(compile_context_t *ctx,
                                  const opt_config_t *cfg)
{
    return compile_optimize_impl(&ctx->ir, cfg);
}

/* Run output stage */
static int compile_output_stage(compile_context_t *ctx, const char *output,
                                int dump_ir, int dump_asm, int use_x86_64,
                                int compile_obj, const cli_options_t *cli)
{
    return compile_output_impl(&ctx->ir, output, dump_ir, dump_asm,
                               use_x86_64, compile_obj, cli);
}

/* Emit assembly or an object file */
static int emit_output_file(ir_builder_t *ir, const char *output,
                            int use_x86_64, int compile_obj,
                            const cli_options_t *cli)
{
    if (compile_obj) {
        char *tmpname = NULL;
        int fd = create_temp_file(cli, "vc", &tmpname);
        if (fd < 0) {
            return 0;
        }
        FILE *tmpf = fdopen(fd, TEMP_FOPEN_MODE);
        if (!tmpf) {
            perror("fdopen");
            close(fd);
            unlink(tmpname);
            free(tmpname);
            return 0;
        }
        if (cli->asm_syntax == ASM_INTEL)
            fputs(nasm_macros, tmpf);
        codegen_emit_x86(tmpf, ir, use_x86_64,
                        cli->asm_syntax);
        if (fflush(tmpf) == EOF) {
            perror("fflush");
            fclose(tmpf);
            unlink(tmpname);
            free(tmpname);
            return 0;
        }
        if (fclose(tmpf) == EOF) {
            perror("fclose");
            unlink(tmpname);
            free(tmpname);
            return 0;
        }

        int rc;
        if (cli->asm_syntax == ASM_INTEL) {
            const char *fmt = use_x86_64 ? "elf64" : "elf32";
            char *argv[] = {"nasm", "-f", (char *)fmt, tmpname, "-o",
                            (char *)output, NULL};
            rc = command_run(argv);
        } else {
            const char *arch_flag = use_x86_64 ? "-m64" : "-m32";
            char *argv[] = {"cc", "-x", "assembler", (char *)arch_flag, "-c",
                            tmpname, "-o", (char *)output, NULL};
            rc = command_run(argv);
        }
        unlink(tmpname);
        free(tmpname);
        if (rc != 1) {
            if (rc == 0) {
                if (cli->asm_syntax == ASM_INTEL)
                    fprintf(stderr, "assembly failed\n");
                else
                    fprintf(stderr, "cc failed\n");
            } else if (rc == -1) {
                if (cli->asm_syntax == ASM_INTEL)
                    fprintf(stderr, "nasm terminated by signal\n");
                else
                    fprintf(stderr, "cc terminated by signal\n");
            }
            return 0;
        }
        return 1;
    }

    FILE *outf = fopen(output, "wb");
    if (!outf) {
        perror("fopen");
        return 0;
    }
    codegen_emit_x86(outf, ir, use_x86_64,
                    cli->asm_syntax);
    if (fclose(outf) == EOF) {
        perror("fclose");
        unlink(output);
        return 0;
    }
    return 1;
}

/* Compile a single translation unit */
#ifndef UNIT_TESTING
int compile_unit(const char *source, const cli_options_t *cli,
                 const char *output, int compile_obj)
{
    error_current_file = source ? source : "";
    error_current_function = NULL;
    label_init();
    codegen_set_export(cli->link);
    codegen_set_debug(cli->debug);

    int ok = 1;
    compile_context_t ctx;
    compile_ctx_init(&ctx);

    ok = compile_tokenize_stage(&ctx, source, &cli->include_dirs,
                                &cli->defines, &cli->undefines);
    if (ok)
        ok = compile_parse_stage(&ctx);
    if (ok)
        ok = compile_semantic_stage(&ctx);
    if (ok) {
        ok = compile_optimize_stage(&ctx, &cli->opt_cfg);
        if (ok)
            ok = compile_output_stage(&ctx, output, cli->dump_ir,
                                     cli->dump_asm, cli->use_x86_64,
                                     compile_obj, cli);
    }

    compile_ctx_cleanup(&ctx);
    semantic_global_cleanup();

    if (ctx.stdin_tmp) {
        unlink(ctx.stdin_tmp);
        free(ctx.stdin_tmp);
    }

    label_reset();

    return ok;
}
#endif /* !UNIT_TESTING */

/*
 * Assemble mkstemp template path using cli->obj_dir (or /tmp) and the
 * given prefix.  Returns a newly allocated string or NULL on error.
 *
 * errno will be ENAMETOOLONG if the resulting path would exceed PATH_MAX
 * or snprintf detected truncation.
 */
static char *
create_temp_template(const cli_options_t *cli, const char *prefix)
{
    const char *dir = cli->obj_dir ? cli->obj_dir : "/tmp";
    size_t len = strlen(dir) + strlen(prefix) + sizeof("/XXXXXX");
    if (len >= PATH_MAX) {
        errno = ENAMETOOLONG;
        return NULL;
    }
    char *tmpl = malloc(len + 1);
    if (!tmpl)
        return NULL;

    errno = 0;
    int n = snprintf(tmpl, len + 1, "%s/%sXXXXXX", dir, prefix);
    int err = errno;
    if (n < 0) {
        free(tmpl);
        errno = err;
        return NULL;
    }
    if ((size_t)n >= len + 1) {
        free(tmpl);
        errno = ENAMETOOLONG;
        return NULL;
    }

    return tmpl;
}

/*
 * Create and open the temporary file described by tmpl.  Returns the file
 * descriptor on success or -1 on failure.  On error the file is unlinked
 * and errno is preserved.
 */
static int
open_temp_file(char *tmpl)
{
    int fd = mkstemp(tmpl);
    if (fd < 0)
        return -1;
    if (fcntl(fd, F_SETFD, FD_CLOEXEC) != 0) {
        int err = errno;
        close(fd);
        unlink(tmpl);
        errno = err;
        return -1;
    }
    return fd;
}

/*
 * Create a temporary file and return its descriptor.  On success the path
 * is stored in *out_path.  On failure -1 is returned, *out_path is set to
 * NULL and errno indicates the error:
 *   ENAMETOOLONG - path would exceed PATH_MAX or snprintf truncated
 *   others       - from malloc, mkstemp or fcntl
 */
int create_temp_file(const cli_options_t *cli, const char *prefix,
                     char **out_path)
{
    char *tmpl = create_temp_template(cli, prefix);
    if (!tmpl) {
        *out_path = NULL;
        return -1;
    }

    int fd = open_temp_file(tmpl);
    if (fd < 0) {
        free(tmpl);
        *out_path = NULL;
        return -1;
    }

    *out_path = tmpl;
    return fd;
}



/* Create object file with program entry point */
static int create_startup_object(const cli_options_t *cli, int use_x86_64,
                                char **out_path)
{
    char *asmfile = NULL;
    int ok = write_startup_asm(use_x86_64, cli->asm_syntax, cli, &asmfile);
    if (ok)
        ok = assemble_startup_obj(asmfile, use_x86_64, cli, out_path);
    if (asmfile) {
        unlink(asmfile);
        free(asmfile);
    }
    return ok;
}

/* Compile a single source file to a temporary object. */
#ifdef UNIT_TESTING
int compile_source_obj(const char *source, const cli_options_t *cli,
                       char **out_path)
#else
static int compile_source_obj(const char *source, const cli_options_t *cli,
                              char **out_path)
#endif
{
    char *objname = NULL;
    int fd = create_temp_file(cli, "vcobj", &objname);
    if (fd < 0) {
        perror("mkstemp");
        return 0;
    }
    close(fd);

    int ok = compile_unit(source, cli, objname, 1);
    if (!ok) {
        unlink(objname);
        free(objname);
        return 0;
    }

    *out_path = objname;
    return 1;
}

/* Compile all sources to temporary object files. */
static int compile_source_files(const cli_options_t *cli, vector_t *objs)
{
    int ok = 1;
    vector_init(objs, sizeof(char *));

    for (size_t i = 0; i < cli->sources.count; i++) {
        const char *src = ((const char **)cli->sources.data)[i];
        char *obj = NULL;

        if (!compile_source_obj(src, cli, &obj)) {
            ok = 0;
            break;
        }

        if (!vector_push(objs, &obj)) {
            fprintf(stderr, "Out of memory\n");
            ok = 0;
            unlink(obj);
            free(obj);
            break;
        }
    }

    if (!ok) {
        for (size_t j = 0; j < objs->count; j++) {
            unlink(((char **)objs->data)[j]);
            free(((char **)objs->data)[j]);
        }
        objs->count = 0;
    }

    return ok;
}

/* Allocate and populate the argument vector for the linker command. */
static char **
build_linker_args(const vector_t *objs, const vector_t *lib_dirs,
                  const vector_t *libs, const char *output, int use_x86_64)
{
    const char *arch_flag = use_x86_64 ? "-m64" : "-m32";

    /* calculate required argument count and detect overflow */
    size_t argc = 0;
    if (objs->count > SIZE_MAX - argc)
        goto arg_overflow;
    argc += objs->count;
    if (lib_dirs->count > (SIZE_MAX - argc) / 2)
        goto arg_overflow;
    argc += lib_dirs->count * 2;
    if (libs->count > (SIZE_MAX - argc) / 2)
        goto arg_overflow;
    argc += libs->count * 2;
    if (5 > SIZE_MAX - argc)
        goto arg_overflow;
    argc += 5;

    size_t n = argc + 1; /* plus NULL terminator */
    if (n > SIZE_MAX / sizeof(char *))
        goto arg_overflow;

    char **argv = vc_alloc_or_exit(n * sizeof(char *));

    size_t idx = 0;
    argv[idx++] = "cc";
    argv[idx++] = (char *)arch_flag;
    for (size_t i = 0; i < objs->count; i++)
        argv[idx++] = ((char **)objs->data)[i];
    for (size_t i = 0; i < lib_dirs->count; i++) {
        argv[idx++] = "-L";
        argv[idx++] = ((char **)lib_dirs->data)[i];
    }
    argv[idx++] = "-nostdlib";
    for (size_t i = 0; i < libs->count; i++) {
        argv[idx++] = "-l";
        argv[idx++] = ((char **)libs->data)[i];
    }
    argv[idx++] = "-o";
    argv[idx++] = (char *)output;
    argv[idx] = NULL;
    return argv;

arg_overflow:
    fprintf(stderr, "vc: argument vector too large\n");
    exit(1);
}

/* Free argument vector returned by build_linker_args. */
static void free_linker_args(char **argv)
{
    free(argv);
}

/* Construct and run the final cc link command. */
static int run_link_command(const vector_t *objs, const vector_t *lib_dirs,
                            const vector_t *libs, const char *output,
                            int use_x86_64)
{
    char **argv = build_linker_args(objs, lib_dirs, libs, output,
                                    use_x86_64);

    int rc = command_run(argv);
    free_linker_args(argv);
    if (rc != 1) {
        if (rc == 0)
            fprintf(stderr, "cc failed\n");
        else if (rc == -1)
            fprintf(stderr, "cc terminated by signal\n");
        return 0;
    }
    return 1;
}

/* Create entry stub and link all objects into the final executable. */
static int build_and_link_objects(vector_t *objs, const cli_options_t *cli)
{
    char *stubobj = NULL;
    int ok = create_startup_object(cli, cli->use_x86_64, &stubobj);
    if (ok) {
        if (!vector_push(objs, &stubobj)) {
            fprintf(stderr, "Out of memory\n");
            unlink(stubobj);
            free(stubobj);
            return 0;
        }
        ok = run_link_command(objs, &cli->lib_dirs, &cli->libs,
                              cli->output, cli->use_x86_64);
    }
    return ok;
}

/* Run the preprocessor and print the result. */
int run_preprocessor(const cli_options_t *cli)
{
    for (size_t i = 0; i < cli->sources.count; i++) {
        const char *src = ((const char **)cli->sources.data)[i];
        char *text = preproc_run(src, &cli->include_dirs, &cli->defines,
                                &cli->undefines);
        if (!text) {
            perror("preproc_run");
            return 1;
        }
        size_t len = strlen(text);
        if (fwrite(text, 1, len, stdout) != len) {
            perror("fwrite");
            free(text);
            return 1;
        }
        if (len == 0 || text[len - 1] != '\n') {
            if (putchar('\n') == EOF) {
                perror("putchar");
                free(text);
                return 1;
            }
        }
        if (fflush(stdout) == EOF) {
            perror("fflush");
            free(text);
            return 1;
        }
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
    vector_free(&objs);

    return ok;
}
