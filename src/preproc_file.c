#define _POSIX_C_SOURCE 200809L
/*
 * File reading and directive processing for the preprocessor.
 *
 * Part of vc under the BSD 2-Clause license.
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

/* handle a single #include directive */
static const char *locate_include(const char *name, char endc, const char *dir,
                                  const vector_t *incdirs, char *buf,
                                  size_t bufsz)
{
    if (endc == '"' && dir) {
        snprintf(buf, bufsz, "%s%s", dir, name);
        if (access(buf, R_OK) == 0)
            return buf;
    }
    for (size_t i = 0; i < incdirs->count; i++) {
        const char *base = ((const char **)incdirs->data)[i];
        snprintf(buf, bufsz, "%s/%s", base, name);
        if (access(buf, R_OK) == 0)
            return buf;
    }
    if (endc == '<') {
        for (size_t i = 0; std_include_dirs[i]; i++) {
            snprintf(buf, bufsz, "%s/%s", std_include_dirs[i], name);
            if (access(buf, R_OK) == 0)
                return buf;
        }
    } else {
        snprintf(buf, bufsz, "%s", name);
        if (access(buf, R_OK) == 0)
            return buf;
        for (size_t i = 0; std_include_dirs[i]; i++) {
            snprintf(buf, bufsz, "%s/%s", std_include_dirs[i], name);
            if (access(buf, R_OK) == 0)
                return buf;
        }
    }
    return NULL;
}

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
        char path[512];
        const char *chosen = locate_include(fname, endc, dir, incdirs,
                                            path, sizeof(path));
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
        return ok;
    }
    return 1;
}

/* handle a single #define directive */
static void parse_params(char **p, vector_t *params)
{
    char *start = ++(*p);
    while (**p && **p != ')')
        (*p)++;
    if (**p == ')') {
        char *list = vc_strndup(start, (size_t)(*p - start));
        char *tok, *sp;
        for (tok = strtok_r(list, ",", &sp); tok; tok = strtok_r(NULL, ",", &sp)) {
            while (isspace((unsigned char)*tok))
                tok++;
            char *end = tok + strlen(tok);
            while (end > tok && isspace((unsigned char)end[-1]))
                end--;
            char *name = vc_strndup(tok, (size_t)(end - tok));
            vector_push(params, &name);
        }
        free(list);
        (*p)++;
    } else {
        *p = start - 1;
    }
}

static int handle_define(char *line, vector_t *macros, vector_t *conds)
{
    char *p = line + 7;
    while (isspace((unsigned char)*p))
        p++;
    char *name = p;
    while (*p && !isspace((unsigned char)*p) && *p != '(')
        p++;
    vector_t params;
    vector_init(&params, sizeof(char *));
    if (*p == '(')
        parse_params(&p, &params);
    else if (*p)
        *p++ = '\0';
    while (isspace((unsigned char)*p))
        p++;
    char *val = vc_strdup(*p ? p : "");
    if (!stack_active(conds)) {
        free(val);
        for (size_t i = 0; i < params.count; i++)
            free(((char **)params.data)[i]);
        vector_free(&params);
        return 1;
    }
    macro_t m;
    m.name = vc_strdup(name);
    vector_init(&m.params, sizeof(char *));
    for (size_t i = 0; i < params.count; i++) {
        char *pr = ((char **)params.data)[i];
        vector_push(&m.params, &pr);
    }
    vector_free(&params);
    m.value = val;
    if (!vector_push(macros, &m)) {
        macro_free(&m);
        return 0;
    }
    return 1;
}

/* handle conditional directives like #if/#else/#endif */
static void push_cond(vector_t *conds, int take)
{
    cond_state_t st;
    st.parent_active = stack_active(conds);
    st.taken = 0;
    if (st.parent_active && take) {
        st.taking = 1;
        st.taken = 1;
    } else {
        st.taking = 0;
    }
    vector_push(conds, &st);
}

static void handle_ifdef(char *line, vector_t *macros, vector_t *conds, int neg)
{
    char *p = line + (neg ? 7 : 6);
    while (*p == ' ' || *p == '\t')
        p++;
    char *id = p;
    while (isalnum((unsigned char)*p) || *p == '_')
        p++;
    *p = '\0';
    int cond = is_macro_defined(macros, id);
    push_cond(conds, neg ? !cond : cond);
}

static void handle_if(char *line, vector_t *macros, vector_t *conds)
{
    push_cond(conds, eval_expr(line + 3, macros));
}

