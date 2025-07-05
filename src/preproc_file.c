#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
/*
 * Preprocessor module for vc.
 *
 * This file drives reading source text and performing directive
 * processing.  It works with the helpers in `preproc_macros.c`
 * and `preproc_expr.c` to expand macros and evaluate conditional
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "preproc_file.h"
#include "preproc_macros.h"
#include "preproc_expr.h"
#include "preproc_include.h"
#include "preproc_io.h"
#include "util.h"
#include "vector.h"
#include "strbuf.h"

#define MAX_INCLUDE_DEPTH 20

/* Advance P past spaces and tabs and return the updated pointer */
static char *skip_ws(char *p)
{
    while (*p == ' ' || *p == '\t')
        p++;
    return p;
}

typedef struct {
    int parent_active;
    int taking;
    int taken;
} cond_state_t;

static void free_param_vector(vector_t *v);

/* Return 1 if all conditional states on the stack are active */
static int stack_active(vector_t *conds)
{
    for (size_t i = 0; i < conds->count; i++) {
        cond_state_t *c = &((cond_state_t *)conds->data)[i];
        if (!c->taking)
            return 0;
    }
    return 1;
}

/* Wrapper used by directive handlers */
static int is_active(vector_t *conds)
{
    return stack_active(conds);
}


/* forward declaration for recursive include handling */
static int process_file(const char *path, vector_t *macros,
                        vector_t *conds, strbuf_t *out,
                        const vector_t *incdirs, vector_t *stack);

/*
 * Locate the full path of an include file.
 *
 * The search order mirrors traditional compiler behaviour:
 *   1. When the include uses quotes and a directory for the current file
 *      is provided, that directory is checked first.
 *   2. Each path in "incdirs" is consulted in order.
 *   3. For quoted includes the plain filename is tried relative to the
 *      working directory.
 *   4. Finally the built in standard include directories are searched.
 *
 * The resulting path is written to "out_path" if found and a pointer to
 * it is returned.  NULL is returned when the file does not exist in any
 * of the locations.

/*
 * Process one #include directive.  The file name is resolved using
 * find_include_path() and, when the current conditional stack is
 * active, the referenced file is processed recursively.
 */
static int handle_include(char *line, const char *dir, vector_t *macros,
                          vector_t *conds, strbuf_t *out,
                          const vector_t *incdirs, vector_t *stack)
{
    char *start = strchr(line, '"');
    char endc = '"';
    if (!start) {
        start = strchr(line, '<');
        endc = '>';
    }
    char *end = start ? strchr(start + 1, endc) : NULL;
    char *fname = NULL;
    char *incpath = NULL;
    int result = 1;
    if (start && end) {
        size_t len = (size_t)(end - start - 1);
        fname = vc_strndup(start + 1, len);
        incpath = find_include_path(fname, endc, dir, incdirs);
        const char *chosen = incpath;
        vector_t subconds;
        vector_init(&subconds, sizeof(cond_state_t));
        int ok = 1;
        if (is_active(conds)) {
            if (!chosen) {
                fprintf(stderr, "%s: No such file or directory\n", fname);
                ok = 0;
            } else if (!pragma_once_contains(chosen)) {
                if (include_stack_contains(stack, chosen)) {
                    fprintf(stderr, "Include cycle detected: %s\n", chosen);
                    ok = 0;
                } else if (!process_file(chosen, macros, &subconds, out,
                                         incdirs, stack)) {
                    ok = 0;
                }
            }
        } else {
            /* directive ignored when conditions are inactive */
        }
        vector_free(&subconds);
        if (!ok)
            result = 0;
    }
    free(incpath);
    free(fname);
    return result;
}

