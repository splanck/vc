#ifndef VC_PREPROC_EXPR_LEX_H
#define VC_PREPROC_EXPR_LEX_H

#include "preproc_expr_parse.h"

char *expr_parse_ident(expr_ctx_t *ctx);
int expr_parse_char_escape(const char **s);
char *expr_parse_header_name(expr_ctx_t *ctx, char *endc);

#endif /* VC_PREPROC_EXPR_LEX_H */
