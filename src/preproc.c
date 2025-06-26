#define _POSIX_C_SOURCE 200809L
/*
 * Minimal preprocessing implementation.
 *
 * Handles '#include "file"', object-like '#define' and parameterized macros
 * like '#define F(a,b)'.  Macro bodies may reference other macros and will be
 * expanded recursively.  Basic conditional directives '#if', '#ifdef',
 * '#ifndef', '#elif', '#else' and '#endif' are also recognized.  Expansion
 * performs naive text substitution with no awareness of strings or comments.
 * Nested includes are supported but there are no include guards.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "preproc.h"
#include <unistd.h>
#include "util.h"
#include "vector.h"
#include "strbuf.h"

/* Stored macro definition */
typedef struct {
    char *name;
    vector_t params; /* vector of char* parameter names */
    char *value;
} macro_t;

/* Free memory for a macro */
static void macro_free(macro_t *m)
{
    if (!m) return;
    free(m->name);
    for (size_t i = 0; i < m->params.count; i++)
        free(((char **)m->params.data)[i]);
    vector_free(&m->params);
    free(m->value);
}

/* Expand a macro body by substituting parameters with provided args */
static char *expand_params(const char *value, const vector_t *params, char **args)
{
    strbuf_t sb;
    strbuf_init(&sb);
    for (size_t i = 0; value[i];) {
        if (isalpha((unsigned char)value[i]) || value[i] == '_') {
            size_t j = i + 1;
            while (isalnum((unsigned char)value[j]) || value[j] == '_')
                j++;
            size_t len = j - i;
            int done = 0;
            for (size_t p = 0; p < params->count; p++) {
                const char *param = ((char **)params->data)[p];
                if (strlen(param) == len && strncmp(param, value + i, len) == 0) {
                    strbuf_append(&sb, args[p]);
                    done = 1;
                    break;
                }
            }
            if (!done)
                strbuf_appendf(&sb, "%.*s", (int)len, value + i);
            i = j;
        } else {
            strbuf_appendf(&sb, "%c", value[i]);
            i++;
        }
    }
    char *out = vc_strdup(sb.data ? sb.data : "");
    strbuf_free(&sb);
    return out;
}

static void expand_line(const char *line, vector_t *macros, strbuf_t *out)
{
    for (size_t i = 0; line[i];) {
        int replaced = 0;
        if (isalpha((unsigned char)line[i]) || line[i] == '_') {
            size_t j = i + 1;
            while (isalnum((unsigned char)line[j]) || line[j] == '_')
                j++;
            size_t len = j - i;
            for (size_t k = 0; k < macros->count; k++) {
                macro_t *m = &((macro_t *)macros->data)[k];
                if (strlen(m->name) == len && strncmp(m->name, line + i, len) == 0) {
                    if (m->params.count) {
                        size_t p = j;
                        while (line[p] == ' ' || line[p] == '\t')
                            p++;
                        if (line[p] == '(') {
                            p++;
                            size_t start = p;
                            while (line[p] && line[p] != ')')
                                p++;
                            if (line[p] == ')') {
                                char *argstr = vc_strndup(line + start, p - start);
                                vector_t args; vector_init(&args, sizeof(char *));
                                char *tok; char *sp;
                                tok = strtok_r(argstr, ",", &sp);
                                while (tok) {
                                    while (*tok == ' ' || *tok == '\t')
                                        tok++;
                                    char *end = tok + strlen(tok);
                                    while (end > tok && (end[-1] == ' ' || end[-1] == '\t'))
                                        end--;
                                    char *a = vc_strndup(tok, (size_t)(end - tok));
                                    vector_push(&args, &a);
                                    tok = strtok_r(NULL, ",", &sp);
                                }
                                if (args.count == m->params.count) {
                                    char *body = expand_params(m->value, &m->params, (char **)args.data);
                                    strbuf_t tmp; strbuf_init(&tmp);
                                    expand_line(body, macros, &tmp);
                                    strbuf_append(out, tmp.data ? tmp.data : "");
                                    strbuf_free(&tmp);
                                    free(body);
                                    i = p + 1;
                                    replaced = 1;
                                }
                                for (size_t t = 0; t < args.count; t++)
                                    free(((char **)args.data)[t]);
                                vector_free(&args);
                                free(argstr);
                                if (replaced)
                                    break;
                            }
                        }
                        /* fallthrough if no parentheses */
                    } else {
                        strbuf_t tmp; strbuf_init(&tmp);
                        expand_line(m->value, macros, &tmp);
                        strbuf_append(out, tmp.data ? tmp.data : "");
                        strbuf_free(&tmp);
                        i = j;
                        replaced = 1;
                        break;
                    }
                }
            }
        }
        if (!replaced) {
            strbuf_appendf(out, "%c", line[i]);
            i++;
        }
    }
}

