#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
/*
 * Preprocessor module for vc.
 *
 * This file drives reading source text and performing directive
 * processing.  It works with the helpers in `preproc_expand.c`
 * and `preproc_table.c` alongside `preproc_expr.c` to expand macros and evaluate conditional
 * expressions, forming a small stand-alone preprocessor used by
 * the compiler.  The core routine `process_file` reads input one
 * line at a time, handles `#include`, `#define`, conditional blocks
 * and macro expansion, and writes the final text to a buffer.
 *
 * Part of vc under the BSD 2-Clause Simplified License.
 * See LICENSE for details.
 */

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "preproc_file.h"
#include "preproc_macros.h"
#include "preproc_cond.h"
#include "preproc_include.h"
#include "preproc_file_io.h"
#include "preproc_path.h"
#include "semantic_global.h"
#include "util.h"
#include "vector.h"
#include "strbuf.h"

/*
 * Core file processing routine.  Reads the file, handles directives
 * and macro expansion line by line, writing the preprocessed result
 * to the output buffer.
 */
int process_file(const char *path, vector_t *macros,
                        vector_t *conds, strbuf_t *out,
                        const vector_t *incdirs, vector_t *stack,
                        preproc_context_t *ctx, size_t idx)
{
    if (stack->count >= ctx->max_include_depth) {
        fprintf(stderr, "Include depth limit exceeded: %s (depth %zu)\n",
                path, stack->count);
        return 0;
    }
    char **lines;
    char *dir;
    char *text;

    if (!load_and_register_file(path, stack, idx, &lines, &dir, &text, ctx))
        return 0;

    ctx->system_header = 0;

    char *prev_file;
    long prev_delta;
    line_state_push(ctx, path, 0, &prev_file, &prev_delta);

    int ok = process_all_lines(lines, path, dir, macros, conds, out, incdirs,
                               stack, ctx);

    if (ok && ctx->in_comment) {
        fprintf(stderr, "Unterminated comment\n");
        ok = 0;
    }

    if (ok && conds->count) {
        cond_state_t *st = &((cond_state_t *)conds->data)[conds->count - 1];
        fprintf(stderr, "%s:%zu: unterminated conditional started here\n",
                path, st->line);
        ok = 0;
    }

    line_state_pop(ctx, prev_file, prev_delta);

    include_stack_pop(stack, ctx);

    cleanup_file_resources(text, lines, dir);
    return ok;
}

/* Initialize the vectors used during preprocessing */
static void init_preproc_vectors(preproc_context_t *ctx, vector_t *macros,
                                 vector_t *conds, vector_t *stack,
                                 strbuf_t *out)
{
    vector_init(macros, sizeof(macro_t));
    vector_init(conds, sizeof(cond_state_t));
    vector_init(stack, sizeof(include_entry_t));
    vector_init(&ctx->pragma_once_files, sizeof(char *));
    vector_init(&ctx->deps, sizeof(char *));
    vector_init(&ctx->pack_stack, sizeof(size_t));
    ctx->pack_alignment = 0;
    ctx->in_comment = 0;
    ctx->current_file = NULL;
    ctx->line_delta = 0;
    ctx->file = "";
    ctx->line = 0;
    ctx->column = 1;
    ctx->func = NULL;
    ctx->base_file = "";
    ctx->include_level = 0;
    ctx->counter = 0;
    ctx->system_header = 0;
    if (ctx->max_include_depth == 0)
        ctx->max_include_depth = DEFAULT_INCLUDE_DEPTH;
    strbuf_init(out);
}

/* Define a simple object-like macro with value VAL */
static int define_simple_macro(vector_t *macros, const char *name,
                               const char *val)
{
    vector_t params;
    vector_init(&params, sizeof(char *));
    remove_macro(macros, name);
    return add_macro(name, val, &params, 0, macros);
}

/* Add some common builtin macros based on the host compiler. The path to the
 * main source file is used to initialise __BASE_FILE__. */