/* Split a comma separated list of parameters and append trimmed names to out */
static int tokenize_param_list(char *list, vector_t *out)
{
    char *tok; char *sp;
    tok = strtok_r(list, ",", &sp);
    while (tok) {
        while (*tok == ' ' || *tok == '\t')
            tok++;
        char *end = tok + strlen(tok);
        while (end > tok && (end[-1] == ' ' || end[-1] == '\t'))
            end--;
        char *dup = vc_strndup(tok, (size_t)(end - tok));
        if (!vector_push(out, &dup)) {
            free(dup);
            for (size_t i = 0; i < out->count; i++)
                free(((char **)out->data)[i]);
            vector_free(out);
            vector_init(out, sizeof(char *));
            return 0;
        }
        tok = strtok_r(NULL, ",", &sp);
    }
    return 1;
}

/*
 * Parse a comma separated parameter list starting at *p.
 *
 * "p" should point to the character immediately following the macro
 * name.  On success the vector "out" contains the parameter names and
 * the returned pointer points to the character following the closing
 * ')', or the first non-whitespace character when no parameter list
 * was present.  The macro name is NUL-terminated by this routine.
 *
 * On failure NULL is returned.  Any partially collected parameter
 * strings are freed and vector_free() leaves "out" reusable via
 * vector_init().
 */
static char *parse_macro_params(char *p, vector_t *out, int *variadic)
{
    vector_init(out, sizeof(char *));
    if (variadic)
        *variadic = 0;
    if (*p == '(') {
        *p++ = '\0';
        char *start = p;
        while (*p && *p != ')')
            p++;
        if (*p == ')') {
            char *plist = vc_strndup(start, (size_t)(p - start));
            if (!tokenize_param_list(plist, out)) {
                free(plist);
                /* cleanup partially collected parameters */
                free_param_vector(out); /* leaves vector reusable */
                return NULL;
            }
            free(plist);
            p++; /* skip ')' */
        } else {
            p = start - 1; /* restore '(' position */
            *p = '('; /* undo temporary termination */
            free_param_vector(out); /* leaves vector reusable */
            return NULL;
        }
    } else if (*p) {
        *p++ = '\0';
    }
    if (variadic && out->count) {
        size_t last = out->count - 1;
        char *name = ((char **)out->data)[last];
        if (strcmp(name, "...") == 0) {
            *variadic = 1;
            free(name);
            out->count--;
        }
    }
    return p;
}

/**
 * Create a macro definition and append it to the macro table.
 *
 * The strings contained in the supplied "params" vector must be
 * heap allocated.  add_macro() duplicates "name" and "value" and
 * takes ownership of all parameter strings.  On success the newly
 * created macro is stored in "macros" and becomes the caller's
 * responsibility to free using macro_free().
 *
 * Any allocation failure results in all intermediate resources
 * being released and zero being returned.
 */
static int add_macro(const char *name, const char *value, vector_t *params,
                     int variadic, vector_t *macros)
{
    macro_t m;
    m.name = vc_strdup(name);
    m.value = NULL; /* ensure safe cleanup on failure */
    vector_init(&m.params, sizeof(char *));
    for (size_t i = 0; i < params->count; i++) {
        char *pname = ((char **)params->data)[i];
        if (!vector_push(&m.params, &pname)) {
            free(pname);
            for (size_t j = i + 1; j < params->count; j++)
                free(((char **)params->data)[j]);
            vector_free(params);
            macro_free(&m);
            fprintf(stderr, "Out of memory\n");
            return 0;
        }
    }
    vector_free(params);
    m.variadic = variadic;
    m.value = vc_strdup(value);
    if (!vector_push(macros, &m)) {
        for (size_t i = 0; i < m.params.count; i++)
            free(((char **)m.params.data)[i]);
        m.params.count = 0;
        macro_free(&m);
        fprintf(stderr, "Out of memory\n");
        return 0;
    }
    return 1;
}

/*
 * Parse and store a macro definition from a #define directive.
 * When the current conditional stack is active, the macro is added
 * to the macro table for later expansion.
 */
