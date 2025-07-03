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

extern char **environ;

/* Build a printable string representation of an argv array. */
char *command_to_string(char *const argv[])
{
    size_t len = 0;

    /* First determine required length */
    for (size_t i = 0; argv[i]; i++) {
        size_t alen = strlen(argv[i]);
        if (len > SIZE_MAX - alen - 1) {
            fprintf(stderr, "vc: command string too large\n");
            exit(1);
        }
        len += alen;
        if (argv[i + 1]) {
            if (len == SIZE_MAX) {
                fprintf(stderr, "vc: command string too large\n");
                exit(1);
            }
            len += 1; /* space */
        }
    }

    char *cmd = vc_alloc_or_exit(len + 1);
    size_t pos = 0;
    for (size_t i = 0; argv[i]; i++) {
        if (i > 0)
            cmd[pos++] = ' ';
        int n = snprintf(cmd + pos, len + 1 - pos, "%s", argv[i]);
        if (n < 0) {
            cmd[pos] = '\0';
            break;
        }
        pos += (size_t)n;
    }
    cmd[len] = '\0';
    return cmd;
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
        fprintf(stderr, "posix_spawnp %s: %s\n", cmd, strerror(ret));
        free(cmd);
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

