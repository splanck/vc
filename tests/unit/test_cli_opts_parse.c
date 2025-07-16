#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "cli.h"

static int failures = 0;
#define ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "Assertion failed: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
        failures++; \
    } \
} while (0)

int main(void)
{
    cli_options_t opts;
    char *argv1[] = {"vc", "-o", "out.s", "file.c", NULL};
    ASSERT(cli_parse_args(4, argv1, &opts) == 0);
    ASSERT(opts.sources.count == 1);
    cli_free_opts(&opts);

    setenv("VCFLAGS", "--intel-syntax", 1);
    char *argv2[] = {"vc", "-o", "out.s", "file.c", NULL};
    ASSERT(cli_parse_args(4, argv2, &opts) == 0);
    ASSERT(opts.asm_syntax == ASM_INTEL);
    cli_free_opts(&opts);
    unsetenv("VCFLAGS");

    if (failures == 0)
        printf("All cli_opts_parse tests passed\n");
    else
        printf("%d cli_opts_parse test(s) failed\n", failures);
    return failures ? 1 : 0;
}
