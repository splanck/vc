#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#ifndef PATH_MAX
# include <sys/param.h>
#endif
#ifndef PATH_MAX
# define PATH_MAX 4096
#endif
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include "cli.h"

int create_temp_file(const cli_options_t *cli, const char *prefix, char **out_path);

static int force_snprintf_overflow;

int snprintf(char *str, size_t size, const char *fmt, ...)
{
    if (force_snprintf_overflow) {
        (void)str; (void)fmt;
        /* return a value indicating truncation */
        return (int)(size + 1);
    }
    va_list ap;
    va_start(ap, fmt);
    int rc = vsnprintf(str, size, fmt, ap);
    va_end(ap);
    return rc;
}

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

static void test_snprintf_overflow(void)
{
    force_snprintf_overflow = 1;
    cli_options_t cli;
    memset(&cli, 0, sizeof(cli));
    const char *prefix = "vc";
    char *path = (char *)0x1;
    errno = 0;
    int fd = create_temp_file(&cli, prefix, &path);
    ASSERT(fd < 0);
    ASSERT(errno == ENAMETOOLONG);
    ASSERT(path == NULL);
    force_snprintf_overflow = 0;
}

static void test_tmpdir(void)
{
    const char *tmpdir = "./tmp_test_dir";
    mkdir(tmpdir, 0700);
    setenv("TMPDIR", tmpdir, 1);

    cli_options_t cli;
    memset(&cli, 0, sizeof(cli));
    const char *prefix = "vc";
    char *path = NULL;
    int fd = create_temp_file(&cli, prefix, &path);
    ASSERT(fd >= 0);
    ASSERT(strncmp(path, "./tmp_test_dir/", strlen("./tmp_test_dir/")) == 0);
    close(fd);
    unlink(path);
    free(path);
    unsetenv("TMPDIR");
    rmdir(tmpdir);
}

static void test_tmpdir_mkdtemp(void)
{
    char template[] = "/tmp/vcXXXXXX";
    char *dir = mkdtemp(template);
    ASSERT(dir != NULL);
    setenv("TMPDIR", dir, 1);

    cli_options_t cli;
    memset(&cli, 0, sizeof(cli));
    const char *prefix = "vc";
    char *path = NULL;
    int fd = create_temp_file(&cli, prefix, &path);
    ASSERT(fd >= 0);
    ASSERT(strncmp(path, dir, strlen(dir)) == 0 && path[strlen(dir)] == '/');
    close(fd);
    unlink(path);
    free(path);
    unsetenv("TMPDIR");
    rmdir(dir);
}

int main(void)
{
    test_reject_long_path();
    test_reject_pathmax_dir();
    test_snprintf_overflow();
    test_tmpdir();
    test_tmpdir_mkdtemp();
    if (failures == 0)
        printf("All create_temp_file tests passed\n");
    else
        printf("%d create_temp_file test(s) failed\n", failures);
    return failures ? 1 : 0;
}
