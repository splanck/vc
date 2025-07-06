#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
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
}

static void test_intel_syntax_option(void)
{
    cli_options_t opts;
    char *argv[] = {"vc", "--intel-syntax", "-o", "out.s", "file.c", NULL};
    int ret = cli_parse_args(5, argv, &opts);
    ASSERT(ret == 0);
    ASSERT(opts.asm_syntax == ASM_INTEL);
    cli_free_opts(&opts);
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
}

static void test_missing_define_arg(void)
{
    cli_options_t opts;
    char *argv[] = {"vc", "-D", "", "-o", "out.s", "file.c", NULL};
    /* reset getopt state before reusing cli_parse_args */
    optind = 1;
#ifdef __BSD_VISIBLE
    optreset = 1;
#endif
    FILE *tmp = tmpfile();
    if (!tmp) {
        perror("tmpfile");
        exit(1);
    }
    int saved = dup(fileno(stderr));
    dup2(fileno(tmp), fileno(stderr));

    int ret = cli_parse_args(6, argv, &opts);

    fflush(stderr);
    fseek(tmp, 0, SEEK_SET);
    char buf[256];
    size_t n = fread(buf, 1, sizeof(buf) - 1, tmp);
    buf[n] = '\0';

    dup2(saved, fileno(stderr));
    close(saved);
    fclose(tmp);

    ASSERT(ret != 0);
    ASSERT(strstr(buf, "Missing argument for -D option") != NULL);
}

static void test_missing_undef_arg(void)
{
    cli_options_t opts;
    char *argv[] = {"vc", "-U", "", "-o", "out.s", "file.c", NULL};
    optind = 1;
#ifdef __BSD_VISIBLE
    optreset = 1;
#endif
    FILE *tmp = tmpfile();
    if (!tmp) {
        perror("tmpfile");
        exit(1);
    }
    int saved = dup(fileno(stderr));
    dup2(fileno(tmp), fileno(stderr));

    int ret = cli_parse_args(6, argv, &opts);

    fflush(stderr);
    fseek(tmp, 0, SEEK_SET);
    char buf[256];
    size_t n = fread(buf, 1, sizeof(buf) - 1, tmp);
    buf[n] = '\0';

    dup2(saved, fileno(stderr));
    close(saved);
    fclose(tmp);

    ASSERT(ret != 0);
    ASSERT(strstr(buf, "Missing argument for -U option") != NULL);
}

static void test_missing_lib_dir_arg(void)
{
    cli_options_t opts;
    char *argv[] = {"vc", "-L", "", "-o", "out.s", "file.c", NULL};
    optind = 1;
#ifdef __BSD_VISIBLE
    optreset = 1;
#endif
    FILE *tmp = tmpfile();
    if (!tmp) {
        perror("tmpfile");
        exit(1);
    }
    int saved = dup(fileno(stderr));
    dup2(fileno(tmp), fileno(stderr));

    int ret = cli_parse_args(6, argv, &opts);

    fflush(stderr);
    fseek(tmp, 0, SEEK_SET);
    char buf[256];
    size_t n = fread(buf, 1, sizeof(buf) - 1, tmp);
    buf[n] = '\0';

    dup2(saved, fileno(stderr));
    close(saved);
    fclose(tmp);

    ASSERT(ret != 0);
    ASSERT(strstr(buf, "Missing argument for -L option") != NULL);
}

static void test_missing_lib_arg(void)
{
    cli_options_t opts;
    char *argv[] = {"vc", "-l", "", "-o", "out.s", "file.c", NULL};
    optind = 1;
#ifdef __BSD_VISIBLE
    optreset = 1;
#endif
    FILE *tmp = tmpfile();
    if (!tmp) {
        perror("tmpfile");
        exit(1);
    }
    int saved = dup(fileno(stderr));
    dup2(fileno(tmp), fileno(stderr));

    int ret = cli_parse_args(6, argv, &opts);

    fflush(stderr);
    fseek(tmp, 0, SEEK_SET);
    char buf[256];
    size_t n = fread(buf, 1, sizeof(buf) - 1, tmp);
    buf[n] = '\0';

    dup2(saved, fileno(stderr));
    close(saved);
    fclose(tmp);

    ASSERT(ret != 0);
    ASSERT(strstr(buf, "Missing argument for -l option") != NULL);
}

int main(void)
{
    test_parse_success();
    test_intel_syntax_option();
    test_parse_failure();
    test_missing_define_arg();
    test_missing_undef_arg();
    test_missing_lib_dir_arg();
    test_missing_lib_arg();
    if (failures == 0)
        printf("All cli tests passed\n");
    else
        printf("%d cli test(s) failed\n", failures);
    return failures ? 1 : 0;
}