static int handle_define(char *line, vector_t *macros, vector_t *conds)
{
    char *n = line + 7;
    n = skip_ws(n);
    char *name = n;
    while (*n && !isspace((unsigned char)*n) && *n != '(')
        n++;
    vector_t params;
    int variadic = 0;
    n = parse_macro_params(n, &params, &variadic);
    if (!n) {
        free_param_vector(&params);
        fprintf(stderr, "Missing ')' in macro definition\n");
        return 0;
    }
    n = skip_ws(n);
    char *val = *n ? n : "";
    int ok = 1;
    if (is_active(conds)) {
        ok = add_macro(name, val, &params, variadic, macros);
    } else {
        /* directive ignored when conditions are inactive */
        free_param_vector(&params);
    }
    return ok;
}

/* Push a new state for #ifdef/#ifndef directives.  When "neg" is non-zero
 * the condition is inverted as for #ifndef. */
static int cond_push_ifdef_common(char *line, vector_t *macros,
                                  vector_t *conds, int neg)
{
    char *n = line + (neg ? 7 : 6);
    n = skip_ws(n);
    char *id = n;
    while (isalnum((unsigned char)*n) || *n == '_')
        n++;
    *n = '\0';
    cond_state_t st;
    st.parent_active = is_active(conds);
    st.taken = 0;
    int defined = is_macro_defined(macros, id);
    if (st.parent_active && (neg ? !defined : defined)) {
        st.taking = 1;
        st.taken = 1;
    } else {
        st.taking = 0;
    }
    if (!vector_push(conds, &st)) {
        fprintf(stderr, "Out of memory\n");
        return 0;
    }
    return 1;
}

/* Push a new state for an #ifdef directive */
static int cond_push_ifdef(char *line, vector_t *macros, vector_t *conds)
{
    return cond_push_ifdef_common(line, macros, conds, 0);
}

/* Push a new state for an #ifndef directive */
static int cond_push_ifndef(char *line, vector_t *macros, vector_t *conds)
{
    return cond_push_ifdef_common(line, macros, conds, 1);
}

/* Push a new state for a generic #if expression */
static int cond_push_ifexpr(char *line, vector_t *macros, vector_t *conds)
{
    char *expr = line + 3;
    cond_state_t st;
    st.parent_active = is_active(conds);
    st.taken = 0;
    if (st.parent_active && eval_expr(expr, macros)) {
        st.taking = 1;
        st.taken = 1;
    } else {
        st.taking = 0;
    }
    if (!vector_push(conds, &st)) {
        fprintf(stderr, "Out of memory\n");
        return 0;
    }
    return 1;
}

/* Handle an #elif directive */
static void cond_handle_elif(char *line, vector_t *macros, vector_t *conds)
{
    if (!conds->count)
        return;
    cond_state_t *st =
        &((cond_state_t *)conds->data)[conds->count - 1];
    if (st->parent_active) {
        if (st->taken) {
            st->taking = 0;
        } else {
            char *expr = line + 5;
            st->taking = eval_expr(expr, macros);
            if (st->taking)
                st->taken = 1;
        }
    } else {
        st->taking = 0;
    }
}

/* Handle an #else directive */
static void cond_handle_else(vector_t *conds)
{
    if (!conds->count)
        return;
    cond_state_t *st =
        &((cond_state_t *)conds->data)[conds->count - 1];
    if (st->parent_active && !st->taken) {
        st->taking = 1;
        st->taken = 1;
    } else {
        st->taking = 0;
    }
}

/* Handle an #endif directive */
static void cond_handle_endif(vector_t *conds)
{
    if (conds->count)
        conds->count--;
}

/*
 * Dispatch conditional directives to the specific helper handlers.
 */
static int handle_conditional(char *line, vector_t *macros, vector_t *conds)
{
    if (strncmp(line, "#ifdef", 6) == 0 && isspace((unsigned char)line[6])) {
        return cond_push_ifdef(line, macros, conds);
    } else if (strncmp(line, "#ifndef", 7) == 0 &&
               isspace((unsigned char)line[7])) {
        return cond_push_ifndef(line, macros, conds);
    } else if (strncmp(line, "#if", 3) == 0 && isspace((unsigned char)line[3])) {
        return cond_push_ifexpr(line, macros, conds);
    } else if (strncmp(line, "#elif", 5) == 0 &&
               isspace((unsigned char)line[5])) {
        cond_handle_elif(line, macros, conds);
        return 1;
    } else if (strncmp(line, "#else", 5) == 0) {
        cond_handle_else(conds);
        return 1;
    } else if (strncmp(line, "#endif", 6) == 0) {
        cond_handle_endif(conds);
        return 1;
    }
    return 1;
}

