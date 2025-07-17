#include <stdio.h>
#include "preproc_expr_parse.h"
#include "preproc_macros.h"
#include "strbuf.h"
#include "vector.h"
#include "util.h"

static int failures = 0;
#define ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "Assertion failed: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
        failures++; \
    } \
} while (0)

static char *test_vc_strndup(const char *s, size_t n) { (void)s; (void)n; return NULL; }
static int test_expand_line(const char *l, vector_t *m, strbuf_t *o, size_t c, int d, preproc_context_t *ctx) { (void)l;(void)m;(void)o;(void)c;(void)d;(void)ctx; return 1; }
static char *test_expr_parse_header_name(expr_ctx_t *ctx, char *endc) { (void)ctx;(void)endc; return NULL; }
static char *test_find_include_path(const char *f, char e, const char *d, const vector_t *i, size_t s, size_t *o) { (void)f;(void)e;(void)d;(void)i;(void)s;(void)o; return NULL; }
void vc_oom(void) { }
static void test_strbuf_init(strbuf_t *b) { (void)b; }
static void test_strbuf_free(strbuf_t *b) { (void)b; }
static char *test_expr_parse_ident(expr_ctx_t *ctx) { (void)ctx; return NULL; }
static int test_expr_parse_char_escape(const char **p) { (void)p; return 0; }
static int test_is_macro_defined(vector_t *m, const char *n) { (void)m;(void)n; return 0; }

#define vc_strndup test_vc_strndup
#define expand_line test_expand_line
#define expr_parse_header_name test_expr_parse_header_name
#define find_include_path test_find_include_path
#define strbuf_init test_strbuf_init
#define strbuf_free test_strbuf_free
#define expr_parse_ident test_expr_parse_ident
#define expr_parse_char_escape test_expr_parse_char_escape
#define is_macro_defined test_is_macro_defined
#include "../../src/preproc_expr_parse.c"

int main(void) {
    expr_ctx_t ctx = { "__has_include(foo)", NULL, NULL, NULL, NULL, 0 };
    long long val = parse_has_include(&ctx, 0);
    ASSERT(val == 0);
    ASSERT(ctx.error);
    if (failures == 0)
        printf("All preproc_has_include_alloc_fail tests passed\n");
    else
        printf("%d preproc_has_include_alloc_fail test(s) failed\n", failures);
    return failures ? 1 : 0;
}
