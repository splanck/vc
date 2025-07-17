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
#include "strbuf.h"

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

/* Append a single argument, quoting/escaping if necessary. */
static int append_quoted(strbuf_t *sb, const char *arg)
{
    if (!needs_quotes(arg))
        return strbuf_append(sb, arg);

    if (strbuf_append(sb, "'") < 0)
        return -1;
    for (const char *p = arg; *p; p++) {
        if (*p == '\'') {
            if (strbuf_append(sb, "'\\''") < 0)
                return -1;
        } else if (*p == '\n') {
            if (strbuf_append(sb, "\\n") < 0)
                return -1;
        } else if (*p == '\r') {
            if (strbuf_append(sb, "\\r") < 0)
                return -1;
        } else {
            char tmp[2] = {*p, '\0'};
            if (strbuf_append(sb, tmp) < 0)
                return -1;
        }
    }
    if (strbuf_append(sb, "'") < 0)
        return -1;
    return 0;
}

/* Build a printable string representation of an argv array. */
char *command_to_string(char *const argv[])
{
    strbuf_t sb;
    strbuf_init(&sb);
    for (size_t i = 0; argv[i]; i++) {
        if (i > 0 && strbuf_append(&sb, " ") < 0)
            goto overflow;
        const char *arg = argv[i];
        if (append_quoted(&sb, arg) < 0) {
            strbuf_free(&sb);
            return NULL;
        }
    }
    return sb.data;

overflow:
    strbuf_free(&sb);
    return NULL;
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

