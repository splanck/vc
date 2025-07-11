#define _POSIX_C_SOURCE 200809L
/*
 * Macro table management and expansion logic.
 *
 * This module stores macro definitions and performs textual replacement
 * during preprocessing.  Macros may be object-like or take parameters,
 * supporting the `#` stringize and `##` token pasting operators.  Macro
 * bodies are expanded recursively so definitions can reference other macros.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "preproc_macros.h"
#include "preproc_cond.h"
#include <stdio.h>
#include "util.h"
#include "vector.h"
#include "strbuf.h"
/*
 * Release all memory associated with a macro definition.
 *
 * The macro must have been created by add_macro() which allocates
 * the name and value strings and takes ownership of all parameter
 * names.  macro_free() disposes of these heap allocations and resets
 * the internal vector.  It is safe to pass NULL.
 */
void macro_free(macro_t *m)
{
    if (!m)
        return;
    free(m->name);
    for (size_t i = 0; i < m->params.count; i++)
        free(((char **)m->params.data)[i]);
    vector_free(&m->params);
    free(m->value);
}
/*
 * Return non-zero if a macro with the given name exists in the
 * macro vector.
 */
int is_macro_defined(vector_t *macros, const char *name)
{
    if (strcmp(name, "__FILE__") == 0 || strcmp(name, "__LINE__") == 0 ||
        strcmp(name, "__DATE__") == 0 || strcmp(name, "__TIME__") == 0 ||
        strcmp(name, "__STDC__") == 0 || strcmp(name, "__STDC_VERSION__") == 0 ||
        strcmp(name, "__func__") == 0 || strcmp(name, "offsetof") == 0)
        return 1;

    for (size_t i = 0; i < macros->count; i++) {
        macro_t *m = &((macro_t *)macros->data)[i];
        if (strcmp(m->name, name) == 0)
            return 1;
    }
    return 0;
}

/*
 * Delete all macros matching the given name from the macro list.
 *
 * Each removed entry is cleaned up with macro_free().  The order of the
 * remaining macros is preserved.
 */
void remove_macro(vector_t *macros, const char *name)
{
    for (size_t i = 0; i < macros->count;) {
        macro_t *m = &((macro_t *)macros->data)[i];
        if (strcmp(m->name, name) == 0) {
            macro_free(m);
            if (i + 1 < macros->count)
                memmove(m, m + 1, (macros->count - i - 1) * sizeof(macro_t));
            macros->count--;
        } else {
            i++;
        }
    }
}

/* Advance P past spaces and tabs and return the updated pointer */
static char *skip_ws(char *p)
{
    while (*p == ' ' || *p == '\t')
        p++;
    return p;
}

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

/* Free a vector of parameter names */
static void free_param_vector(vector_t *v)
{
    for (size_t i = 0; i < v->count; i++)
        free(((char **)v->data)[i]);
    vector_free(v);
}

/* Split a comma separated list of parameters and append trimmed names */
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

/* Parse a comma separated parameter list starting at *p */
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
                free_param_vector(out);
                return NULL;
            }
            free(plist);
            p++;
        } else {
            p = start - 1;
            *p = '(';
            free_param_vector(out);
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

int add_macro(const char *name, const char *value, vector_t *params,
              int variadic, vector_t *macros)
{
    macro_t m;
    m.name = vc_strdup(name);
    m.value = NULL;
    vector_init(&m.params, sizeof(char *));
    for (size_t i = 0; i < params->count; i++) {
        char *pname = ((char **)params->data)[i];
        if (!vector_push(&m.params, &pname)) {
            free(pname);
            for (size_t j = i + 1; j < params->count; j++)
                free(((char **)params->data)[j]);
            vector_free(params);
            macro_free(&m);
            vc_oom();
            return 0;
        }
    }
    vector_free(params);
    m.variadic = variadic;
    m.value = vc_strdup(value);
    m.expanding = 0;
    if (!vector_push(macros, &m)) {
        for (size_t i = 0; i < m.params.count; i++)
            free(((char **)m.params.data)[i]);
        m.params.count = 0;
        macro_free(&m);
        vc_oom();
        return 0;
    }
    return 1;
}

int handle_define(char *line, vector_t *macros, vector_t *conds)
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
        remove_macro(macros, name);
        ok = add_macro(name, val, &params, variadic, macros);
    } else {
        free_param_vector(&params);
    }
    return ok;
}

int handle_define_directive(char *line, const char *dir, vector_t *macros,
                            vector_t *conds, strbuf_t *out,
                            const vector_t *incdirs, vector_t *stack,
                            preproc_context_t *ctx)
{
    (void)dir; (void)out; (void)incdirs; (void)stack; (void)ctx;
    return handle_define(line, macros, conds);
}
