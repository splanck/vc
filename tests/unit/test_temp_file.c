#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <linux/limits.h>
#include <errno.h>
#include "cli.h"

int create_temp_file(const cli_options_t *cli, const char *prefix, char **out_path);

static int failures = 0;
#define ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "Assertion failed: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
        failures++; \
    } \
} while (0)

static void test_reject_long_path(void)
{
    const char *prefix = "vc";
    size_t dir_len = PATH_MAX - strlen(prefix) - sizeof("/XXXXXX") + 1;
    char *dir = malloc(dir_len + 1);
    memset(dir, 'a', dir_len);
    dir[dir_len] = '\0';

    cli_options_t cli;
    memset(&cli, 0, sizeof(cli));
    cli.obj_dir = dir;

    char *path = (char *)0x1;
    errno = 0;
    int fd = create_temp_file(&cli, prefix, &path);
    ASSERT(fd < 0);
    ASSERT(errno == ENAMETOOLONG);
    ASSERT(path == NULL);

    free(dir);
}

static void test_reject_pathmax_dir(void)
{
    const char *prefix = "vc";
    size_t dir_len = PATH_MAX - strlen(prefix) - sizeof("/XXXXXX");
    char *dir = malloc(dir_len + 1);
    memset(dir, 'a', dir_len);
    dir[dir_len] = '\0';

    cli_options_t cli;
    memset(&cli, 0, sizeof(cli));
    cli.obj_dir = dir;

    char *path = (char *)0x1;
    errno = 0;
    int fd = create_temp_file(&cli, prefix, &path);
    ASSERT(fd < 0);
    ASSERT(errno == ENAMETOOLONG);
    ASSERT(path == NULL);

    free(dir);
}

int main(void)
{
    test_reject_long_path();
    test_reject_pathmax_dir();
    if (failures == 0)
        printf("All create_temp_file tests passed\n");
    else
        printf("%d create_temp_file test(s) failed\n", failures);
    return failures ? 1 : 0;
}
