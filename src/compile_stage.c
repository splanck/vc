#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
#include "ast_dump.h"
#include "opt.h"
#include "codegen.h"
#include "label.h"
#include "preproc.h"
#include "command.h"
#include "compile_stage.h"
#include "compile.h"

extern const char *error_current_file;
extern const char *error_current_function;

/* Stage implementations from other compilation units */
int compile_tokenize_impl(const char *source, const cli_options_t *cli,
                          const vector_t *incdirs,
                          const vector_t *defines,
                          const vector_t *undefines,
                          char **out_src, token_t **out_toks,
                          size_t *out_count, char **tmp_path,
                          vector_t *deps);
char *tokens_to_string(const token_t *toks, size_t count);
int compile_parse_impl(token_t *toks, size_t count,
                       vector_t *funcs_v, vector_t *globs_v,
                       symtable_t *funcs);
static int compile_semantic_impl(func_t **func_list, size_t fcount,
                                 stmt_t **glob_list, size_t gcount,
                                 symtable_t *funcs, symtable_t *globals,
                                 ir_builder_t *ir);
static int compile_optimize_impl(ir_builder_t *ir, const opt_config_t *cfg);
int compile_output_impl(ir_builder_t *ir, const char *output,
                        int dump_ir, int dump_asm, int use_x86_64,
                        int compile, const cli_options_t *cli);
int write_dep_file(const char *target, const vector_t *deps);

/* Compilation context used by the pipeline */
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
    vector_t    deps;
    size_t      pack_alignment;
} compile_context_t;

/* --- Semantic analysis helpers --------------------------------------- */
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
            if (func_list[i]->is_noreturn)
                existing->is_noreturn = 1;
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
                              func_list[i]->is_inline,
                              func_list[i]->is_noreturn);
        }
    }
    return 1;
}

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

static int compile_optimize_impl(ir_builder_t *ir, const opt_config_t *cfg)
{
    if (cfg)
        opt_run(ir, cfg);
    return 1;
}

/* --- Context management ----------------------------------------------- */
static void compile_ctx_init(compile_context_t *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    vector_init(&ctx->func_list_v, sizeof(func_t *));
    vector_init(&ctx->glob_list_v, sizeof(stmt_t *));
    symtable_init(&ctx->funcs);
    symtable_init(&ctx->globals);
    ir_builder_init(&ctx->ir);
    vector_init(&ctx->deps, sizeof(char *));
    ctx->pack_alignment = 0;
    semantic_set_pack(0);
}

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

    free_string_vector(&ctx->deps);

    lexer_free_tokens(ctx->tokens, ctx->tok_count);
    free(ctx->src_text);
}

/* --- Stage wrappers ---------------------------------------------------- */
/* Tokenize source and run the preprocessor */
static int compile_tokenize_stage(compile_context_t *ctx, const char *source,
                                  const cli_options_t *cli,
                                  const vector_t *incdirs,
                                  const vector_t *defines,
                                  const vector_t *undefines)
{
    return compile_tokenize_impl(source, cli, incdirs, defines, undefines,
                                 &ctx->src_text,
                                 &ctx->tokens, &ctx->tok_count,
                                 &ctx->stdin_tmp, &ctx->deps);
}

/* Parse tokens into an AST */
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

/* Perform semantic analysis and IR generation */
static int compile_semantic_stage(compile_context_t *ctx)
{
    return compile_semantic_impl((func_t **)ctx->func_list_v.data,
                                 ctx->func_list_v.count,
                                 (stmt_t **)ctx->glob_list_v.data,
                                 ctx->glob_list_v.count,
                                 &ctx->funcs, &ctx->globals,
                                 &ctx->ir);
}

/* Run optimizer passes on the IR */
static int compile_optimize_stage(compile_context_t *ctx,
                                  const opt_config_t *cfg)
{
    return compile_optimize_impl(&ctx->ir, cfg);
}

/* Emit object code or assembly */
static int compile_output_stage(compile_context_t *ctx, const char *output,
                                int dump_ir, int dump_asm, int use_x86_64,
                                int compile_obj, const cli_options_t *cli)
{
    return compile_output_impl(&ctx->ir, output, dump_ir, dump_asm,
                               use_x86_64, compile_obj, cli);
}

/* --- High level helpers ----------------------------------------------- */
static void init_compile_context(compile_context_t *ctx, const char *source,
                                 const cli_options_t *cli)
{
    error_current_file = source ? source : "";
    error_current_function = NULL;
    label_init();
    codegen_set_export(cli->link);
    codegen_set_debug(cli->debug || cli->emit_dwarf);
    codegen_set_dwarf(cli->emit_dwarf);
    compile_ctx_init(ctx);
}

static void finalize_compile_context(compile_context_t *ctx)
{
    compile_ctx_cleanup(ctx);
    semantic_global_cleanup();
    if (ctx->stdin_tmp) {
        unlink(ctx->stdin_tmp);
        free(ctx->stdin_tmp);
    }
    label_reset();
}

static int run_tokenize(compile_context_t *ctx, const char *source,
                        const cli_options_t *cli)
{
    int ok = compile_tokenize_stage(ctx, source, cli,
                                    &cli->include_dirs,
                                    &cli->defines,
                                    &cli->undefines);
    if (ok && cli->dump_tokens) {
        char *text = tokens_to_string(ctx->tokens, ctx->tok_count);
        if (text) {
            printf("%s", text);
            free(text);
        }
    }
    return ok;
}

static int run_parse(compile_context_t *ctx, const cli_options_t *cli)
{
    if (cli->dump_tokens)
        return 1;

    int ok = compile_parse_stage(ctx);
    if (ok && cli->dump_ast) {
        char *text = ast_to_string((func_t **)ctx->func_list_v.data,
                                   ctx->func_list_v.count,
                                   (stmt_t **)ctx->glob_list_v.data,
                                   ctx->glob_list_v.count);
        if (text) {
            printf("%s", text);
            free(text);
        }
    }
    return ok;
}

static int run_semantic(compile_context_t *ctx, const cli_options_t *cli)
{
    if (cli->dump_ast || cli->dump_tokens)
        return 1;
    return compile_semantic_stage(ctx);
}

static int run_optimize(compile_context_t *ctx, const cli_options_t *cli)
{
    if (cli->dump_ast || cli->dump_tokens)
        return 1;
    return compile_optimize_stage(ctx, &cli->opt_cfg);
}

static int run_output(compile_context_t *ctx, const char *output,
                      int compile_obj, const cli_options_t *cli)
{
    if (cli->dump_ast || cli->dump_tokens)
        return 1;
    return compile_output_stage(ctx, output, cli->dump_ir, cli->dump_asm,
                                cli->use_x86_64, compile_obj, cli);
}

/* --- Public API ------------------------------------------------------- */
/*
 * compile_pipeline orchestrates the full compilation process:
 * tokenization, parsing, semantic analysis, optimization and
 * final output generation.
 */
int compile_pipeline(const char *source, const cli_options_t *cli,
                     const char *output, int compile_obj)
{
    int ok = 1;
    compile_context_t ctx;

    init_compile_context(&ctx, source, cli);

    ok = run_tokenize(&ctx, source, cli);
    if (ok)
        ok = run_parse(&ctx, cli);
    if (ok)
        ok = run_semantic(&ctx, cli);
    if (ok)
        ok = run_optimize(&ctx, cli);
    if (ok)
        ok = run_output(&ctx, output, compile_obj, cli);

    if (ok && cli->deps)
        ok = write_dep_file(output ? output : source, &ctx.deps);

    finalize_compile_context(&ctx);

    return ok;
}

