#define _POSIX_C_SOURCE 200809L
/*
 * Command execution utilities.
 *
 * Implementation of command_run using posix_spawnp and helpers to
 * convert argument vectors into printable strings.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include "command.h"
#include "util.h"

/* Determine if an argument contains characters that require shell quoting */
static int needs_quotes(const char *arg)
{
    if (!arg)
        return 0;
    for (const char *p = arg; *p; p++) {
        if (strchr(" \t\n\'\"`$&;|<>*?()", *p))
            return 1;
    }
    return 0;
}

/* Ensure the output buffer can hold at least "extra" more characters */
static int ensure_cap(char **buf, size_t *len, size_t *cap, size_t extra)
{
    if (*len > SIZE_MAX - extra - 1)
        return -1;
    size_t need = *len + extra + 1;
    if (need <= *cap)
        return 0;
    size_t new_cap = *cap ? *cap : 128;
    while (new_cap < need) {
        if (new_cap > SIZE_MAX / 2)
            return -1;
        new_cap *= 2;
    }
    char *tmp = realloc(*buf, new_cap);
    if (!tmp)
        return -1;
    *buf = tmp;
    *cap = new_cap;
    return 0;
}

/* Append text to the growing buffer */
static int append_raw(char **buf, size_t *len, size_t *cap,
                      const char *text, size_t n)
{
    if (ensure_cap(buf, len, cap, n) < 0)
        return -1;
    memcpy(*buf + *len, text, n);
    *len += n;
    (*buf)[*len] = '\0';
    return 0;
}

/* Append a single argument, quoting/escaping if necessary. */
static int append_quoted(char **buf, size_t *len, size_t *cap, const char *arg)
{
    if (!needs_quotes(arg))
        return append_raw(buf, len, cap, arg, strlen(arg));

    if (append_raw(buf, len, cap, "'", 1) < 0)
        return -1;
    for (const char *p = arg; *p; p++) {
        if (*p == '\'') {
            if (append_raw(buf, len, cap, "'\\''", 4) < 0)
                return -1;
        } else if (*p == '\n') {
            if (append_raw(buf, len, cap, "\\n", 2) < 0)
                return -1;
        } else if (*p == '\r') {
            if (append_raw(buf, len, cap, "\\r", 2) < 0)
                return -1;
        } else {
            if (append_raw(buf, len, cap, p, 1) < 0)
                return -1;
        }
    }
    return append_raw(buf, len, cap, "'", 1);
}

/* Build a printable string representation of an argv array. */
char *command_to_string(char *const argv[])
{
    char *buf = NULL;
    size_t len = 0, cap = 0;
    for (size_t i = 0; argv[i]; i++) {
        if (i > 0) {
            if (append_raw(&buf, &len, &cap, " ", 1) < 0) {
                free(buf);
                return NULL;
            }
        }
        if (append_quoted(&buf, &len, &cap, argv[i]) < 0) {
            free(buf);
            return NULL;
        }
    }
    if (!buf) {
        buf = malloc(1);
        if (!buf)
            return NULL;
        buf[0] = '\0';
    }
    return buf;
}

/*
 * Spawn a command using posix_spawnp and wait for it to finish.
 */
int command_run(char *const argv[])
{
    pid_t pid;
    int status;
    int ret = posix_spawnp(&pid, argv[0], NULL, NULL, argv, environ);
    if (ret != 0) {
        char *cmd = command_to_string(argv);
        if (cmd) {
            fprintf(stderr, "posix_spawnp %s: %s\n", cmd, strerror(ret));
            free(cmd);
        } else {
            fprintf(stderr, "posix_spawnp");
            for (size_t i = 0; argv[i]; i++)
                fprintf(stderr, " %s", argv[i]);
            fprintf(stderr, ": %s\n", strerror(ret));
        }
        return 0;
    }
    while (waitpid(pid, &status, 0) < 0) {
        if (errno == EINTR)
            continue;
        perror("waitpid");
        return 0;
    }
    if (WIFSIGNALED(status)) {
        fprintf(stderr, "%s terminated by signal %d\n", argv[0],
                WTERMSIG(status));
        return -1;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        return 0;
    return 1;
}

