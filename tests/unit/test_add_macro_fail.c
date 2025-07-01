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
static int fail_push = 0;
#undef vector_push
int test_vector_push(vector_t *vec, const void *elem)
{
    if (fail_push)
        return 0;
    return vector_push(vec, elem);
}

static int allocs = 0;
static char *xstrdup(const char *s)
{
    char *p = strdup(s);
    if (!p) {
        perror("strdup");
        exit(1);
    }
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

/* simplified macro structure */
typedef struct {
    char *name;
    vector_t params; /* vector of char* */
    char *value;
} macro_t;

static void macro_free(macro_t *m)
{
    if (!m)
        return;
    xfree(m->name);
    for (size_t i = 0; i < m->params.count; i++)
        xfree(((char **)m->params.data)[i]);
    vector_free(&m->params);
    xfree(m->value);
}

static int add_macro(const char *name, const char *value,
                     vector_t *params, vector_t *macros)
{
    macro_t m;
    m.name = xstrdup(name);
    m.value = NULL;
    vector_init(&m.params, sizeof(char *));
    for (size_t i = 0; i < params->count; i++) {
        char *pname = ((char **)params->data)[i];
        if (!test_vector_push(&m.params, &pname)) {
            xfree(pname);
            for (size_t j = i + 1; j < params->count; j++)
                xfree(((char **)params->data)[j]);
            vector_free(params);
            macro_free(&m);
            return 0;
        }
    }
    vector_free(params);
    m.value = xstrdup(value);
    if (!test_vector_push(macros, &m)) {
        for (size_t i = 0; i < m.params.count; i++)
            xfree(((char **)m.params.data)[i]);
        m.params.count = 0;
        macro_free(&m);
        return 0;
    }
    return 1;
}

static void test_fail_push(void)
{
    vector_t params;
    vector_init(&params, sizeof(char *));
    char *p = xstrdup("x");
    ASSERT(test_vector_push(&params, &p));

    vector_t macros;
    vector_init(&macros, sizeof(macro_t));

    fail_push = 1;
    ASSERT(!add_macro("M", "1", &params, &macros));
    fail_push = 0;

    ASSERT(macros.count == 0);
    ASSERT(allocs == 0);
    vector_free(&macros);
}

int main(void)
{
    test_fail_push();
    if (failures == 0)
        printf("All add_macro tests passed\n");
    else
        printf("%d add_macro test(s) failed\n", failures);
    return failures ? 1 : 0;
}
