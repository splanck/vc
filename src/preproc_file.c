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
 * Process one #include directive.  Searches the provided include
 * directories as well as standard paths and recursively processes
 * the chosen file when the current conditional stack is active.
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
        const char *chosen = NULL;
        if (endc == '"' && dir) {
            snprintf(incpath, sizeof(incpath), "%s%s", dir, fname);
            if (access(incpath, R_OK) == 0)
                chosen = incpath;
        }
        if (!chosen) {
            for (size_t i = 0; i < incdirs->count && !chosen; i++) {
                const char *base = ((const char **)incdirs->data)[i];
                snprintf(incpath, sizeof(incpath), "%s/%s", base, fname);
                if (access(incpath, R_OK) == 0)
                    chosen = incpath;
            }
        }
        if (!chosen && endc == '<') {
            for (size_t i = 0; std_include_dirs[i] && !chosen; i++) {
                snprintf(incpath, sizeof(incpath), "%s/%s",
                         std_include_dirs[i], fname);
                if (access(incpath, R_OK) == 0)
                    chosen = incpath;
            }
        }
        if (!chosen && endc == '"') {
            snprintf(incpath, sizeof(incpath), "%s", fname);
            if (access(incpath, R_OK) == 0)
                chosen = incpath;
            else {
                for (size_t i = 0; std_include_dirs[i] && !chosen; i++) {
                    snprintf(incpath, sizeof(incpath), "%s/%s",
                             std_include_dirs[i], fname);
                    if (access(incpath, R_OK) == 0)
                        chosen = incpath;
                }
            }
        }
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
    vector_init(&params, sizeof(char *));
    if (*n == '(') {
        *n++ = '\0';
        char *pstart = n;
        while (*n && *n != ')')
            n++;
        if (*n == ')') {
            char *plist = vc_strndup(pstart, (size_t)(n - pstart));
            char *tok; char *sp;
            tok = strtok_r(plist, ",", &sp);
            while (tok) {
                while (*tok == ' ' || *tok == '\t')
                    tok++;
                char *end = tok + strlen(tok);
                while (end > tok && (end[-1] == ' ' || end[-1] == '\t'))
                    end--;
                char *pname = vc_strndup(tok, (size_t)(end - tok));
                vector_push(&params, &pname);
                tok = strtok_r(NULL, ",", &sp);
            }
            free(plist);
            n++; /* skip ')' */
        } else {
            n = pstart - 1; /* restore '(' position */
            for (size_t t = 0; t < params.count; t++)
                free(((char **)params.data)[t]);
            vector_free(&params);
            vector_init(&params, sizeof(char *));
        }
    } else if (*n) {
        *n++ = '\0';
    }
    while (*n == ' ' || *n == '\t')
        n++;
    char *val = *n ? n : "";
    if (stack_active(conds)) {
        macro_t m;
        m.name = vc_strdup(name);
        vector_init(&m.params, sizeof(char *));
        for (size_t t = 0; t < params.count; t++) {
            char *pname = ((char **)params.data)[t];
            vector_push(&m.params, &pname);
        }
        vector_free(&params);
        m.value = vc_strdup(val);
        if (!vector_push(macros, &m)) {
            macro_free(&m);
            return 0;
        }
    } else {
        for (size_t t = 0; t < params.count; t++)
            free(((char **)params.data)[t]);
        vector_free(&params);
    }
    return 1;
}

/*
 * Update the conditional state stack for directives such as
 * #if, #ifdef, #elif, #else and #endif.
 */