static void handle_elif(char *line, vector_t *macros, vector_t *conds)
{
    if (!conds->count)
        return;
    cond_state_t *st = &((cond_state_t *)conds->data)[conds->count - 1];
    if (!st->parent_active) {
        st->taking = 0;
        return;
    }
    if (st->taken) {
        st->taking = 0;
    } else {
        st->taking = eval_expr(line + 5, macros);
        if (st->taking)
            st->taken = 1;
    }
}

static void handle_else(vector_t *conds)
{
    if (conds->count) {
        cond_state_t *st = &((cond_state_t *)conds->data)[conds->count - 1];
        if (st->parent_active && !st->taken) {
            st->taking = 1;
            st->taken = 1;
        } else {
            st->taking = 0;
        }
    }
}

static void handle_endif(vector_t *conds)
{
    if (conds->count)
        conds->count--;
}

static void handle_conditional(char *line, vector_t *macros, vector_t *conds)
{
    if (strncmp(line, "#ifdef", 6) == 0 && isspace((unsigned char)line[6]))
        handle_ifdef(line, macros, conds, 0);
    else if (strncmp(line, "#ifndef", 7) == 0 && isspace((unsigned char)line[7]))
        handle_ifdef(line, macros, conds, 1);
    else if (strncmp(line, "#if", 3) == 0 && isspace((unsigned char)line[3]))
        handle_if(line, macros, conds);
    else if (strncmp(line, "#elif", 5) == 0 && isspace((unsigned char)line[5]))
        handle_elif(line, macros, conds);
    else if (strncmp(line, "#else", 5) == 0)
        handle_else(conds);
    else if (strncmp(line, "#endif", 6) == 0)
        handle_endif(conds);
}

/* handle a #pragma directive (currently passed through) */
static void handle_pragma(char *line, vector_t *conds, strbuf_t *out)
{
    if (stack_active(conds)) {
        strbuf_append(out, line);
        strbuf_append(out, "\n");
    }
}

static void handle_line_directive(char *line, vector_t *conds, strbuf_t *out)
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
        char *fstart = ++p;
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
}

static void handle_undef(char *line, vector_t *macros, vector_t *conds)
{
    char *p = line + 6;
    while (*p == ' ' || *p == '\t')
        p++;
    char *id = p;
    while (isalnum((unsigned char)*p) || *p == '_')
        p++;
    *p = '\0';
    if (stack_active(conds))
        remove_macro(macros, id);
}

static void handle_plain_line(char *line, vector_t *macros, vector_t *conds,
                              strbuf_t *out)
{
    if (!stack_active(conds))
        return;
    strbuf_t tmp;
    strbuf_init(&tmp);
    expand_line(line, macros, &tmp);
    strbuf_append(&tmp, "\n");
    strbuf_append(out, tmp.data);
    strbuf_free(&tmp);
}

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
        if (strncmp(line, "#include", 8) == 0 &&
            (line[8] == ' ' || line[8] == '\t')) {
            if (!handle_include(line, dir, macros, conds, out, incdirs)) {
                free(text);
                free(dir);
                return 0;
            }
        } else if (strncmp(line, "#line", 5) == 0 &&
                   isspace((unsigned char)line[5])) {
            handle_line_directive(line, conds, out);
        } else if (strncmp(line, "#define", 7) == 0 &&
                   (line[7] == ' ' || line[7] == '\t')) {
            if (!handle_define(line, macros, conds)) {
                free(text);
                free(dir);
                return 0;
            }
        } else if (strncmp(line, "#undef", 6) == 0 &&
                   isspace((unsigned char)line[6])) {
            handle_undef(line, macros, conds);
        } else if (strncmp(line, "#pragma", 7) == 0 &&
                   isspace((unsigned char)line[7])) {
            handle_pragma(line, conds, out);
        } else if (strncmp(line, "#", 1) == 0 &&
                   (strncmp(line, "#ifdef", 6) == 0 ||
                    strncmp(line, "#ifndef", 7) == 0 ||
                    strncmp(line, "#if", 3) == 0 ||
                    strncmp(line, "#elif", 5) == 0 ||
                    strncmp(line, "#else", 5) == 0 ||
                    strncmp(line, "#endif", 6) == 0)) {
            handle_conditional(line, macros, conds);
        } else {
            handle_plain_line(line, macros, conds, out);
        }
        line = strtok_r(NULL, "\n", &saveptr);
    }
    free(text);
    free(dir);
    return 1;
}

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

