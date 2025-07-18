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

    ASSERT(eval_expr_full("defined FOO", &macros, NULL, NULL, NULL) == 0);
    ASSERT(eval_expr_full("(11 << 16) + 1 >= (10 << 16) + 1", &macros,
                         NULL, NULL, NULL));
    ASSERT(eval_expr_full("199309L >= 2 || 0", &macros, NULL, NULL, NULL));
    ASSERT(eval_expr_full("1 ? 2 : 3", &macros, NULL, NULL, NULL));
    ASSERT(eval_expr_full("0 ? 2 : 3", &macros, NULL, NULL, NULL));

    vector_free(&macros);
}

static void test_large_constants(void)
{
    vector_t macros; vector_init(&macros, sizeof(macro_t));

    ASSERT(eval_expr_full("4294967296", &macros, NULL, NULL, NULL) == 4294967296LL);
    ASSERT(eval_expr_full("9223372036854775807", &macros, NULL, NULL, NULL) == 9223372036854775807LL);
    ASSERT(eval_expr_full("-9223372036854775807 - 1", &macros, NULL, NULL, NULL) == (-9223372036854775807LL - 1LL));

    vector_free(&macros);
}

static void test_shift_clamp(void)
{
    vector_t macros; vector_init(&macros, sizeof(macro_t));

    ASSERT(eval_expr_full("1 << 70", &macros, NULL, NULL, NULL) == (long long)(1ULL << 63));
    ASSERT(eval_expr_full("8 >> 70", &macros, NULL, NULL, NULL) == 0);
    ASSERT(eval_expr_full("1 << -1", &macros, NULL, NULL, NULL) == 1);
    ASSERT(eval_expr_full("8 >> -2", &macros, NULL, NULL, NULL) == 8);

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
