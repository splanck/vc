#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vector.h"
#include "util.h"

/* Minimal helpers from preproc_file.c needed for testing */
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
            if (!tokenize_param_list(plist, out)) {
                free(plist);
                return NULL;
            }
            free(plist);
            p++; /* skip ')' */
        } else {
            p = start - 1; /* restore '(' position */
            *p = '('; /* undo temporary termination */
            for (size_t i = 0; i < out->count; i++)
                free(((char **)out->data)[i]);
            vector_free(out);
            return NULL;
        }
    } else if (*p) {
        *p++ = '\0';
    }
    return p;
}

static int failures = 0;
#define ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "Assertion failed: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
        failures++; \
    } \
} while (0)

static void test_invalid_params(void)
{
    char line[] = "(x, y"; /* missing closing parenthesis */
    vector_t v;
    char *res = parse_macro_params(line, &v);
    ASSERT(res == NULL);         /* failure detected */
    ASSERT(line[0] == '(');      /* '(' restored */
    ASSERT(v.count == 0);        /* vector reset */
    ASSERT(v.cap == 0);
    vector_free(&v);
}

int main(void)
{
    test_invalid_params();
    if (failures == 0)
        printf("All invalid macro tests passed\n");
    else
        printf("%d invalid macro test(s) failed\n", failures);
    return failures ? 1 : 0;
}