/* Process a single file, writing output to 'out'. */
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

static int is_macro_defined(vector_t *macros, const char *name)
{
    for (size_t i = 0; i < macros->count; i++) {
        macro_t *m = &((macro_t *)macros->data)[i];
        if (strcmp(m->name, name) == 0)
            return 1;
    }
    return 0;
}

typedef struct {
    const char *s;
    vector_t *macros;
} expr_ctx_t;

static void skip_ws(expr_ctx_t *ctx)
{
    while (*ctx->s == ' ' || *ctx->s == '\t')
        ctx->s++;
}

static char *parse_ident(expr_ctx_t *ctx)
{
    skip_ws(ctx);
    if (!isalpha((unsigned char)*ctx->s) && *ctx->s != '_')
        return NULL;
    const char *start = ctx->s;
    size_t len = 1;
    ctx->s++;
    while (isalnum((unsigned char)*ctx->s) || *ctx->s == '_') {
        ctx->s++;
        len++;
    }
    return vc_strndup(start, len);
}

static int parse_expr(expr_ctx_t *ctx);

static int parse_primary(expr_ctx_t *ctx)
{
    skip_ws(ctx);
    if (strncmp(ctx->s, "defined", 7) == 0 &&
        (ctx->s[7] == '(' || ctx->s[7] == ' ' || ctx->s[7] == '\t')) {
        ctx->s += 7;
        skip_ws(ctx);
        if (*ctx->s == '(') {
            ctx->s++;
            char *id = parse_ident(ctx);
            skip_ws(ctx);
            if (*ctx->s == ')')
                ctx->s++;
            int val = id ? is_macro_defined(ctx->macros, id) : 0;
            free(id);
            return val;
        } else {
            char *id = parse_ident(ctx);
            int val = id ? is_macro_defined(ctx->macros, id) : 0;
            free(id);
            return val;
        }
    } else if (*ctx->s == '(') {
        ctx->s++;
        int val = parse_expr(ctx);
        skip_ws(ctx);
        if (*ctx->s == ')')
            ctx->s++;
        return val;
    } else if (isdigit((unsigned char)*ctx->s)) {
        int val = 0;
        while (isdigit((unsigned char)*ctx->s)) {
            val = val * 10 + (*ctx->s - '0');
            ctx->s++;
        }
        return val;
    } else {
        char *id = parse_ident(ctx);
        free(id);
        return 0;
    }
}

static int parse_not(expr_ctx_t *ctx)
{
    skip_ws(ctx);
    if (*ctx->s == '!') {
        ctx->s++;
        return !parse_not(ctx);
    }
    return parse_primary(ctx);
}

static int parse_and(expr_ctx_t *ctx)
{
    int val = parse_not(ctx);
    skip_ws(ctx);
    while (strncmp(ctx->s, "&&", 2) == 0) {
        ctx->s += 2;
        int rhs = parse_not(ctx);
        val = val && rhs;
        skip_ws(ctx);
    }
    return val;
}

