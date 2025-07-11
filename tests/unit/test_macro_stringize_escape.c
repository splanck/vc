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

static void add_str_macro(vector_t *macros)
{
    vector_t params;
    vector_init(&params, sizeof(char *));
    char *p = strdup("x");
    vector_push(&params, &p);
    add_macro("STR", "#x", &params, 0, macros);
}

int main(void)
{
    vector_t macros; vector_init(&macros, sizeof(macro_t));
    add_str_macro(&macros);

    strbuf_t sb; strbuf_init(&sb);
    preproc_set_location("t.c", 1, 1);
    ASSERT(expand_line("STR(\"a\\\"b\\\\c\")", &macros, &sb, 0, 0));
    ASSERT(strcmp(sb.data, "\"\\\"a\\\\\\\"b\\\\\\\\c\\\"\"") == 0);
    strbuf_free(&sb);

    macro_free(&((macro_t *)macros.data)[0]);
    vector_free(&macros);

    if (failures == 0)
        printf("All macro_stringize_escape tests passed\n");
    else
        printf("%d macro_stringize_escape test(s) failed\n", failures);
    return failures ? 1 : 0;
}
