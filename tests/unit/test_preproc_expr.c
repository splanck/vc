#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "preproc_expr.h"
#include "preproc_macros.h"
#include "vector.h"

static int failures = 0;
#define ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "Assertion failed: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
        failures++; \
    } \
} while (0)

static void push_macro(vector_t *macros, const char *name, const char *value)
{
    macro_t m;
    m.name = strdup(name);
    vector_init(&m.params, sizeof(char *));
    m.variadic = 0;
    m.value = strdup(value);
    vector_push(macros, &m);
}

static void free_macros(vector_t *macros)
{
    for (size_t i = 0; i < macros->count; i++)
        macro_free(&((macro_t *)macros->data)[i]);
    vector_free(macros);
}

static void test_features_expr(void)
{
    vector_t macros; vector_init(&macros, sizeof(macro_t));

    ASSERT(eval_expr("defined FOO", &macros) == 0);
    ASSERT(eval_expr("(11 << 16) + 1 >= (10 << 16) + 1", &macros));
    ASSERT(eval_expr("199309L >= 2 || 0", &macros));
    ASSERT(eval_expr("1 ? 2 : 3", &macros));
    ASSERT(eval_expr("0 ? 2 : 3", &macros));

    vector_free(&macros);
}

static void test_large_constants(void)
{
    vector_t macros; vector_init(&macros, sizeof(macro_t));

    ASSERT(eval_expr("4294967296", &macros) == 4294967296LL);
    ASSERT(eval_expr("9223372036854775807", &macros) == 9223372036854775807LL);
    ASSERT(eval_expr("-9223372036854775807 - 1", &macros) == (-9223372036854775807LL - 1LL));

    vector_free(&macros);
}

static void test_shift_clamp(void)
{
    vector_t macros; vector_init(&macros, sizeof(macro_t));

    ASSERT(eval_expr("1 << 70", &macros) == (long long)(1ULL << 63));
    ASSERT(eval_expr("8 >> 70", &macros) == 0);
    ASSERT(eval_expr("1 << -1", &macros) == 1);
    ASSERT(eval_expr("8 >> -2", &macros) == 8);

    vector_free(&macros);
}

int main(void)
{
    test_features_expr();
    test_large_constants();
    test_shift_clamp();
    if (failures == 0)
        printf("All preproc_expr tests passed\n");
    else
        printf("%d preproc_expr test(s) failed\n", failures);
    return failures ? 1 : 0;
}
