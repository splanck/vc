#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

static int failures = 0;
#define ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "Assertion failed: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
        failures++; \
    } \
} while (0)

static int test_tokens_have_inline(const char *prefix)
{
    (void)prefix;
    errno = ENOMEM;
    return -1;
}

#define tokens_have_inline test_tokens_have_inline
#include "../../src/opt_inline_helpers.c"
#undef tokens_have_inline

static int count_fds(void)
{
    DIR *d = opendir("/proc/self/fd");
    if (!d)
        return -1;
    int count = 0;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
            continue;
        count++;
    }
    closedir(d);
    return count;
}

int main(void)
{
    char tmpl[] = "/tmp/hintXXXXXX";
    int fd = mkstemp(tmpl);
    ASSERT(fd >= 0);
    const char *src = "int foo(void) { return 0; }\n";
    if (fd >= 0) {
        ASSERT(write(fd, src, strlen(src)) == (ssize_t)strlen(src));
        close(fd);
    }

    int before = count_fds();
    errno = 0;
    int r = parse_inline_hint(tmpl, "foo");
    int saved = errno;
    int after = count_fds();
    unlink(tmpl);

    ASSERT(r < 0);
    ASSERT(saved == ENOMEM);
    ASSERT(before == after);

    if (failures == 0)
        printf("All parse_inline_hint tests passed\n");
    else
        printf("%d parse_inline_hint test(s) failed\n", failures);
    return failures ? 1 : 0;
}
