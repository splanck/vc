#define _POSIX_C_SOURCE 200809L
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "preproc_file.h"
#include "preproc_macros.h"
#include "preproc_expr.h"
#include "util.h"
#include "vector.h"
#include "strbuf.h"

/* Default system include search paths */
static const char *std_include_dirs[] = {
    "/usr/local/include",
    "/usr/include",
    NULL
};

typedef struct {
    int parent_active;
    int taking;
    int taken;
} cond_state_t;

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

/* forward declaration for recursive include handling */
static int process_file(const char *path, vector_t *macros,
                        vector_t *conds, strbuf_t *out,
                        const vector_t *incdirs);

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
*/
static const char *find_include_path(const char *fname, char endc,
                                     const char *dir,
                                     const vector_t *incdirs,
                                     char out_path[512])
{
    if (endc == '"' && dir) {
        snprintf(out_path, 512, "%s%s", dir, fname);
        if (access(out_path, R_OK) == 0)
            return out_path;
    }

    for (size_t i = 0; i < incdirs->count; i++) {
        const char *base = ((const char **)incdirs->data)[i];
        snprintf(out_path, 512, "%s/%s", base, fname);
        if (access(out_path, R_OK) == 0)
            return out_path;
    }

    if (endc == '<') {
        for (size_t i = 0; std_include_dirs[i]; i++) {
            snprintf(out_path, 512, "%s/%s", std_include_dirs[i], fname);
            if (access(out_path, R_OK) == 0)
                return out_path;
        }
        return NULL;
    }

    snprintf(out_path, 512, "%s", fname);
    if (access(out_path, R_OK) == 0)
        return out_path;

    for (size_t i = 0; std_include_dirs[i]; i++) {
        snprintf(out_path, 512, "%s/%s", std_include_dirs[i], fname);
        if (access(out_path, R_OK) == 0)
            return out_path;
    }

    return NULL;
}

/*
 * Process one #include directive.  The file name is resolved using
 * find_include_path() and, when the current conditional stack is
 * active, the referenced file is processed recursively.
 */
static int handle_include(char *line, const char *dir, vector_t *macros,
                          vector_t *conds, strbuf_t *out,
                          const vector_t *incdirs)
{
    char *start = strchr(line, '"');
    char endc = '"';
    if (!start) {
        start = strchr(line, '<');
        endc = '>';
    }
    char *end = start ? strchr(start + 1, endc) : NULL;
    if (start && end) {
        size_t len = (size_t)(end - start - 1);
        char fname[256];
        snprintf(fname, sizeof(fname), "%.*s", (int)len, start + 1);
        char incpath[512];
        const char *chosen = find_include_path(fname, endc, dir, incdirs,
                                               incpath);
        vector_t subconds;
        vector_init(&subconds, sizeof(cond_state_t));
        int ok = 1;
        if (stack_active(conds)) {
            if (!chosen ||
                !process_file(chosen, macros, &subconds, out, incdirs)) {
                if (!chosen)
                    perror(fname);
                ok = 0;
            }
        }
        vector_free(&subconds);
        if (!ok)
            return 0;
    }
    return 1;
}

/*
 * Parse a comma separated parameter list starting at *p.
 *
 * "p" should point to the character immediately following the macro
 * name.  On return the vector "out" contains the parameter names and
 * the returned pointer points to the character following the closing
 * ')', or the first non-whitespace character when no parameter list
 * was present.  The macro name is NUL-terminated by this routine. */
static char *parse_macro_params(char *p, vector_t *out)
{
    vector_init(out, sizeof(char *));
    if (*p == '(') {
        *p++ = '\0';
        char *start = p;
        while (*p && *p != ')')
            p++;
        if (*p == ')') {
            char *plist = vc_strndup(start, (size_t)(p - start));
            char *tok; char *sp;
            tok = strtok_r(plist, ",", &sp);
            while (tok) {
                while (*tok == ' ' || *tok == '\t')
                    tok++;
                char *end = tok + strlen(tok);
                while (end > tok && (end[-1] == ' ' || end[-1] == '\t'))
                    end--;
                char *dup = vc_strndup(tok, (size_t)(end - tok));
                vector_push(out, &dup);
                tok = strtok_r(NULL, ",", &sp);
            }
            free(plist);
            p++; /* skip ')' */
        } else {
            p = start - 1; /* restore '(' position */
            for (size_t i = 0; i < out->count; i++)
                free(((char **)out->data)[i]);
            vector_free(out);
            vector_init(out, sizeof(char *));
        }
    } else if (*p) {
        *p++ = '\0';
    }
    return p;
}

