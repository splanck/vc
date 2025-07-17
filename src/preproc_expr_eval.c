#include <stdio.h>
#include "preproc_expr.h"
#include "preproc_expr_parse.h"
#include "preproc_utils.h"

static long long eval_internal(const char *s, vector_t *macros,
                               const char *dir, const vector_t *incdirs,
                               vector_t *stack)
{
    expr_ctx_t ctx = { s, macros, dir, incdirs, stack, 0 };
    long long val = parse_expr(&ctx);
    ctx.s = skip_ws((char *)ctx.s);
    if (*ctx.s != '\0')
        ctx.error = 1;
    if (ctx.error) {
        fprintf(stderr, "Invalid preprocessor expression\n");
        return 0;
    }
    return val;
}

long long eval_expr_full(const char *s, vector_t *macros,
                         const char *dir, const vector_t *incdirs,
                         vector_t *stack)
{
    return eval_internal(s, macros, dir, incdirs, stack);
}