/*
 * Append a #pragma directive to the output when the current
 * conditional stack is active.  Pragmas are otherwise ignored.
 */
static int handle_pragma(char *line, vector_t *conds, strbuf_t *out)
{
    if (is_active(conds)) {
        if (strbuf_append(out, line) != 0)
            return 0;
        if (strbuf_append(out, "\n") != 0)
            return 0;
    }
    return 1;
}

/* Small helpers used by process_file */

/*
 * Read a file and split it into NUL terminated lines.  The returned
 * text buffer holds the line data and must remain valid for the lifetime
 * of the line array.  The caller must free both using the cleanup helper.
 * NULL is returned on I/O errors.
 */
static int handle_directive(char *line, const char *dir, vector_t *macros,
                            vector_t *conds, strbuf_t *out,
                            const vector_t *incdirs, vector_t *stack);


/* Free a vector of parameter names */
static void free_param_vector(vector_t *v)
{
    for (size_t i = 0; i < v->count; i++)
        free(((char **)v->data)[i]);
    vector_free(v);
}

/* Iterate over the loaded lines and process each one. */
static int handle_directive_line(char *line, const char *dir, vector_t *macros,
                                 vector_t *conds, strbuf_t *out,
                                 const vector_t *incdirs, vector_t *stack)
{
    line = skip_ws(line);
    return handle_directive(line, dir, macros, conds, out, incdirs, stack);
}

static int process_all_lines(char **lines, const char *path, const char *dir,
                             vector_t *macros, vector_t *conds,
                             strbuf_t *out, const vector_t *incdirs,
                             vector_t *stack)
{
    for (size_t i = 0; lines[i]; i++) {
        preproc_set_location(path, i + 1, 1);
        if (!handle_directive_line(lines[i], dir, macros, conds,
                                   out, incdirs, stack))
            return 0;
    }
    return 1;
}
/* Process a single #include directive and recursively handle the file. */
static int handle_include_directive(char *line, const char *dir,
                                    vector_t *macros, vector_t *conds,
                                    strbuf_t *out,
                                    const vector_t *incdirs,
                                    vector_t *stack)
{
    return handle_include(line, dir, macros, conds, out, incdirs, stack);
}

/* Apply a #line directive to adjust reported line numbers. */
static int handle_line_directive(char *line, const char *dir, vector_t *macros,
                                 vector_t *conds, strbuf_t *out,
                                 const vector_t *incdirs,
                                 vector_t *stack)
{
    (void)dir; (void)macros; (void)incdirs; (void)stack;
    char *p = line + 5;
    p = skip_ws(p);
    errno = 0;
    char *end;
    long long val = strtoll(p, &end, 10);
    if (p == end || errno != 0 || val > INT_MAX || val <= 0) {
        fprintf(stderr, "Invalid line number in #line directive\n");
        return 0;
    }
    p = end;
    int lineno = (int)val;
    p = skip_ws(p);
    char *fname = NULL;
    if (*p == '"') {
        p++;
        char *fstart = p;
        while (*p && *p != '"')
            p++;
        if (*p == '"')
            fname = vc_strndup(fstart, (size_t)(p - fstart));
    }
    if (is_active(conds)) {
        if (strbuf_appendf(out, "# %d", lineno) < 0) {
            free(fname);
            return 0;
        }
        if (fname && strbuf_appendf(out, " \"%s\"", fname) < 0) {
            free(fname);
            return 0;
        }
        if (strbuf_append(out, "\n") < 0) {
            free(fname);
            return 0;
        }
    }
    free(fname);
    return 1;
}

