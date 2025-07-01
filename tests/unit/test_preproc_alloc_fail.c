#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vector.h"

static int failures = 0;
#define ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "Assertion failed: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
        failures++; \
    } \
} while (0)

extern int vector_push(vector_t *vec, const void *elem); /* real impl */
static int fail_at = 0;
static int call_count = 0;
int test_vector_push(vector_t *vec, const void *elem)
{
    call_count++;
    if (fail_at && call_count == fail_at)
        return 0;
    return vector_push(vec, elem);
}

static int allocs = 0;
static char *xstrndup(const char *s, size_t n)
{
    size_t len = strlen(s);
    if (len > n)
        len = n;
    char *p = malloc(len + 1);
    if (!p) {
        perror("malloc");
        exit(1);
    }
    memcpy(p, s, len);
    p[len] = '\0';
    allocs++;
    return p;
}

static void xfree(void *p)
{
    if (p) {
        allocs--;
        free(p);
    }
}

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
        char *dup = xstrndup(tok, (size_t)(end - tok));
        if (!test_vector_push(out, &dup)) {
            xfree(dup);
            for (size_t i = 0; i < out->count; i++)
                xfree(((char **)out->data)[i]);
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
            char *plist = xstrndup(start, (size_t)(p - start));
            if (!tokenize_param_list(plist, out)) {
                xfree(plist);
                for (size_t i = 0; i < out->count; i++)
                    xfree(((char **)out->data)[i]);
                vector_free(out);
                return NULL;
            }
            xfree(plist);
            p++;
        } else {
            p = start - 1;
            *p = '(';
            for (size_t i = 0; i < out->count; i++)
                xfree(((char **)out->data)[i]);
            vector_free(out);
            return NULL;
        }
    } else if (*p) {
        *p++ = '\0';
    }
    return p;
}

static void test_fail_first(void)
{
    vector_t v;
    vector_init(&v, sizeof(char *));
    call_count = 0;
    fail_at = 1; /* fail on first push */
    char list[] = "a,b";
    ASSERT(!tokenize_param_list(list, &v));
    ASSERT(v.count == 0 && v.cap == 0);
    ASSERT(allocs == 0);
    vector_free(&v);
}

static void test_fail_second(void)
{
    vector_t v;
    vector_init(&v, sizeof(char *));
    call_count = 0;
    fail_at = 2; /* fail on second push */
    char list[] = "a,b";
    ASSERT(!tokenize_param_list(list, &v));
    ASSERT(v.count == 0 && v.cap == 0);
    ASSERT(allocs == 0);
    vector_free(&v);
}

static void test_parse_fail(void)
{
    vector_t v;
    call_count = 0;
    fail_at = 2; /* fail while tokenizing parameters */
    char line[] = "(a,b)";
    ASSERT(parse_macro_params(line, &v) == NULL);
    ASSERT(v.count == 0 && v.cap == 0);
    ASSERT(allocs == 0);
    vector_free(&v);
}

int main(void)
{
    test_fail_first();
    test_fail_second();
    test_parse_fail();
    if (failures == 0)
        printf("All preproc alloc tests passed\n");
    else
        printf("%d preproc alloc test(s) failed\n", failures);
    return failures ? 1 : 0;
}
