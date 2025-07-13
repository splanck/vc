#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <ctype.h>
#include "preproc_cond.h"
#include "preproc_macros.h"
#include "preproc_expr.h"
#include "preproc_builtin.h"
#include "util.h"
#include "vector.h"
#include "strbuf.h"
#include <string.h>
#include <stdlib.h>
#include "preproc_utils.h"


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


/* Push a new state for #ifdef/#ifndef directives.  When "neg" is non-zero
 * the condition is inverted as for #ifndef. */
int cond_push_ifdef_common(char *line, vector_t *macros,
                           vector_t *conds, int neg,
                           preproc_context_t *ctx)
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
    st.line = preproc_get_line(ctx);
    int defined = is_macro_defined(macros, id);
    if (st.parent_active && (neg ? !defined : defined)) {
        st.taking = 1;
        st.taken = 1;
    } else {
        st.taking = 0;
    }
    if (!vector_push(conds, &st)) {
        vc_oom();
        return 0;
    }
    return 1;
}

/* Push a new state for an #ifdef directive */
int cond_push_ifdef(char *line, vector_t *macros, vector_t *conds,
                    preproc_context_t *ctx)
{
    return cond_push_ifdef_common(line, macros, conds, 0, ctx);
}

/* Push a new state for an #ifndef directive */
int cond_push_ifndef(char *line, vector_t *macros, vector_t *conds,
                     preproc_context_t *ctx)
{
    return cond_push_ifdef_common(line, macros, conds, 1, ctx);
}

/* Expand macros in a conditional expression except within defined() */
static int expand_cond_expr(const char *expr, vector_t *macros, strbuf_t *out,
                            preproc_context_t *ctx)
{
    strbuf_init(out);
    const char *p = expr;
    while (*p) {
        if (strncmp(p, "defined", 7) == 0 &&
            (p == expr || !(isalnum((unsigned char)p[-1]) || p[-1] == '_')) &&
            (p[7] == '(' || p[7] == ' ' || p[7] == '\t')) {
            if (strbuf_append(out, "defined") < 0)
                return 0;
            p += 7;
            while (*p == ' ' || *p == '\t') {
                if (strbuf_appendf(out, "%c", *p) < 0)
                    return 0;
                p++;
            }
            if (*p == '(') {
                if (strbuf_append(out, "(") < 0)
                    return 0;
                p++;
                while (*p == ' ' || *p == '\t') {
                    if (strbuf_appendf(out, "%c", *p) < 0)
                        return 0;
                    p++;
                }
                const char *id = p;
                while (isalnum((unsigned char)*p) || *p == '_')
                    p++;
                if (strbuf_appendf(out, "%.*s", (int)(p - id), id) < 0)
                    return 0;
                while (*p == ' ' || *p == '\t') {
                    if (strbuf_appendf(out, "%c", *p) < 0)
                        return 0;
                    p++;
                }
                if (*p == ')') {
                    if (strbuf_append(out, ")") < 0)
                        return 0;
                    p++;
                }
            } else {
                while (*p == ' ' || *p == '\t') {
                    if (strbuf_appendf(out, "%c", *p) < 0)
                        return 0;
                    p++;
                }
                const char *id = p;
                while (isalnum((unsigned char)*p) || *p == '_')
                    p++;
                if (strbuf_appendf(out, "%.*s", (int)(p - id), id) < 0)
                    return 0;
            }
            continue;
        }
        const char *start = p;
        while (*p && !(strncmp(p, "defined", 7) == 0 &&
                       (p == expr || !(isalnum((unsigned char)p[-1]) || p[-1] == '_')) &&
                       (p[7] == '(' || p[7] == ' ' || p[7] == '\t')))
            p++;
        if (p > start) {
            char *seg = vc_strndup(start, (size_t)(p - start));
            if (!seg)
                return 0;
            strbuf_t tmp;
            strbuf_init(&tmp);
            if (!expand_line(seg, macros, &tmp, 0, 0, ctx)) {
                free(seg);
                strbuf_free(&tmp);
                return 0;
            }
            if (strbuf_append(out, tmp.data ? tmp.data : "") < 0) {
                free(seg);
                strbuf_free(&tmp);
                return 0;
            }
            free(seg);
            strbuf_free(&tmp);
        }
    }
    return 1;
}

/* Push a new state for a generic #if expression */
int cond_push_ifexpr(char *line, const char *dir, vector_t *macros,
                     vector_t *conds, const vector_t *incdirs, vector_t *stack,
                     preproc_context_t *ctx)
{
    char *expr = line + 3;
    strbuf_t tmp;
    if (!expand_cond_expr(expr, macros, &tmp, ctx))
        return 0;
    expr = tmp.data ? tmp.data : "";
    cond_state_t st;
    st.parent_active = is_active(conds);
    st.taken = 0;
    st.line = preproc_get_line(ctx);
    if (st.parent_active &&
        eval_expr_full(expr, macros, dir, incdirs, stack) != 0) {
        st.taking = 1;
        st.taken = 1;
    } else {
        st.taking = 0;
    }
    strbuf_free(&tmp);
    if (!vector_push(conds, &st)) {
        vc_oom();
        return 0;
    }
    return 1;
}

/* Handle an #elif directive */
void cond_handle_elif(char *line, const char *dir, vector_t *macros,
                      vector_t *conds, const vector_t *incdirs,
                      vector_t *stack, preproc_context_t *ctx)
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
            strbuf_t tmp;
            if (!expand_cond_expr(expr, macros, &tmp, ctx)) {
                st->taking = 0;
                return;
            }
            expr = tmp.data ? tmp.data : "";
            st->taking = eval_expr_full(expr, macros, dir, incdirs, stack) != 0;
            strbuf_free(&tmp);
            if (st->taking)
                st->taken = 1;
        }
    } else {
        st->taking = 0;
    }
}

/* Handle an #else directive */
void cond_handle_else(vector_t *conds)
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
void cond_handle_endif(vector_t *conds)
{
    if (conds->count)
        conds->count--;
}

/* Dispatch conditional directives to the specific helper handlers. */
int handle_conditional(char *line, const char *dir, vector_t *macros,
                       vector_t *conds, const vector_t *incdirs,
                       vector_t *stack, preproc_context_t *ctx)
{
    if (strncmp(line, "#ifdef", 6) == 0 && isspace((unsigned char)line[6])) {
        return cond_push_ifdef(line, macros, conds, ctx);
    } else if (strncmp(line, "#ifndef", 7) == 0 &&
               isspace((unsigned char)line[7])) {
        return cond_push_ifndef(line, macros, conds, ctx);
    } else if (strncmp(line, "#if", 3) == 0 && isspace((unsigned char)line[3])) {
        return cond_push_ifexpr(line, dir, macros, conds, incdirs, stack, ctx);
    } else if (strncmp(line, "#elif", 5) == 0 &&
               isspace((unsigned char)line[5])) {
        cond_handle_elif(line, dir, macros, conds, incdirs, stack, ctx);
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