static void define_default_macros(vector_t *macros, const char *base_file)
{
#define STR2(x) #x
#define STR(x) STR2(x)

#ifdef __linux__
    define_simple_macro(macros, "__linux__", "1");
    define_simple_macro(macros, "__unix__", "1");
#endif

#if defined(__x86_64__) || defined(__amd64__)
    define_simple_macro(macros, "__x86_64__", "1");
# ifdef __LP64__
    define_simple_macro(macros, "__LP64__", "1");
# endif
#endif

#ifdef __GNUC__
    define_simple_macro(macros, "__GNUC__", STR(__GNUC__));
    define_simple_macro(macros, "__GNUC_MINOR__", STR(__GNUC_MINOR__));
    define_simple_macro(macros, "__GNUC_PATCHLEVEL__", STR(__GNUC_PATCHLEVEL__));
#endif
#ifdef __STDC_HOSTED__
    define_simple_macro(macros, "__STDC_HOSTED__", STR(__STDC_HOSTED__));
#endif
#ifdef __STDC_UTF_16__
    define_simple_macro(macros, "__STDC_UTF_16__", STR(__STDC_UTF_16__));
#endif
#ifdef __STDC_UTF_32__
    define_simple_macro(macros, "__STDC_UTF_32__", STR(__STDC_UTF_32__));
#endif
#ifdef __STDC_ISO_10646__
    define_simple_macro(macros, "__STDC_ISO_10646__", STR(__STDC_ISO_10646__));
#endif
#ifdef __STDC_IEC_559__
    define_simple_macro(macros, "__STDC_IEC_559__", STR(__STDC_IEC_559__));
#endif
#ifdef __STDC_IEC_559_COMPLEX__
    define_simple_macro(macros, "__STDC_IEC_559_COMPLEX__", STR(__STDC_IEC_559_COMPLEX__));
#endif
#ifdef __STDC_IEC_60559_BFP__
    define_simple_macro(macros, "__STDC_IEC_60559_BFP__", STR(__STDC_IEC_60559_BFP__));
#endif
#ifdef __STDC_IEC_60559_COMPLEX__
    define_simple_macro(macros, "__STDC_IEC_60559_COMPLEX__", STR(__STDC_IEC_60559_COMPLEX__));
#endif

#ifdef __SIZE_TYPE__
    define_simple_macro(macros, "__SIZE_TYPE__", STR(__SIZE_TYPE__));
#endif
#ifdef __PTRDIFF_TYPE__
    define_simple_macro(macros, "__PTRDIFF_TYPE__", STR(__PTRDIFF_TYPE__));
#endif
#ifdef __WCHAR_TYPE__
    define_simple_macro(macros, "__WCHAR_TYPE__", STR(__WCHAR_TYPE__));
#endif
#ifdef __WINT_TYPE__
    define_simple_macro(macros, "__WINT_TYPE__", STR(__WINT_TYPE__));
#endif
#ifdef __INTMAX_TYPE__
    define_simple_macro(macros, "__INTMAX_TYPE__", STR(__INTMAX_TYPE__));
#endif
#ifdef __UINTMAX_TYPE__
    define_simple_macro(macros, "__UINTMAX_TYPE__", STR(__UINTMAX_TYPE__));
#endif
#ifdef __INTPTR_TYPE__
    define_simple_macro(macros, "__INTPTR_TYPE__", STR(__INTPTR_TYPE__));
#endif
#ifdef __UINTPTR_TYPE__
    define_simple_macro(macros, "__UINTPTR_TYPE__", STR(__UINTPTR_TYPE__));
#endif
#ifdef __CHAR_BIT__
    define_simple_macro(macros, "__CHAR_BIT__", STR(__CHAR_BIT__));
#endif
#ifdef __SIZEOF_SHORT__
    define_simple_macro(macros, "__SIZEOF_SHORT__", STR(__SIZEOF_SHORT__));
#endif
#ifdef __SIZEOF_INT__
    define_simple_macro(macros, "__SIZEOF_INT__", STR(__SIZEOF_INT__));
#endif
#ifdef __SIZEOF_LONG__
    define_simple_macro(macros, "__SIZEOF_LONG__", STR(__SIZEOF_LONG__));
#endif
#ifdef __SIZEOF_LONG_LONG__
    define_simple_macro(macros, "__SIZEOF_LONG_LONG__", STR(__SIZEOF_LONG_LONG__));
#endif
#ifdef __SIZEOF_POINTER__
    define_simple_macro(macros, "__SIZEOF_POINTER__", STR(__SIZEOF_POINTER__));
#endif
#ifdef __SIZEOF_WCHAR_T__
    define_simple_macro(macros, "__SIZEOF_WCHAR_T__", STR(__SIZEOF_WCHAR_T__));
#endif
#ifdef __BYTE_ORDER__
    define_simple_macro(macros, "__BYTE_ORDER__", STR(__BYTE_ORDER__));
#endif
#ifdef __ORDER_LITTLE_ENDIAN__
    define_simple_macro(macros, "__ORDER_LITTLE_ENDIAN__", STR(__ORDER_LITTLE_ENDIAN__));
#endif
#ifdef __ORDER_BIG_ENDIAN__
    define_simple_macro(macros, "__ORDER_BIG_ENDIAN__", STR(__ORDER_BIG_ENDIAN__));
#endif
#ifdef __FLOAT_WORD_ORDER__
    define_simple_macro(macros, "__FLOAT_WORD_ORDER__", STR(__FLOAT_WORD_ORDER__));
#endif
#undef STR
#undef STR2

    /* Predefined macros for internal bookkeeping */
    define_simple_macro(macros, "__COUNTER__", "0");
    if (base_file) {
        char *canon = realpath(base_file, NULL);
        if (!canon)
            canon = vc_strdup(base_file);
        char quoted[PATH_MAX + 2];
        snprintf(quoted, sizeof(quoted), "\"%s\"", canon);
        define_simple_macro(macros, "__BASE_FILE__", quoted);
        free(canon);
    } else {
        define_simple_macro(macros, "__BASE_FILE__", "\"\"");
    }
}

