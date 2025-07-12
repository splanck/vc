#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "preproc_macros.h"
#include "strbuf.h"
#include "vector.h"

static int failures = 0;
#define ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "Assertion failed: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
        failures++; \
    } \
} while (0)

static void add_echo_macro(vector_t *macros)
{
    vector_t params;
    vector_init(&params, sizeof(char *));
    char *p = strdup("x");
    vector_push(&params, &p);
    add_macro("ECHO", "x", &params, 0, macros);
}

static void run_case(const char *call, const char *expect)
{
    vector_t macros; vector_init(&macros, sizeof(macro_t));
    add_echo_macro(&macros);

    strbuf_t sb; strbuf_init(&sb);
    preproc_context_t ctx = {0};
    preproc_set_location(&ctx, "t.c", 1, 1);
    ASSERT(expand_line(call, &macros, &sb, 0, 0, &ctx));
    ASSERT(strcmp(sb.data, expect) == 0);
    strbuf_free(&sb);

    macro_free(&((macro_t *)macros.data)[0]);
    vector_free(&macros);
}

int main(void)
{
    run_case("ECHO(\"a,b\")", "\"a,b\"");
    run_case("ECHO(')')", "')'");
    run_case("ECHO(',')", "','");
    run_case("ECHO(\"a\\\"b,\")", "\"a\\\"b,\"");
    if (failures == 0)
        printf("All preproc_literal_args tests passed\n");
    else
        printf("%d preproc_literal_args test(s) failed\n", failures);
    return failures ? 1 : 0;
}