/* Parse and store a macro from a #define directive. */
static int handle_define_directive(char *line, const char *dir,
                                   vector_t *macros, vector_t *conds,
                                   strbuf_t *out,
                                   const vector_t *incdirs,
                                   vector_t *stack)
{
    (void)dir; (void)out; (void)incdirs; (void)stack;
    return handle_define(line, macros, conds);
}

/* Remove a macro defined earlier when #undef is seen. */
static int handle_undef_directive(char *line, const char *dir, vector_t *macros,
                                  vector_t *conds, strbuf_t *out,
                                  const vector_t *incdirs,
                                  vector_t *stack)
{
    (void)dir; (void)out; (void)incdirs; (void)stack;
    char *n = line + 6;
    n = skip_ws(n);
    char *id = n;
    while (isalnum((unsigned char)*n) || *n == '_')
        n++;
    *n = '\0';
    if (is_active(conds))
        remove_macro(macros, id);
    return 1;
}

/* Emit an error message and abort preprocessing when active. */
static int handle_error_directive(char *line, const char *dir,
                                  vector_t *macros, vector_t *conds,
                                  strbuf_t *out,
                                  const vector_t *incdirs,
                                  vector_t *stack)
{
    (void)dir; (void)macros; (void)out; (void)incdirs; (void)stack;
    char *msg = line + 6; /* skip '#error' */
    msg = skip_ws(msg);
    if (is_active(conds)) {
        fprintf(stderr, "%s\n", *msg ? msg : "preprocessor error");
        return 0;
    }
    return 1;
}

/* Copy a #pragma line into the output when active. */
static int handle_pragma_directive(char *line, const char *dir,
                                   vector_t *macros, vector_t *conds,
                                   strbuf_t *out,
                                   const vector_t *incdirs,
                                   vector_t *stack)
{
    (void)dir; (void)macros; (void)incdirs;
    char *p = line + 7; /* skip '#pragma' */
    p = skip_ws(p);
    if (strncmp(p, "once", 4) == 0) {
        p += 4;
        p = skip_ws(p);
        if (*p == '\0' && stack->count) {
            const char *cur = ((const char **)stack->data)[stack->count - 1];
            if (!pragma_once_add(cur))
                return 0;
        }
    }
    (void)stack;
    return handle_pragma(line, conds, out);
}

/* Update conditional state based on #if/#else/#endif directives. */
static int handle_conditional_directive(char *line, const char *dir,
                                        vector_t *macros, vector_t *conds,
                                        strbuf_t *out,
                                        const vector_t *incdirs,
                                        vector_t *stack)
{
    (void)dir; (void)out; (void)incdirs; (void)stack;
    return handle_conditional(line, macros, conds);
}

/*
 * Expand a regular text line and append it to the output when the current
 * conditional stack is active.
 */
static int handle_text_line(char *line, const char *dir, vector_t *macros,
                            vector_t *conds, strbuf_t *out,
                            const vector_t *incdirs, vector_t *stack)
{
    (void)dir; (void)incdirs; (void)stack;
    int ok = 1;
    if (is_active(conds)) {
        strbuf_t tmp;
        strbuf_init(&tmp);
        if (!expand_line(line, macros, &tmp, 0, 0))
            ok = 0;
        else
            strbuf_append(&tmp, "\n");
        if (ok)
            strbuf_append(out, tmp.data);
        strbuf_free(&tmp);
    }
    return ok;
}

/*
 * Handle a preprocessor directive or regular text line.  Dispatches to the
 * specific handler for the directive and falls back to text expansion when the
 * line is not a recognised directive.
 */
typedef int (*directive_fn_t)(char *, const char *, vector_t *, vector_t *,
                              strbuf_t *, const vector_t *, vector_t *);

enum { SPACE_NONE, SPACE_BLANK, SPACE_ANY };

typedef struct {
    const char    *name;
    int            space;
    directive_fn_t handler;
} directive_entry_t;