/** Create a macro definition and append it to the macro table.
 *
 * Ownership of any parameter strings is transferred from "params" to
 * the new macro entry.  Returns non-zero on success. */
static int add_macro(const char *name, const char *value, vector_t *params,
                     vector_t *macros)
{
    macro_t m;
    m.name = vc_strdup(name);
    vector_init(&m.params, sizeof(char *));
    for (size_t i = 0; i < params->count; i++) {
        char *pname = ((char **)params->data)[i];
        vector_push(&m.params, &pname);
    }
    vector_free(params);
    m.value = vc_strdup(value);
    if (!vector_push(macros, &m)) {
        macro_free(&m);
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
    while (*n == ' ' || *n == '\t')
        n++;
    char *name = n;
    while (*n && !isspace((unsigned char)*n) && *n != '(')
        n++;
    vector_t params;
    n = parse_macro_params(n, &params);
    while (*n == ' ' || *n == '\t')
        n++;
    char *val = *n ? n : "";
    int ok = 1;
    if (stack_active(conds)) {
        ok = add_macro(name, val, &params, macros);
    } else {
        for (size_t t = 0; t < params.count; t++)
            free(((char **)params.data)[t]);
        vector_free(&params);
    }
    return ok;
}

/* Push a new state for #ifdef/#ifndef directives.  When "neg" is non-zero
 * the condition is inverted as for #ifndef. */
static void cond_push_ifdef_common(char *line, vector_t *macros,
                                   vector_t *conds, int neg)
{
    char *n = line + (neg ? 7 : 6);
    while (*n == ' ' || *n == '\t')
        n++;
    char *id = n;
    while (isalnum((unsigned char)*n) || *n == '_')
        n++;
    *n = '\0';
    cond_state_t st;
    st.parent_active = stack_active(conds);
    st.taken = 0;
    int defined = is_macro_defined(macros, id);
    if (st.parent_active && (neg ? !defined : defined)) {
        st.taking = 1;
        st.taken = 1;
    } else {
        st.taking = 0;
    }
    vector_push(conds, &st);
}

/* Push a new state for an #ifdef directive */
static void cond_push_ifdef(char *line, vector_t *macros, vector_t *conds)
{
    cond_push_ifdef_common(line, macros, conds, 0);
}

/* Push a new state for an #ifndef directive */
static void cond_push_ifndef(char *line, vector_t *macros, vector_t *conds)
{
    cond_push_ifdef_common(line, macros, conds, 1);
}

/* Push a new state for a generic #if expression */
static void cond_push_ifexpr(char *line, vector_t *macros, vector_t *conds)
{
    char *expr = line + 3;
    cond_state_t st;
    st.parent_active = stack_active(conds);
    st.taken = 0;
    if (st.parent_active && eval_expr(expr, macros)) {
        st.taking = 1;
        st.taken = 1;
    } else {
        st.taking = 0;
    }
    vector_push(conds, &st);
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
static void handle_conditional(char *line, vector_t *macros, vector_t *conds)
{
    if (strncmp(line, "#ifdef", 6) == 0 && isspace((unsigned char)line[6])) {
        cond_push_ifdef(line, macros, conds);
    } else if (strncmp(line, "#ifndef", 7) == 0 &&
               isspace((unsigned char)line[7])) {
        cond_push_ifndef(line, macros, conds);
    } else if (strncmp(line, "#if", 3) == 0 && isspace((unsigned char)line[3])) {
        cond_push_ifexpr(line, macros, conds);
    } else if (strncmp(line, "#elif", 5) == 0 &&
               isspace((unsigned char)line[5])) {
        cond_handle_elif(line, macros, conds);
    } else if (strncmp(line, "#else", 5) == 0) {
        cond_handle_else(conds);
    } else if (strncmp(line, "#endif", 6) == 0) {
        cond_handle_endif(conds);
    }
}

/*
 * Append a #pragma directive to the output when the current
 * conditional stack is active.  Pragmas are otherwise ignored.
 */
static void handle_pragma(char *line, vector_t *conds, strbuf_t *out)
{
    if (stack_active(conds)) {
        strbuf_append(out, line);
        strbuf_append(out, "\n");
    }
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
                            const vector_t *incdirs);

static char *read_file_lines(const char *path, char ***out_lines)
{
    char *text = vc_read_file(path);
    if (!text)
        return NULL;

    size_t line_count = 1;
    for (char *p = text; *p; p++)
        if (*p == '\n')
            line_count++;

    char **lines = vc_alloc_or_exit(sizeof(char *) * (line_count + 1));

    char *saveptr;
    char *line = strtok_r(text, "\n", &saveptr);
    size_t idx = 0;
    while (line) {
        lines[idx++] = line;
        line = strtok_r(NULL, "\n", &saveptr);
    }
    lines[idx] = NULL;
    *out_lines = lines;
    return text;
}

/* Process one line of input.  Leading whitespace is skipped before
 * dispatching to the directive handlers. */
static int process_line(char *line, const char *dir, vector_t *macros,
                        vector_t *conds, strbuf_t *out,
                        const vector_t *incdirs)
{
    while (*line == ' ' || *line == '\t')
        line++;
    return handle_directive(line, dir, macros, conds, out, incdirs);
}

/* Free resources allocated by process_file */
static void cleanup_file_resources(char *text, char **lines, char *dir)
{
    free(lines);
    free(text);
    free(dir);
}
/* Process a single #include directive and recursively handle the file. */
static int process_include(char *line, const char *dir, vector_t *macros,
                           vector_t *conds, strbuf_t *out,
                           const vector_t *incdirs)
{
    return handle_include(line, dir, macros, conds, out, incdirs);
}

/* Apply a #line directive to adjust reported line numbers. */
static int process_line_directive(char *line, vector_t *conds, strbuf_t *out)
{
    char *p = line + 5;
    while (*p == ' ' || *p == '\t')
        p++;
    char *start = p;
    while (isdigit((unsigned char)*p))
        p++;
    int lineno = atoi(start);
    while (*p == ' ' || *p == '\t')
        p++;
    char *fname = NULL;
    if (*p == '"') {
        p++;
        char *fstart = p;
        while (*p && *p != '"')
            p++;
        if (*p == '"')
            fname = vc_strndup(fstart, (size_t)(p - fstart));
    }
    if (stack_active(conds)) {
        strbuf_appendf(out, "# %d", lineno);
        if (fname)
            strbuf_appendf(out, " \"%s\"", fname);
        strbuf_append(out, "\n");
    }
    free(fname);
    return 1;
}

/* Parse and store a macro from a #define directive. */
static int process_define(char *line, vector_t *macros, vector_t *conds)
{
    return handle_define(line, macros, conds);
}

/* Remove a macro defined earlier when #undef is seen. */
static int process_undef(char *line, vector_t *macros, vector_t *conds)
{
    char *n = line + 6;
    while (*n == ' ' || *n == '\t')
        n++;
    char *id = n;
    while (isalnum((unsigned char)*n) || *n == '_')
        n++;
    *n = '\0';
    if (stack_active(conds))
        remove_macro(macros, id);
    return 1;
}

/* Copy a #pragma line into the output when active. */
static int process_pragma_line(char *line, vector_t *conds, strbuf_t *out)
{
    handle_pragma(line, conds, out);
    return 1;
}

/* Update conditional state based on #if/#else/#endif directives. */
static int process_conditional_line(char *line, vector_t *macros, vector_t *conds)
{
    handle_conditional(line, macros, conds);
    return 1;
}

/*
 * Expand a regular text line and append it to the output when the current
 * conditional stack is active.
 */
static int process_text_line(char *line, vector_t *macros,
                             vector_t *conds, strbuf_t *out)
{
    if (stack_active(conds)) {
        strbuf_t tmp;
        strbuf_init(&tmp);
        expand_line(line, macros, &tmp);
        strbuf_append(&tmp, "\n");
        strbuf_append(out, tmp.data);
        strbuf_free(&tmp);
    }
    return 1;
}

/*
 * Handle a preprocessor directive or regular text line.  Dispatches to the
 * specific handler for the directive and falls back to text expansion when the
 * line is not a recognised directive.
 */
static int handle_directive(char *line, const char *dir, vector_t *macros,
                            vector_t *conds, strbuf_t *out,
                            const vector_t *incdirs)
{
    if (strncmp(line, "#include", 8) == 0 &&
        (line[8] == ' ' || line[8] == '\t')) {
        return process_include(line, dir, macros, conds, out, incdirs);
    } else if (strncmp(line, "#line", 5) == 0 &&
               isspace((unsigned char)line[5])) {
        return process_line_directive(line, conds, out);
    } else if (strncmp(line, "#define", 7) == 0 &&
               (line[7] == ' ' || line[7] == '\t')) {
        return process_define(line, macros, conds);
    } else if (strncmp(line, "#undef", 6) == 0 &&
               isspace((unsigned char)line[6])) {
        return process_undef(line, macros, conds);
    } else if (strncmp(line, "#pragma", 7) == 0 &&
               isspace((unsigned char)line[7])) {
        return process_pragma_line(line, conds, out);
    } else if (strncmp(line, "#", 1) == 0 &&
               (strncmp(line, "#ifdef", 6) == 0 ||
                strncmp(line, "#ifndef", 7) == 0 ||
                strncmp(line, "#if", 3) == 0 ||
                strncmp(line, "#elif", 5) == 0 ||
                strncmp(line, "#else", 5) == 0 ||
                strncmp(line, "#endif", 6) == 0)) {
        return process_conditional_line(line, macros, conds);
    }
    return process_text_line(line, macros, conds, out);
}

/*
 * Core file processing routine.  Reads the file, handles directives
 * and macro expansion line by line, writing the preprocessed result
 * to the output buffer.
 */
static int process_file(const char *path, vector_t *macros,
                        vector_t *conds, strbuf_t *out,
                        const vector_t *incdirs)
{
    char **lines;
    char *text = read_file_lines(path, &lines);
    if (!text)
        return 0;

    char *dir = NULL;
    const char *slash = strrchr(path, '/');
    if (slash) {
        size_t len = (size_t)(slash - path) + 1;
        dir = vc_strndup(path, len);
    }

    int ok = 1;
    for (size_t i = 0; lines[i]; i++) {
        if (!process_line(lines[i], dir, macros, conds, out, incdirs)) {
            ok = 0;
            break;
        }
    }

    cleanup_file_resources(text, lines, dir);
    return ok;
}

/*
 * Entry point used by the compiler.  Sets up include search paths,
 * invokes the file processor and returns the resulting text.
 */
char *preproc_run(const char *path, const vector_t *include_dirs)
{
    vector_t search_dirs;
    vector_init(&search_dirs, sizeof(char *));
    for (size_t i = 0; i < include_dirs->count; i++) {
        const char *s = ((const char **)include_dirs->data)[i];
        char *dup = vc_strdup(s);
        vector_push(&search_dirs, &dup);
    }

    const char *env = getenv("VCPATH");
    if (env && *env) {
        char *tmp = vc_strdup(env);
        char *tok; char *sp;
        tok = strtok_r(tmp, ":", &sp);
        while (tok) {
            if (*tok) {
                char *dup = vc_strdup(tok);
                vector_push(&search_dirs, &dup);
            }
            tok = strtok_r(NULL, ":", &sp);
        }
        free(tmp);
    }

    vector_t macros;
    vector_init(&macros, sizeof(macro_t));
    vector_t conds;
    vector_init(&conds, sizeof(cond_state_t));
    strbuf_t out;
    strbuf_init(&out);
    int ok = process_file(path, &macros, &conds, &out, &search_dirs);
    vector_free(&conds);
    for (size_t i = 0; i < macros.count; i++)
        macro_free(&((macro_t *)macros.data)[i]);
    vector_free(&macros);
    for (size_t i = 0; i < search_dirs.count; i++)
        free(((char **)search_dirs.data)[i]);
    vector_free(&search_dirs);
    char *res = NULL;
    if (ok)
        res = vc_strdup(out.data ? out.data : "");
    strbuf_free(&out);
    return res;
}