/* Release vectors and buffers used during preprocessing */
static void cleanup_preproc_vectors(preproc_context_t *ctx, vector_t *macros,
                                    vector_t *conds, vector_t *stack,
                                    vector_t *search_dirs, strbuf_t *out)
{
    for (size_t i = 0; i < stack->count; i++) {
        include_entry_t *e = &((include_entry_t *)stack->data)[i];
        free(e->path);
    }
    vector_free(stack);
    vector_free(conds);
    free_macro_vector(macros);
    free_string_vector(search_dirs);
    for (size_t i = 0; i < ctx->pragma_once_files.count; i++)
        free(((char **)ctx->pragma_once_files.data)[i]);
    vector_free(&ctx->pragma_once_files);
    strbuf_free(out);
}

/* Free dependency lists stored in the context */
void preproc_context_free(preproc_context_t *ctx)
{
    for (size_t i = 0; i < ctx->deps.count; i++)
        free(((char **)ctx->deps.data)[i]);
    vector_free(&ctx->deps);
    free(ctx->current_file);
    vector_free(&ctx->pack_stack);
    preproc_path_cleanup();
}

/*
 * Update the macro table with command line definitions and undefinitions.
 *
 * Returns 1 on success, 0 on failure when adding a definition.
 */
static int update_macros_from_cli(vector_t *macros, const vector_t *defines,
                                  const vector_t *undefines)
{
    if (defines) {
        for (size_t i = 0; i < defines->count; i++) {
            const char *def = ((const char **)defines->data)[i];
            const char *eq = strchr(def, '=');
            const char *val = "1";
            char *name;
            if (eq) {
                name = vc_strndup(def, (size_t)(eq - def));
                val = eq + 1;
            } else {
                name = vc_strdup(def);
            }
            vector_t params;
            vector_init(&params, sizeof(char *));
            remove_macro(macros, name);
            if (!add_macro(name, val, &params, 0, macros)) {
                free(name);
                return 0;
            }
            free(name);
        }
    }

    if (undefines) {
        for (size_t i = 0; i < undefines->count; i++) {
            const char *name = ((const char **)undefines->data)[i];
            remove_macro(macros, name);
        }
    }

    return 1;
}

/* Wrapper around process_file used by the entry point */
static int process_input_file(const char *path, vector_t *macros,
                              vector_t *conds, strbuf_t *out,
                              const vector_t *incdirs, vector_t *stack,
                              preproc_context_t *ctx)
{
    return process_file(path, macros, conds, out, incdirs, stack, ctx,
                        (size_t)-1);
}

/*
 * Entry point used by the compiler.  Sets up include search paths,
 * invokes the file processor and returns the resulting text.
 */
char *preproc_run(preproc_context_t *ctx, const char *path,
                  const vector_t *include_dirs,
                  const vector_t *defines, const vector_t *undefines,
                  const char *sysroot)
{
    vector_t search_dirs, macros, conds, stack;
    strbuf_t out;

    /* Build include search list from CLI options and environment */
    if (!collect_include_dirs(&search_dirs, include_dirs, sysroot))
        return NULL;

    /* Prepare all vectors used during preprocessing */
    init_preproc_vectors(ctx, &macros, &conds, &stack, &out);
    /* Reset builtin counter so each run starts from zero */
    ctx->counter = 0;
    define_default_macros(&macros, path);
    if (!record_dependency(ctx, path)) {
        cleanup_preproc_vectors(ctx, &macros, &conds, &stack, &search_dirs, &out);
        return NULL;
    }

    /* Apply -D and -U options from the command line */
    if (!update_macros_from_cli(&macros, defines, undefines)) {
        cleanup_preproc_vectors(ctx, &macros, &conds, &stack, &search_dirs, &out);
        return NULL;
    }

    /* Process the initial source file */
    int ok = process_input_file(path, &macros, &conds, &out,
                                &search_dirs, &stack, ctx);

    int saved_errno = errno;
    char *res = NULL;
    if (ok)
        res = vc_strdup(out.data ? out.data : "");

    cleanup_preproc_vectors(ctx, &macros, &conds, &stack, &search_dirs, &out);
    errno = saved_errno;

    return res;
}