typedef struct {
    size_t start;
    size_t count;
} directive_bucket_t;

static const directive_entry_t directive_table[] = {
    {"#define",  SPACE_BLANK, handle_define_directive},
    {"#elif",    SPACE_ANY,   handle_conditional_directive},
    {"#else",    SPACE_NONE,  handle_conditional_directive},
    {"#endif",   SPACE_NONE,  handle_conditional_directive},
    {"#error",   SPACE_ANY,   handle_error_directive},
    {"#ifdef",   SPACE_ANY,   handle_conditional_directive},
    {"#ifndef",  SPACE_ANY,   handle_conditional_directive},
    {"#if",      SPACE_ANY,   handle_conditional_directive},
    {"#include", SPACE_BLANK, handle_include_directive},
    {"#line",    SPACE_ANY,   handle_line_directive},
    {"#pragma",  SPACE_ANY,   handle_pragma_directive},
    {"#undef",   SPACE_ANY,   handle_undef_directive},
};

static const directive_bucket_t directive_buckets[26] = {
    ['d' - 'a'] = {0, 1},  /* #define */
    ['e' - 'a'] = {1, 4},  /* #elif, #else, #endif, #error */
    ['i' - 'a'] = {5, 4},  /* #ifdef, #ifndef, #if, #include */
    ['l' - 'a'] = {9, 1},  /* #line */
    ['p' - 'a'] = {10, 1}, /* #pragma */
    ['u' - 'a'] = {11, 1}, /* #undef */
};

static const directive_entry_t *lookup_directive(const char *line)
{
    if (line[0] != '#')
        return NULL;

    char c = line[1];
    if (c < 'a' || c > 'z')
        return NULL;

    directive_bucket_t bucket = directive_buckets[c - 'a'];
    for (size_t i = 0; i < bucket.count; i++) {
        const directive_entry_t *d = &directive_table[bucket.start + i];
        size_t len = strlen(d->name);
        if (strncmp(line, d->name, len) == 0) {
            char next = line[len];
            if (d->space == SPACE_BLANK) {
                if (next == ' ' || next == '\t')
                    return d;
            } else if (d->space == SPACE_ANY) {
                if (isspace((unsigned char)next))
                    return d;
            } else {
                return d;
            }
        }
    }

    return NULL;
}

static int handle_directive(char *line, const char *dir, vector_t *macros,
                            vector_t *conds, strbuf_t *out,
                            const vector_t *incdirs, vector_t *stack)
{
    const directive_entry_t *d = lookup_directive(line);
    if (d)
        return d->handler(line, dir, macros, conds, out, incdirs, stack);

    return handle_text_line(line, dir, macros, conds, out, incdirs, stack);
}

/*
 * Core file processing routine.  Reads the file, handles directives
 * and macro expansion line by line, writing the preprocessed result
 * to the output buffer.
 */
static int process_file(const char *path, vector_t *macros,
                        vector_t *conds, strbuf_t *out,
                        const vector_t *incdirs, vector_t *stack)
{
    if (stack->count >= MAX_INCLUDE_DEPTH) {
        fprintf(stderr, "Include depth limit exceeded\n");
        return 0;
    }
    char **lines;
    char *dir;
    char *text;

    if (!load_source(path, stack, &lines, &dir, &text))
        return 0;

    int ok = process_all_lines(lines, path, dir, macros, conds, out, incdirs,
                               stack);

    include_stack_pop(stack);

    cleanup_source(text, lines, dir);
    return ok;
}

/* Append colon-separated paths from ENV to SEARCH_DIRS */
static int append_env_paths(const char *env, vector_t *search_dirs)
{
    if (!env || !*env)
        return 1;

    char *tmp = vc_strdup(env);
    if (!tmp)
        return 0;
    char *tok, *sp;
    tok = strtok_r(tmp, ":", &sp);
    while (tok) {
        if (*tok) {
            char *dup = vc_strdup(tok);
            if (!dup || !vector_push(search_dirs, &dup)) {
                free(dup);
                free(tmp);
                return 0;
            }
        }
        tok = strtok_r(NULL, ":", &sp);
    }
    free(tmp);
    return 1;
}

