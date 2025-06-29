#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
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
    vector_free(&opts.sources);
    vector_free(&opts.include_dirs);
}

static void test_parse_failure(void)
{
    cli_options_t opts;
    char *argv[] = {"vc", "-o", "out.s", "file.c", NULL};
    /* reset getopt state before reusing cli_parse_args */
    optind = 1;
    fail_push = 1;
    FILE *tmp = tmpfile();
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

int main(void)
{
    test_parse_success();
    test_parse_failure();
    if (failures == 0)
        printf("All cli tests passed\n");
    else
        printf("%d cli test(s) failed\n", failures);
    return failures ? 1 : 0;
}
