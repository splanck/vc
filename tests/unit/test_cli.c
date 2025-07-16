#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <limits.h>
#include "cli.h"
#include "vector.h"

static int failures = 0;
#define ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "Assertion failed: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
        failures++; \
    } \
} while (0)

/* wrapper for vector_push allowing failure injection */
extern int vector_push(vector_t *vec, const void *elem); /* real impl */
static int fail_push = 0;
int test_vector_push(vector_t *vec, const void *elem)
{
    if (fail_push)
        return 0;
    return vector_push(vec, elem);
}

/* malloc wrappers to track leaks */
extern void *malloc(size_t size); /* real malloc */
extern void *calloc(size_t nmemb, size_t size); /* real calloc */
extern void *realloc(void *ptr, size_t size); /* real realloc */
extern void free(void *ptr); /* real free */
static int allocs = 0;

void *test_malloc(size_t size)
{
    void *p = malloc(size);
    if (p)
        allocs++;
    return p;
}

void *test_calloc(size_t nmemb, size_t size)
{
    if (size && nmemb > SIZE_MAX / size)
        return NULL;
    void *p = calloc(nmemb, size);
    if (p)
        allocs++;
    return p;
}

void *test_realloc(void *ptr, size_t size)
{
    if (!ptr)
        return test_malloc(size);
    void *p = realloc(ptr, size);
    if (p && p != ptr) {
        allocs++;
        allocs--; /* previous ptr freed */
    }
    return p;
}

void test_free(void *ptr)
{
    if (ptr)
        allocs--;
    free(ptr);
}

static void test_parse_success(void)
{
    cli_options_t opts;
    char *argv[] = {"vc", "-o", "out.s", "file.c", NULL};
    int ret = cli_parse_args(4, argv, &opts);
    ASSERT(ret == 0);
    ASSERT(opts.sources.count == 1);
    ASSERT(strcmp(((char **)opts.sources.data)[0], "file.c") == 0);
    ASSERT(opts.asm_syntax == ASM_ATT);
    cli_free_opts(&opts);
    ASSERT(allocs == 0);
}

static void test_intel_syntax_option(void)
{
    cli_options_t opts;
    char *argv[] = {"vc", "--intel-syntax", "-o", "out.s", "file.c", NULL};
    int ret = cli_parse_args(5, argv, &opts);
    ASSERT(ret == 0);
    ASSERT(opts.asm_syntax == ASM_INTEL);
    cli_free_opts(&opts);
    ASSERT(allocs == 0);
}

static void test_dump_ast_option(void)
{
    cli_options_t opts;
    char *argv[] = {"vc", "--dump-ast", "file.c", NULL};
    int ret = cli_parse_args(3, argv, &opts);
    ASSERT(ret == 0);
    ASSERT(opts.dump_ast);
    cli_free_opts(&opts);
    ASSERT(allocs == 0);
}

static void test_dump_tokens_option(void)
{
    cli_options_t opts;
    char *argv[] = {"vc", "--dump-tokens", "file.c", NULL};
    int ret = cli_parse_args(3, argv, &opts);
    ASSERT(ret == 0);
    ASSERT(opts.dump_tokens);
    cli_free_opts(&opts);
    ASSERT(allocs == 0);
}

static void test_verbose_includes_option(void)
{
    cli_options_t opts;
    char *argv[] = {"vc", "--verbose-includes", "--preprocess", "file.c", NULL};
    int ret = cli_parse_args(4, argv, &opts);
    ASSERT(ret == 0);
    ASSERT(opts.verbose_includes);
    cli_free_opts(&opts);
    ASSERT(allocs == 0);
}

static void test_parse_failure(void)
{
    cli_options_t opts;
    char *argv[] = {"vc", "-o", "out.s", "file.c", NULL};
    /* reset getopt state before reusing cli_parse_args */
    optind = 1;
#ifdef __BSD_VISIBLE
    optreset = 1;
#endif
    fail_push = 1;
    FILE *tmp = tmpfile();
    if (!tmp) {
        perror("tmpfile");
        exit(1);
    }
    int saved = dup(fileno(stderr));
    dup2(fileno(tmp), fileno(stderr));

    int ret = cli_parse_args(4, argv, &opts);

    fflush(stderr);
    fseek(tmp, 0, SEEK_SET);
    char buf[256];
    size_t n = fread(buf, 1, sizeof(buf) - 1, tmp);
    buf[n] = '\0';

    dup2(saved, fileno(stderr));
    close(saved);
    fclose(tmp);
    fail_push = 0;

    ASSERT(ret != 0);
    ASSERT(strstr(buf, "Out of memory") != NULL);
    ASSERT(allocs == 0);
}

static void test_internal_libc_leak(void)
{
    cli_options_t opts;
    char *argv[] = {"vc", "--internal-libc", "-o", "out.o", "file.c", NULL};
    int ret = cli_parse_args(5, argv, &opts);
    ASSERT(ret == 0);
    ASSERT(opts.internal_libc);
    ASSERT(opts.vc_sysinclude != NULL);
    cli_free_opts(&opts);
    ASSERT(allocs == 0);
}

static void test_vcflags_quotes(void)
{
    cli_options_t opts;
    setenv("VCFLAGS", "--intel-syntax --output 'out file.s'", 1);
    char *argv[] = {"vc", "file.c", NULL};
    int ret = cli_parse_args(2, argv, &opts);
    unsetenv("VCFLAGS");
    ASSERT(ret == 0);
    ASSERT(opts.asm_syntax == ASM_INTEL);
    ASSERT(strcmp(opts.output, "out file.s") == 0);
    cli_free_opts(&opts);
    ASSERT(allocs == 0);
}

static void test_vcflags_backslash(void)
{
    cli_options_t opts;
    setenv("VCFLAGS", "--intel-syntax --output out\\ file.s", 1);
    char *argv[] = {"vc", "file.c", NULL};
    int ret = cli_parse_args(2, argv, &opts);
    unsetenv("VCFLAGS");
    ASSERT(ret == 0);
    ASSERT(opts.asm_syntax == ASM_INTEL);
    ASSERT(strcmp(opts.output, "out file.s") == 0);
    cli_free_opts(&opts);
    ASSERT(allocs == 0);
}

static void test_shortcut_quotes(void)
{
    cli_options_t opts;
    char *argv1[] = {"vc", "\"-MD\"", "file.c", NULL};
    int ret = cli_parse_args(3, argv1, &opts);
    ASSERT(ret == 0);
    ASSERT(opts.deps);
    ASSERT(!opts.dep_only);
    cli_free_opts(&opts);
    ASSERT(allocs == 0);

    char *argv2[] = {"vc", "'-M'", "file.c", NULL};
    ret = cli_parse_args(3, argv2, &opts);
    ASSERT(ret == 0);
    ASSERT(opts.dep_only);
    ASSERT(!opts.deps);
    cli_free_opts(&opts);
    ASSERT(allocs == 0);
}

int main(void)
{
    test_parse_success();
    test_intel_syntax_option();
    test_dump_ast_option();
    test_dump_tokens_option();
    test_verbose_includes_option();
    test_internal_libc_leak();
    test_vcflags_quotes();
    test_vcflags_backslash();
    test_shortcut_quotes();
    test_parse_failure();
    if (failures == 0)
        printf("All cli tests passed\n");
    else
        printf("%d cli test(s) failed\n", failures);
    return failures ? 1 : 0;
}