/* Collect include search directories from CLI options and environment */
static int collect_include_dirs(vector_t *search_dirs,
                                const vector_t *include_dirs)
{
    vector_init(search_dirs, sizeof(char *));
    for (size_t i = 0; i < include_dirs->count; i++) {
        const char *s = ((const char **)include_dirs->data)[i];
        char *dup = vc_strdup(s);
        if (!dup || !vector_push(search_dirs, &dup)) {
            free(dup);
            free_string_vector(search_dirs);
            return 0;
        }
    }

    if (!append_env_paths(getenv("VCPATH"), search_dirs)) {
        free_string_vector(search_dirs);
        return 0;
    }
    if (!append_env_paths(getenv("VCINC"), search_dirs)) {
        free_string_vector(search_dirs);
        return 0;
    }
    return 1;
}

/* Initialize the vectors used during preprocessing */
static void init_preproc_vectors(vector_t *macros, vector_t *conds,
                                 vector_t *stack, strbuf_t *out)
{
    vector_init(macros, sizeof(macro_t));
    vector_init(conds, sizeof(cond_state_t));
    vector_init(stack, sizeof(char *));
    preproc_include_init();
    strbuf_init(out);
}

/* Release vectors and buffers used during preprocessing */
static void cleanup_preproc_vectors(vector_t *macros, vector_t *conds,
                                    vector_t *stack, vector_t *search_dirs,
                                    strbuf_t *out)
{
    for (size_t i = 0; i < stack->count; i++)
        free(((char **)stack->data)[i]);
    vector_free(stack);
    vector_free(conds);
    free_macro_vector(macros);
    free_string_vector(search_dirs);
    preproc_include_cleanup();
    strbuf_free(out);
}

/* Apply macro definitions specified on the command line */
static int apply_cli_defines(vector_t *macros, const vector_t *defines)
{
    if (!defines)
        return 1;

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
        if (!add_macro(name, val, &params, 0, macros)) {
            free(name);
            return 0;
        }
        free(name);
    }

    return 1;
}

/* Remove macros listed via the command line */
static void apply_cli_undefines(vector_t *macros, const vector_t *undefines)
{
    if (!undefines)
        return;

    for (size_t i = 0; i < undefines->count; i++) {
        const char *name = ((const char **)undefines->data)[i];
        remove_macro(macros, name);
    }
}

/* Wrapper around process_file used by the entry point */
static int process_input_file(const char *path, vector_t *macros,
                              vector_t *conds, strbuf_t *out,
                              const vector_t *incdirs, vector_t *stack)
{
    return process_file(path, macros, conds, out, incdirs, stack);
}

/*
 * Entry point used by the compiler.  Sets up include search paths,
 * invokes the file processor and returns the resulting text.
 */
char *preproc_run(const char *path, const vector_t *include_dirs,
                  const vector_t *defines, const vector_t *undefines)
{
    vector_t search_dirs, macros, conds, stack;
    strbuf_t out;

    /* Build include search list from CLI options and environment */
    if (!collect_include_dirs(&search_dirs, include_dirs))
        return NULL;

    /* Prepare all vectors used during preprocessing */
    init_preproc_vectors(&macros, &conds, &stack, &out);

    /* Import any -D command line definitions */
    if (!apply_cli_defines(&macros, defines)) {
        cleanup_preproc_vectors(&macros, &conds, &stack, &search_dirs, &out);
        return NULL;
    }

    /* Remove macros listed with -U */
    apply_cli_undefines(&macros, undefines);

    /* Process the initial source file */
    int ok = process_input_file(path, &macros, &conds, &out,
                                &search_dirs, &stack);

    char *res = NULL;
    if (ok)
        res = vc_strdup(out.data ? out.data : "");

    cleanup_preproc_vectors(&macros, &conds, &stack, &search_dirs, &out);

    return res;
}