static void handle_conditional(char *line, vector_t *macros, vector_t *conds)
{
    if (strncmp(line, "#ifdef", 6) == 0 && isspace((unsigned char)line[6])) {
        char *n = line + 6;
        while (*n == ' ' || *n == '\t')
            n++;
        char *id = n;
        while (isalnum((unsigned char)*n) || *n == '_')
            n++;
        *n = '\0';
        cond_state_t st;
        st.parent_active = stack_active(conds);
        st.taken = 0;
        if (st.parent_active && is_macro_defined(macros, id)) {
            st.taking = 1;
            st.taken = 1;
        } else {
            st.taking = 0;
        }
        vector_push(conds, &st);
    } else if (strncmp(line, "#ifndef", 7) == 0 &&
               isspace((unsigned char)line[7])) {
        char *n = line + 7;
        while (*n == ' ' || *n == '\t')
            n++;
        char *id = n;
        while (isalnum((unsigned char)*n) || *n == '_')
            n++;
        *n = '\0';
        cond_state_t st;
        st.parent_active = stack_active(conds);
        st.taken = 0;
        if (st.parent_active && !is_macro_defined(macros, id)) {
            st.taking = 1;
            st.taken = 1;
        } else {
            st.taking = 0;
        }
        vector_push(conds, &st);
    } else if (strncmp(line, "#if", 3) == 0 && isspace((unsigned char)line[3])) {
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
    } else if (strncmp(line, "#elif", 5) == 0 &&
               isspace((unsigned char)line[5])) {
        if (conds->count) {
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
    } else if (strncmp(line, "#else", 5) == 0) {
        if (conds->count) {
            cond_state_t *st =
                &((cond_state_t *)conds->data)[conds->count - 1];
            if (st->parent_active && !st->taken) {
                st->taking = 1;
                st->taken = 1;
            } else {
                st->taking = 0;
            }
        }
    } else if (strncmp(line, "#endif", 6) == 0) {
        if (conds->count)
            conds->count--;
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
static int process_include_line(char *line, const char *dir, vector_t *macros,
                                vector_t *conds, strbuf_t *out,
                                const vector_t *incdirs)
{
    return handle_include(line, dir, macros, conds, out, incdirs);
}

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

static int process_define_line(char *line, vector_t *macros, vector_t *conds)
{
    return handle_define(line, macros, conds);
}

static int process_undef_line(char *line, vector_t *macros, vector_t *conds)
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

static int process_pragma_line(char *line, vector_t *conds, strbuf_t *out)
{
    handle_pragma(line, conds, out);
    return 1;
}

static int process_conditional_line(char *line, vector_t *macros, vector_t *conds)
{
    handle_conditional(line, macros, conds);
    return 1;
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
    char *text = vc_read_file(path);
    if (!text)
        return 0;
    char *dir = NULL;
    const char *slash = strrchr(path, '/');
    if (slash) {
        size_t len = (size_t)(slash - path) + 1;
        dir = vc_strndup(path, len);
    }

    char *saveptr;
    char *line = strtok_r(text, "\n", &saveptr);
    while (line) {
        while (*line == ' ' || *line == '\t')
            line++;
        int ok = 1;
        if (strncmp(line, "#include", 8) == 0 &&
            (line[8] == ' ' || line[8] == '\t')) {
            ok = process_include_line(line, dir, macros, conds, out, incdirs);
        } else if (strncmp(line, "#line", 5) == 0 &&
                   isspace((unsigned char)line[5])) {
            ok = process_line_directive(line, conds, out);
        } else if (strncmp(line, "#define", 7) == 0 &&
                   (line[7] == ' ' || line[7] == '\t')) {
            ok = process_define_line(line, macros, conds);
        } else if (strncmp(line, "#undef", 6) == 0 &&
                   isspace((unsigned char)line[6])) {
            ok = process_undef_line(line, macros, conds);
        } else if (strncmp(line, "#pragma", 7) == 0 &&
                   isspace((unsigned char)line[7])) {
            ok = process_pragma_line(line, conds, out);
        } else if (strncmp(line, "#", 1) == 0 &&
                   (strncmp(line, "#ifdef", 6) == 0 ||
                    strncmp(line, "#ifndef", 7) == 0 ||
                    strncmp(line, "#if", 3) == 0 ||
                    strncmp(line, "#elif", 5) == 0 ||
                    strncmp(line, "#else", 5) == 0 ||
                    strncmp(line, "#endif", 6) == 0)) {
            ok = process_conditional_line(line, macros, conds);
        } else {
            if (stack_active(conds)) {
                strbuf_t tmp;
                strbuf_init(&tmp);
                expand_line(line, macros, &tmp);
                strbuf_append(&tmp, "\n");
                strbuf_append(out, tmp.data);
                strbuf_free(&tmp);
            }
        }
        if (!ok) {
            free(text);
            free(dir);
            return 0;
        }
        line = strtok_r(NULL, "\n", &saveptr);
    }
    free(text);
    free(dir);
    return 1;
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

