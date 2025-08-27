#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
/*
 * Preprocessor module for vc.
 *
 * This file drives reading source text and performing directive
 * processing.  It works with the helpers in `preproc_expand.c`
 * and `preproc_table.c` alongside `preproc_expr.c` to expand macros and
 * evaluate conditional expressions, forming a small stand-alone
 * preprocessor used by the compiler.  Preprocessing happens in three
 * stages: `open_source_file()` loads the text and records it on the
 * include stack, `process_file_lines()` expands each line and handles
 * directives, and `close_source_file()` releases all temporary data.
 * `process_file()` merely coordinates these helpers for each file.
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


static int process_file_lines(char **lines, const char *path, const char *dir,
                              vector_t *macros, vector_t *conds, strbuf_t *out,
                              const vector_t *incdirs, vector_t *stack,
                              preproc_context_t *ctx)
{
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
    return ok;
}

static void close_source_file(char *text, char **lines, char *dir,
                              vector_t *stack, preproc_context_t *ctx)
{
    include_stack_pop(stack, ctx);
    cleanup_file_resources(text, lines, dir);
}

/*
 * Core coordinator for preprocessing.  Each file is opened with
 * open_source_file(), all lines are processed via process_file_lines()
 * and temporary resources are released by close_source_file().
 */
int process_file(const char *path, vector_t *macros,
                        vector_t *conds, strbuf_t *out,
                        const vector_t *incdirs, vector_t *stack,
                        preproc_context_t *ctx, size_t idx)
{
    char **lines;
    char *dir;
    char *text;

    if (!open_source_file(path, stack, idx, &lines, &dir, &text, ctx))
        return 0;

    int ok = process_file_lines(lines, path, dir, macros, conds, out,
                                incdirs, stack, ctx);

    close_source_file(text, lines, dir, stack, ctx);
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
    if (ctx->max_expand_size == 0)
        ctx->max_expand_size = SIZE_MAX;
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

/* Convenience structure used to define groups of builtin macros */
typedef struct {
    const char *name;
    const char *value;
} macro_def_t;

/* Define all macros in LIST until a NULL name is encountered */
static int define_macro_list(vector_t *macros, const macro_def_t *list)
{
    for (size_t i = 0; list[i].name; i++) {
        if (!define_simple_macro(macros, list[i].name, list[i].value))
            return 0;
    }
    return 1;
}

/* Architecture specific macros mimic GCC for LP64 and ILP32 targets */
static int define_arch_macros(vector_t *macros, bool use_x86_64)
{
    static const macro_def_t arch64[] = {
        {"__x86_64__", "1"},
        {"__SIZE_TYPE__", "unsigned long"},
        {"__PTRDIFF_TYPE__", "long"},
        {NULL, NULL}
    };
    static const macro_def_t arch32[] = {
        {"__i386__", "1"},
        {"__SIZE_TYPE__", "unsigned int"},
        {"__PTRDIFF_TYPE__", "int"},
        {NULL, NULL}
    };
    return define_macro_list(macros, use_x86_64 ? arch64 : arch32);
}

/* Operating system macros used by system headers */
static int define_host_macros(vector_t *macros)
{
#ifdef __linux__
    static const macro_def_t os_list[] = {
        {"__linux__", "1"},
        {"__unix__", "1"},
        {NULL, NULL}
    };
    return define_macro_list(macros, os_list);
#else
    (void)macros;
    return 1;
#endif
}

/* Compiler identification macros follow the host GCC values */
static int define_compiler_macros(vector_t *macros)
{
#ifdef __GNUC__
#define STR2(x) #x
#define STR(x) STR2(x)
    static const macro_def_t gcc_list[] = {
        {"__GNUC__", STR(__GNUC__)},
        {"__GNUC_MINOR__", STR(__GNUC_MINOR__)},
        {"__GNUC_PATCHLEVEL__", STR(__GNUC_PATCHLEVEL__)},
        {NULL, NULL}
    };
#undef STR
#undef STR2
    return define_macro_list(macros, gcc_list);
#else
    (void)macros;
    return 1;
#endif
}

/* Misc host feature macros copied when available */
static int define_feature_macros(vector_t *macros)
{
#define STR2(x) #x
#define STR(x) STR2(x)
#define HOST_MACRO(name) \
    do { if (!define_simple_macro(macros, #name, STR(name))) return 0; } while (0)

#ifdef __STDC_UTF_16__
    HOST_MACRO(__STDC_UTF_16__);
#endif
#ifdef __STDC_UTF_32__
    HOST_MACRO(__STDC_UTF_32__);
#endif
#ifdef __STDC_ISO_10646__
    HOST_MACRO(__STDC_ISO_10646__);
#endif
#ifdef __STDC_IEC_559__
    HOST_MACRO(__STDC_IEC_559__);
#endif
#ifdef __STDC_IEC_559_COMPLEX__
    HOST_MACRO(__STDC_IEC_559_COMPLEX__);
#endif
#ifdef __STDC_IEC_60559_BFP__
    HOST_MACRO(__STDC_IEC_60559_BFP__);
#endif
#ifdef __STDC_IEC_60559_COMPLEX__
    HOST_MACRO(__STDC_IEC_60559_COMPLEX__);
#endif
#ifdef __WCHAR_TYPE__
    HOST_MACRO(__WCHAR_TYPE__);
#endif
#ifdef __WINT_TYPE__
    HOST_MACRO(__WINT_TYPE__);
#endif
#ifdef __INTMAX_TYPE__
    HOST_MACRO(__INTMAX_TYPE__);
#endif
#ifdef __UINTMAX_TYPE__
    HOST_MACRO(__UINTMAX_TYPE__);
#endif
#ifdef __INTPTR_TYPE__
    HOST_MACRO(__INTPTR_TYPE__);
#endif
#ifdef __UINTPTR_TYPE__
    HOST_MACRO(__UINTPTR_TYPE__);
#endif
#ifdef __CHAR_BIT__
    HOST_MACRO(__CHAR_BIT__);
#endif
#ifdef __SIZEOF_SHORT__
    HOST_MACRO(__SIZEOF_SHORT__);
#endif
#ifdef __SIZEOF_INT__
    HOST_MACRO(__SIZEOF_INT__);
#endif
#ifdef __SIZEOF_LONG__
    HOST_MACRO(__SIZEOF_LONG__);
#endif
#ifdef __SIZEOF_LONG_LONG__
    HOST_MACRO(__SIZEOF_LONG_LONG__);
#endif
#ifdef __SIZEOF_POINTER__
    HOST_MACRO(__SIZEOF_POINTER__);
#endif
#ifdef __SIZEOF_WCHAR_T__
    HOST_MACRO(__SIZEOF_WCHAR_T__);
#endif
#ifdef __BYTE_ORDER__
    HOST_MACRO(__BYTE_ORDER__);
#endif
#ifdef __ORDER_LITTLE_ENDIAN__
    HOST_MACRO(__ORDER_LITTLE_ENDIAN__);
#endif
#ifdef __ORDER_BIG_ENDIAN__
    HOST_MACRO(__ORDER_BIG_ENDIAN__);
#endif
#ifdef __FLOAT_WORD_ORDER__
    HOST_MACRO(__FLOAT_WORD_ORDER__);
#endif

#undef HOST_MACRO
#undef STR
#undef STR2
    return 1;
}

/* Add some common builtin macros based on the host compiler. The path to the
 * main source file is used to initialise __BASE_FILE__. */
static int define_default_macros(vector_t *macros, const char *base_file,
                                 bool use_x86_64)
{
    define_simple_macro(macros, "__STDC__", "1");
    define_simple_macro(macros, "__STDC_HOSTED__", "1");

    if (!define_arch_macros(macros, use_x86_64) ||
        !define_host_macros(macros) ||
        !define_compiler_macros(macros) ||
        !define_feature_macros(macros))
        return 0;

    /* Predefined macros for internal bookkeeping */
    if (base_file) {
        char *canon = realpath(base_file, NULL);
        if (!canon)
            canon = vc_strdup(base_file);
        if (!canon) {
            vc_oom();
            return 0;
        }
        char quoted[PATH_MAX + 2];
        int n = snprintf(quoted, sizeof(quoted), "\"%s\"", canon);
        if (n < 0 || (size_t)n >= sizeof(quoted)) {
            free(canon);
            errno = ENAMETOOLONG;
            return 0;
        }
        define_simple_macro(macros, "__BASE_FILE__", quoted);
        free(canon);
    } else {
        define_simple_macro(macros, "__BASE_FILE__", "\"\"");
    }
    return 1;
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
/* Remove matching single or double quotes around a string value.
 * Returns a newly allocated copy without the quotes when present or
 * NULL when no stripping was performed. */
static char *unquote_value(const char *val)
{
    size_t len = strlen(val);
    if (len >= 2 &&
        ((val[0] == '"' && val[len - 1] == '"') ||
         (val[0] == '\'' && val[len - 1] == '\''))) {
        return vc_strndup(val + 1, len - 2);
    }
    return NULL;
}

static int update_macros_from_cli(vector_t *macros, const vector_t *defines,
                                  const vector_t *undefines)
{
    if (defines) {
        for (size_t i = 0; i < defines->count; i++) {
            const char *def = ((const char **)defines->data)[i];
            const char *eq = strchr(def, '=');
            const char *val = "1";
            char *name;
            char *unquoted = NULL;
            if (eq) {
                name = vc_strndup(def, (size_t)(eq - def));
                if (!name)
                    return 0;
                val = eq + 1;
                unquoted = unquote_value(val);
                if (unquoted)
                    val = unquoted;
            } else {
                name = vc_strdup(def);
                if (!name)
                    return 0;
            }
            vector_t params;
            vector_init(&params, sizeof(char *));
            remove_macro(macros, name);
            if (!add_macro(name, val, &params, 0, macros)) {
                free(name);
                free(unquoted);
                return 0;
            }
            free(name);
            free(unquoted);
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
                  const char *sysroot, const char *vc_sysinclude,
                  bool internal_libc, bool use_x86_64)
{
    vector_t search_dirs, macros, conds, stack;
    strbuf_t out;

    /* Build include search list from CLI options and environment */
    if (!collect_include_dirs(&search_dirs, include_dirs, sysroot,
                              vc_sysinclude, internal_libc))
        return NULL;

    /* Prepare all vectors used during preprocessing */
    init_preproc_vectors(ctx, &macros, &conds, &stack, &out);
    /* Reset builtin counter so each run starts from zero */
    ctx->counter = 0;
    if (!define_default_macros(&macros, path, use_x86_64)) {
        cleanup_preproc_vectors(ctx, &macros, &conds, &stack, &search_dirs, &out);
        return NULL;
    }
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
    if (ok) {
        res = vc_strdup(out.data ? out.data : "");
        if (!res)
            vc_oom();
    }

    cleanup_preproc_vectors(ctx, &macros, &conds, &stack, &search_dirs, &out);
    errno = saved_errno;

    return res;
}

