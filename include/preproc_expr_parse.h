#ifndef VC_PREPROC_EXPR_PARSE_H
#define VC_PREPROC_EXPR_PARSE_H

#include "vector.h"
#include "preproc_macros.h"

/* Parser context used for expression evaluation */
typedef struct {
    const char *s;
    vector_t *macros;
    const char *dir;
    const vector_t *incdirs;
    vector_t *stack;
    int error;
} expr_ctx_t;

long long parse_expr(expr_ctx_t *ctx);
long long parse_conditional(expr_ctx_t *ctx);

#endif /* VC_PREPROC_EXPR_PARSE_H */