static int parse_or(expr_ctx_t *ctx)
{
    int val = parse_and(ctx);
    skip_ws(ctx);
    while (strncmp(ctx->s, "||", 2) == 0) {
        ctx->s += 2;
        int rhs = parse_and(ctx);
        val = val || rhs;
        skip_ws(ctx);
    }
    return val;
}

static int parse_expr(expr_ctx_t *ctx)
{
    return parse_or(ctx) != 0;
}

static int eval_expr(const char *s, vector_t *macros)
{
    expr_ctx_t ctx = { s, macros };
    return parse_expr(&ctx);
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
        if (strncmp(line, "#include", 8) == 0 && (line[8] == ' ' || line[8] == '\t')) {
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
                    const char *sysdirs[] = {"/usr/local/include", "/usr/include", NULL};
                    for (size_t i = 0; sysdirs[i] && !chosen; i++) {
                        snprintf(incpath, sizeof(incpath), "%s/%s", sysdirs[i], fname);
                        if (access(incpath, R_OK) == 0)
                            chosen = incpath;
                    }
                }
                if (!chosen && endc == '"') {
                    snprintf(incpath, sizeof(incpath), "%s", fname);
                    if (access(incpath, R_OK) == 0)
                        chosen = incpath;
                }
                vector_t subconds;
                vector_init(&subconds, sizeof(cond_state_t));
                if (stack_active(conds)) {
                    if (!chosen || !process_file(chosen, macros, &subconds, out, incdirs)) {
                        if (!chosen)
                            perror(fname);
                        vector_free(&subconds);
                        free(text);
                        free(dir);
                        return 0;
                    }
                }
                vector_free(&subconds);
            }
        } else if (strncmp(line, "#define", 7) == 0 && (line[7] == ' ' || line[7] == '\t')) {
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
                        char *p = vc_strndup(tok, (size_t)(end - tok));
                        vector_push(&params, &p);
                        tok = strtok_r(NULL, ",", &sp);
                    }
                    free(plist);
                    n++; /* skip ')' */
                } else {
                    /* malformed macro, treat as object-like */
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
                    char *p = ((char **)params.data)[t];
                    vector_push(&m.params, &p); /* take ownership */
                }
                vector_free(&params);
                m.value = vc_strdup(val);
                if (!vector_push(macros, &m)) {
                    macro_free(&m);
                    free(text);
                    free(dir);
                    return 0;
                }
            } else {
                for (size_t t = 0; t < params.count; t++)
                    free(((char **)params.data)[t]);
                vector_free(&params);
            }
        } else if (strncmp(line, "#ifdef", 6) == 0 && isspace((unsigned char)line[6])) {
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
        } else if (strncmp(line, "#ifndef", 7) == 0 && isspace((unsigned char)line[7])) {
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
        } else if (strncmp(line, "#elif", 5) == 0 && isspace((unsigned char)line[5])) {
            if (conds->count) {
                cond_state_t *st = &((cond_state_t *)conds->data)[conds->count - 1];
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
                cond_state_t *st = &((cond_state_t *)conds->data)[conds->count - 1];
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
        line = strtok_r(NULL, "\n", &saveptr);
    }
    free(text);
    free(dir);
    return 1;
}

/* Entry point: preprocess the file and return a newly allocated buffer. */
char *preproc_run(const char *path, const vector_t *include_dirs)
{
    vector_t macros;
    vector_init(&macros, sizeof(macro_t));
    vector_t conds;
    vector_init(&conds, sizeof(cond_state_t));
    strbuf_t out;
    strbuf_init(&out);
    int ok = process_file(path, &macros, &conds, &out, include_dirs);
    vector_free(&conds);
    for (size_t i = 0; i < macros.count; i++)
        macro_free(&((macro_t *)macros.data)[i]);
    vector_free(&macros);
    char *res = NULL;
    if (ok)
        res = vc_strdup(out.data ? out.data : "");
    strbuf_free(&out);
    return res;
}

