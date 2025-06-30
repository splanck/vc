#include <stdio.h>
#include <string.h>
#include "parser_core.h"
#include "token.h"
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
int test_vector_push(vector_t *vec, const void *elem)
{
    if (fail_push)
        return 0;
    return vector_push(vec, elem);
}

static void test_param_alloc_fail(void)
{
    const char *src = "int f(int a)"; /* body not needed, param push will fail */
    size_t count = 0;
    token_t *toks = lexer_tokenize(src, &count);
    parser_t p; parser_init(&p, toks, count);
    fail_push = 1;
    func_t *fn = parser_parse_func(&p, 0);
    ASSERT(fn == NULL);
    fail_push = 0;
    lexer_free_tokens(toks, count);
}

int main(void)
{
    test_param_alloc_fail();
    if (failures == 0)
        printf("All parser alloc tests passed\n");
    else
        printf("%d parser alloc test(s) failed\n", failures);
    return failures ? 1 : 0;
}
