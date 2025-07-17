#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#ifndef PATH_MAX
# include <sys/param.h>
#endif
#ifndef PATH_MAX
# define PATH_MAX 4096
#endif
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
    size_t len = PATH_MAX + 10;
    char *prog = malloc(len + 1);
    memset(prog, 'a', len);
    prog[len] = '\0';

    char *argv[] = { prog, "--internal-libc", "-o", "out.o", "file.c", NULL };
    int argc = 5;

    FILE *tmp = tmpfile();
    if (!tmp) {
        perror("tmpfile");
        exit(1);
    }
    int saved = dup(fileno(stderr));
    dup2(fileno(tmp), fileno(stderr));

    cli_options_t opts;
    int ret = cli_parse_args(argc, argv, &opts);

    fflush(stderr);
    fseek(tmp, 0, SEEK_SET);
    char buf[256];
    size_t n = fread(buf, 1, sizeof(buf) - 1, tmp);
    buf[n] = '\0';

    dup2(saved, fileno(stderr));
    close(saved);
    fclose(tmp);

    if (ret == 0)
        cli_free_opts(&opts);
    free(prog);

    ASSERT(ret != 0);
    ASSERT(strstr(buf, "internal libc path too long") != NULL);

    if (failures == 0)
        printf("All prog_path_long tests passed\n");
    else
        printf("%d prog_path_long test(s) failed\n", failures);
    return failures ? 1 : 0;
}
