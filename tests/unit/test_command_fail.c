#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <spawn.h>
#include "command.h"

static int failures = 0;
#define ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "Assertion failed: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
        failures++; \
    } \
} while (0)

/* malloc stubs */
static int fail_malloc = 0;
void *test_malloc(size_t size) {
    if (fail_malloc)
        return NULL;
    return malloc(size);
}
void *test_realloc(void *ptr, size_t size) {
    if (fail_malloc)
        return NULL;
    return realloc(ptr, size);
}

/* posix_spawnp stub that always fails */
int test_posix_spawnp(pid_t *pid, const char *file, const void *fa, const void *at,
                      char *const argv[], char *const envp[]) {
    (void)pid; (void)file; (void)fa; (void)at; (void)argv; (void)envp;
    return ENOENT;
}

static void test_error_output(void) {
    char *argv[] = {"cmd", "arg1", "arg2", NULL};
    FILE *tmp = tmpfile();
    if (!tmp) {
        perror("tmpfile");
        exit(1);
    }
    int saved = dup(fileno(stderr));
    dup2(fileno(tmp), fileno(stderr));

    fail_malloc = 1; /* force command_to_string failure */
    int rc = command_run(argv);

    fflush(stderr);
    fseek(tmp, 0, SEEK_SET);
    char buf[256];
    size_t n = fread(buf, 1, sizeof(buf) - 1, tmp);
    buf[n] = '\0';

    dup2(saved, fileno(stderr));
    close(saved);
    fclose(tmp);

    ASSERT(rc == 0);
    ASSERT(strstr(buf, "posix_spawnp cmd arg1 arg2") != NULL);
}

int main(void) {
    test_error_output();
    if (failures == 0)
        printf("All command_fail tests passed\n");
    else
        printf("%d command_fail test(s) failed\n", failures);
    return failures ? 1 : 0;
}
