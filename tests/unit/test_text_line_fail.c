#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include "strbuf.h"

static int failures = 0;
#define ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "Assertion failed: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
        failures++; \
    } \
} while (0)

extern int strbuf_append(strbuf_t *sb, const char *text); /* real impl */
static int fail_at = 0;
static int call_count = 0;
int test_strbuf_append(strbuf_t *sb, const char *text)
{
    call_count++;
    if (fail_at && call_count == fail_at)
        return -1;
    return strbuf_append(sb, text);
}

/* simplified version of handle_text_line from preproc_directives.c */
static int handle_text_line_sim(const char *line, strbuf_t *out)
{
    int ok = 1;
    strbuf_t tmp;
    strbuf_init(&tmp);
    /* simulate expand_line output */
    strbuf_appendf(&tmp, "%s", line);
    if (test_strbuf_append(&tmp, "\n") != 0)
        ok = 0;
    if (ok && test_strbuf_append(out, tmp.data) != 0)
        ok = 0;
    strbuf_free(&tmp);
    return ok;
}

static void test_fail_newline(void)
{
    strbuf_t out; strbuf_init(&out);
    call_count = 0; fail_at = 1; /* fail on newline append */
    ASSERT(!handle_text_line_sim("x", &out));
    strbuf_free(&out);
}

static void test_fail_output(void)
{
    strbuf_t out; strbuf_init(&out);
    call_count = 0; fail_at = 2; /* fail on output append */
    ASSERT(!handle_text_line_sim("x", &out));
    strbuf_free(&out);
}

int main(void)
{
    test_fail_newline();
    test_fail_output();
    if (failures == 0)
        printf("All text_line_fail tests passed\n");
    else
        printf("%d text_line_fail test(s) failed\n", failures);
    return failures ? 1 : 0;
}
