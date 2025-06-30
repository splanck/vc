#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <spawn.h>
#include <sys/wait.h>
#include "strbuf.h"

extern char **environ;

static int run_command(char *const argv[])
{
    pid_t pid;
    int status;
    int ret = posix_spawnp(&pid, argv[0], NULL, NULL, argv, environ);
    if (ret != 0) {
        strbuf_t cmd;
        strbuf_init(&cmd);
        for (size_t i = 0; argv[i]; i++) {
            if (i > 0)
                strbuf_append(&cmd, " ");
            strbuf_append(&cmd, argv[i]);
        }
        fprintf(stderr, "posix_spawnp %s: %s\n", cmd.data, strerror(ret));
        strbuf_free(&cmd);
        return 0;
    }
    while (waitpid(pid, &status, 0) < 0) {
        if (errno == EINTR)
            continue;
        perror("waitpid");
        return 0;
    }
    if (WIFSIGNALED(status))
        return -1;
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        return 0;
    return 1;
}

static void handle_alarm(int sig)
{
    (void)sig;
}

int main(void)
{
    signal(SIGALRM, handle_alarm);
    alarm(1);
    char *cmd[] = {"sleep", "2", NULL};
    int rc = run_command(cmd);
    if (rc != 1) {
        printf("waitpid retry failed\n");
        return 1;
    }
    printf("All waitpid_retry tests passed\n");
    return 0;
}
