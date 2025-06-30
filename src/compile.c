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

/* Compilation stage helpers */
static int compile_tokenize(const char *source, const vector_t *incdirs,
                            char **out_src, token_t **out_toks,
                            size_t *out_count, char **tmp_path);
static int compile_parse(token_t *toks, size_t count,
                         vector_t *funcs_v, vector_t *globs_v,
                         symtable_t *funcs);
static int compile_semantic(func_t **func_list, size_t fcount,
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
static int compile_optimize(ir_builder_t *ir, const opt_config_t *cfg);
static int compile_output(ir_builder_t *ir, const char *output,
                          int dump_ir, int dump_asm, int use_x86_64,
                          int compile, const cli_options_t *cli);
static int emit_output_file(ir_builder_t *ir, const char *output,
                            int use_x86_64, int compile_obj,
                            const cli_options_t *cli);
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

/* Spawn a command and wait for completion */
static int run_command(char *const argv[]);

/* Write the entry stub assembly to a temporary file. */
static int write_startup_asm(int use_x86_64, const cli_options_t *cli,
                             char **out_path);

/* Assemble the stub into an object file. */
static int assemble_startup_obj(const char *asm_path, int use_x86_64,
                               const cli_options_t *cli, char **out_path);

/* Create a temporary file and return its descriptor. */
#ifdef UNIT_TESTING
int create_temp_file(const cli_options_t *cli, const char *prefix,
                     char **out_path);
#else
static int create_temp_file(const cli_options_t *cli, const char *prefix,
                            char **out_path);
#endif

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

/* Tokenize the preprocessed source file */
static int compile_tokenize(const char *source, const vector_t *incdirs,
                            char **out_src, token_t **out_toks,
                            size_t *out_count, char **tmp_path)
{
    if (tmp_path)
        *tmp_path = NULL;

    if (source && strcmp(source, "-") == 0) {
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
                if (fclose(f) == EOF) {
                    perror("fclose");
                }
                unlink(tmpl);
                return 0;
            }
        }
        if (ferror(stdin)) {
            perror("fread");
            if (fclose(f) == EOF) {
                perror("fclose");
            }
            unlink(tmpl);
            return 0;
        }
        if (fclose(f) == EOF) {
            perror("fclose");
            unlink(tmpl);
            return 0;
        }
        char *stdin_path = strdup(tmpl);
        if (!stdin_path) {
            unlink(tmpl);
            return 0;
        }
        source = stdin_path;
        char *text = preproc_run(stdin_path, incdirs);
        if (!text) {
            perror("preproc_run");
            unlink(stdin_path);
            free(stdin_path);
            return 0;
        }
        size_t count = 0;
        token_t *toks = lexer_tokenize(text, &count);
        if (!toks) {
            fprintf(stderr, "Tokenization failed\n");
            free(text);
            unlink(stdin_path);
            free(stdin_path);
            return 0;
        }
        *out_src = text;
        *out_toks = toks;
        if (out_count)
            *out_count = count;
        if (tmp_path)
            *tmp_path = stdin_path;
        else {
            unlink(stdin_path);
            free(stdin_path);
        }
        return 1;
    }

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
static int compile_parse(token_t *toks, size_t count,
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
                           existing->param_count != func_list[i]->param_count ||
                           existing->is_variadic != func_list[i]->is_variadic;
            for (size_t j = 0; j < existing->param_count && !mismatch; j++)
                if (existing->param_types[j] != func_list[i]->param_types[j])
                    mismatch = 1;
            if (mismatch) {
                error_set(0, 0, error_current_file, error_current_function);
                return 0;
            }
            existing->is_prototype = 0;
            if (func_list[i]->is_inline)
                existing->is_inline = 1;
        } else {
            symtable_add_func(funcs, func_list[i]->name,
                              func_list[i]->return_type,
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
static int compile_semantic(func_t **func_list, size_t fcount,
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
static int compile_optimize(ir_builder_t *ir, const opt_config_t *cfg)
{
    if (cfg)
        opt_run(ir, cfg);
    return 1;
}

/* Emit the requested output */
static int compile_output(ir_builder_t *ir, const char *output,
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
        char *text = codegen_ir_to_string(ir, use_x86_64);
        if (text) {
            printf("%s", text);
            free(text);
        }
        return 1;
    }

    return emit_output_file(ir, output, use_x86_64, compile, cli);
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
        codegen_emit_x86(tmpf, ir, use_x86_64);
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

        const char *arch_flag = use_x86_64 ? "-m64" : "-m32";
        char *argv[] = {"cc", "-x", "assembler", (char *)arch_flag, "-c",
                        tmpname, "-o", (char *)output, NULL};
        int rc = run_command(argv);
        unlink(tmpname);
        free(tmpname);
        if (rc != 1) {
            if (rc == 0)
                fprintf(stderr, "cc failed\n");
            else if (rc == -1)
                fprintf(stderr, "cc terminated by signal\n");
            return 0;
        }
        return 1;
    }

    FILE *outf = fopen(output, "wb");
    if (!outf) {
        perror("fopen");
        return 0;
    }
    codegen_emit_x86(outf, ir, use_x86_64);
    if (fclose(outf) == EOF) {
        perror("fclose");
        unlink(output);
        return 0;
    }
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
    error_current_file = source ? source : "";
    error_current_function = NULL;
    label_init();
    codegen_set_export(cli->link);
    codegen_set_debug(cli->debug);

    int ok = 1;

    vector_t func_list_v, glob_list_v;
    symtable_t funcs, globals;
    ir_builder_t ir;

    /* Initialize containers so cleanup can run safely */
    vector_init(&func_list_v, sizeof(func_t *));
    vector_init(&glob_list_v, sizeof(stmt_t *));
    symtable_init(&funcs);
    symtable_init(&globals);
    ir_builder_init(&ir);

    /* Tokenization stage */
    char *src_text = NULL;
    token_t *tokens = NULL;
    size_t tok_count = 0;
    char *stdin_tmp = NULL;
    ok = compile_tokenize(source, &cli->include_dirs, &src_text,
                          &tokens, &tok_count, &stdin_tmp);

    /* Parsing stage */
    if (ok)
        ok = compile_parse(tokens, tok_count, &func_list_v, &glob_list_v,
                           &funcs);
    lexer_free_tokens(tokens, tok_count);
    free(src_text);

    /* Semantic analysis */
    if (ok)
        ok = compile_semantic((func_t **)func_list_v.data, func_list_v.count,
                              (stmt_t **)glob_list_v.data, glob_list_v.count,
                              &funcs, &globals, &ir);

    /* Optimization and output */
    if (ok) {
        ok = compile_optimize(&ir, &cli->opt_cfg);
        if (ok)
            ok = compile_output(&ir, output, cli->dump_ir, cli->dump_asm,
                                cli->use_x86_64, compile_obj, cli);
    }

    cleanup_compile_unit(&func_list_v, &glob_list_v, &funcs, &globals, &ir);
    semantic_global_cleanup();

    if (stdin_tmp) {
        unlink(stdin_tmp);
        free(stdin_tmp);
    }

    label_reset();

    return ok;
}

/* Create a temporary file and return its descriptor. */
#ifdef UNIT_TESTING
int create_temp_file(const cli_options_t *cli, const char *prefix,
                     char **out_path)
#else
static int create_temp_file(const cli_options_t *cli, const char *prefix,
                            char **out_path)
#endif
{
    const char *dir = cli->obj_dir ? cli->obj_dir : "/tmp";
    /* dir + '/' + prefix + "XXXXXX" + NUL */
    size_t len = strlen(dir) + strlen(prefix) + sizeof("/XXXXXX");
    if (len > PATH_MAX) {
        *out_path = NULL;
        errno = ENAMETOOLONG;
        return -1;
    }
    char *tmpl = malloc(len + 1);
    if (!tmpl) {
        *out_path = NULL;
        return -1;
    }
    int n = snprintf(tmpl, len + 1, "%s/%sXXXXXX", dir, prefix);
    if (n < 0 || (size_t)n >= len + 1) {
        /* Propagate errno from snprintf if it set one; do not overwrite it */
        free(tmpl);
        *out_path = NULL;
        return -1;
    }
    int fd = mkstemp(tmpl);
    if (fd < 0) {
        perror("mkstemp");
        free(tmpl);
        *out_path = NULL;
        return -1;
    }
    if (fcntl(fd, F_SETFD, FD_CLOEXEC) != 0) {
        int err = errno;
        close(fd);
        free(tmpl);
        *out_path = NULL;
        errno = err;
        return -1;
    }
    *out_path = tmpl;
    return fd;
}

/* Write the entry stub assembly to a temporary file. */
static int write_startup_asm(int use_x86_64, const cli_options_t *cli,
                             char **out_path)
{
    char *asmname = NULL;
    int asmfd = create_temp_file(cli, "vcstub", &asmname);
    if (asmfd < 0)
        return 0;
    FILE *stub = fdopen(asmfd, TEMP_FOPEN_MODE);
    if (!stub) {
        perror("fdopen");
        close(asmfd);
        unlink(asmname);
        free(asmname);
        return 0;
    }
    if (use_x86_64) {
        fputs(".globl _start\n_start:\n    call main\n    mov %rax, %rdi\n    mov $60, %rax\n    syscall\n", stub);
    } else {
        fputs(".globl _start\n_start:\n    call main\n    mov %eax, %ebx\n    mov $1, %eax\n    int $0x80\n", stub);
    }
    if (fclose(stub) == EOF) {
        perror("fclose");
        unlink(asmname);
        free(asmname);
        return 0;
    }

    *out_path = asmname;
    return 1;
}

/* Assemble the entry stub into an object file. */
static int assemble_startup_obj(const char *asm_path, int use_x86_64,
                               const cli_options_t *cli, char **out_path)
{
    char *objname = NULL;
    int objfd = create_temp_file(cli, "vcobj", &objname);
    if (objfd < 0)
        return 0;
    close(objfd);

    const char *arch_flag = use_x86_64 ? "-m64" : "-m32";
    char *argv[] = {"cc", "-x", "assembler", (char *)arch_flag, "-c",
                    (char *)asm_path, "-o", objname, NULL};
    int rc = run_command(argv);
    if (rc != 1) {
        if (rc == 0)
            fprintf(stderr, "cc failed\n");
        else if (rc == -1)
            fprintf(stderr, "cc terminated by signal\n");
        unlink(objname);
        free(objname);
        return 0;
    }

    *out_path = objname;
    return 1;
}

/* Create object file with program entry point */
static int create_startup_object(const cli_options_t *cli, int use_x86_64,
                                char **out_path)
{
    char *asmfile = NULL;
    int ok = write_startup_asm(use_x86_64, cli, &asmfile);
    if (ok)
        ok = assemble_startup_obj(asmfile, use_x86_64, cli, out_path);
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
    char *objname = NULL;
    int fd = create_temp_file(cli, "vcobj", &objname);
    if (fd < 0)
        return 0;
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

/*
 * Spawn a command using posix_spawnp and wait for it to finish.
 *
 * Returns 1 on success, 0 on failure and -1 if the child
 * was terminated by a signal.
 */
static int run_command(char *const argv[])
{
    pid_t pid;
    int status;
    int ret = posix_spawnp(&pid, argv[0], NULL, NULL, argv, environ);
    if (ret != 0) {
        strbuf_t cmd;
        strbuf_init(&cmd);
        for (size_t i = 0; argv[i]; i++) {
            if (i > 0)
                strbuf_append(&cmd, " ");
            strbuf_append(&cmd, argv[i]);
        }
        fprintf(stderr, "posix_spawnp %s: %s\n", cmd.data, strerror(ret));
        strbuf_free(&cmd);
        return 0;
    }
    while (waitpid(pid, &status, 0) < 0) {
        if (errno == EINTR)
            continue;
        perror("waitpid");
        return 0;
    }
    if (WIFSIGNALED(status)) {
        fprintf(stderr, "%s terminated by signal %d\n", argv[0],
                WTERMSIG(status));
        return -1;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        return 0;
    return 1;
}

/* Construct and run the final cc link command. */
static int run_link_command(const vector_t *objs, const char *output,
                            int use_x86_64)
{
    const char *arch_flag = use_x86_64 ? "-m64" : "-m32";
    size_t argc = objs->count + 5;
    char **argv = vc_alloc_or_exit((argc + 1) * sizeof(char *));

    size_t idx = 0;
    argv[idx++] = "cc";
    argv[idx++] = (char *)arch_flag;
    for (size_t i = 0; i < objs->count; i++)
        argv[idx++] = ((char **)objs->data)[i];
    argv[idx++] = "-nostdlib";
    argv[idx++] = "-o";
    argv[idx++] = (char *)output;
    argv[idx] = NULL;

    int rc = run_command(argv);
    free(argv);
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
