#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "preproc_macros.h"
#include "strbuf.h"
#include "vector.h"

static int failures = 0;
#define ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "Assertion failed: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
        failures++; \
    } \
} while (0)

/* simplified tokenizer from preproc_file.c */
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
        char *dup = strndup(tok, (size_t)(end - tok));
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

static char *parse_macro_params(char *p, vector_t *out, int *variadic)
{
    vector_init(out, sizeof(char *));
    *variadic = 0;
    if (*p == '(') {
        *p++ = '\0';
        char *start = p;
        while (*p && *p != ')')
            p++;
        if (*p == ')') {
            char *plist = strndup(start, (size_t)(p - start));
            if (!tokenize_param_list(plist, out)) {
                free(plist);
                for (size_t i = 0; i < out->count; i++)
                    free(((char **)out->data)[i]);
                vector_free(out);
                return NULL;
            }
            free(plist);
            p++; /* skip ')' */
        } else {
            p = start - 1;
            *p = '(';
            for (size_t i = 0; i < out->count; i++)
                free(((char **)out->data)[i]);
            vector_free(out);
            return NULL;
        }
    } else if (*p) {
        *p++ = '\0';
    }
    if (out->count) {
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

static void test_parse_variadic(void)
{
    char line[] = "MAC(x, ...) rest";
    char *p = strchr(line, '(');
    vector_t params;
    int variadic;
    char *res = parse_macro_params(p, &params, &variadic);
    ASSERT(res && variadic);
    ASSERT(params.count == 1);
    ASSERT(strcmp(((char **)params.data)[0], "x") == 0);
    for (size_t i = 0; i < params.count; i++)
        free(((char **)params.data)[i]);
    vector_free(&params);
}

static void test_variadic_expand(void)
{
    vector_t macros;
    vector_init(&macros, sizeof(macro_t));
    macro_t m;
    m.name = strdup("LOG");
    vector_init(&m.params, sizeof(char *));
    char *p = strdup("fmt");
    vector_push(&m.params, &p);
    m.variadic = 1;
    m.value = strdup("printf(fmt, __VA_ARGS__)");
    vector_push(&macros, &m);

    strbuf_t sb;
    strbuf_init(&sb);
    preproc_set_location("t.c",1,1);
    ASSERT(expand_line("LOG(\"%d\", 1)", &macros, &sb, 0, 0));
    ASSERT(strcmp(sb.data, "printf(\"%d\", 1)") == 0);
    strbuf_free(&sb);

    macro_free(&((macro_t *)macros.data)[0]);
    vector_free(&macros);
}

int main(void)
{
    test_parse_variadic();
    test_variadic_expand();
    if (failures == 0)
        printf("All variadic macro tests passed\n");
    else
        printf("%d variadic macro test(s) failed\n", failures);
    return failures ? 1 : 0;
}
