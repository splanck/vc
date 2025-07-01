#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <spawn.h>
#include <sys/wait.h>

extern char **environ;

static int run_command(char *const argv[])
{
    pid_t pid;
    int status;
    int ret = posix_spawnp(&pid, argv[0], NULL, NULL, argv, environ);
    if (ret != 0) {
        char cmd[256];
        size_t len = 0;
        cmd[0] = '\0';
        for (size_t i = 0; argv[i] && len < sizeof(cmd) - 1; i++) {
            if (i > 0 && len < sizeof(cmd) - 1)
                cmd[len++] = ' ';
            if (len >= sizeof(cmd) - 1)
                break;
            int n = snprintf(cmd + len, sizeof(cmd) - len, "%s", argv[i]);
            if (n < 0)
                break;
            if ((size_t)n >= sizeof(cmd) - len) {
                len = sizeof(cmd) - 1;
                break;
            }
            len += (size_t)n;
        }
        cmd[len] = '\0';
        if (len == sizeof(cmd) - 1 && sizeof(cmd) > 4) {
            memcpy(cmd + sizeof(cmd) - 4, "...", 3);
            cmd[sizeof(cmd) - 1] = '\0';
        }
        fprintf(stderr, "posix_spawnp %s: %s\n", cmd, strerror(ret));
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
    void (*prev)(int) = signal(SIGALRM, handle_alarm);
    alarm(1);
    char *cmd[] = {"sleep", "2", NULL};
    int rc = run_command(cmd);
    signal(SIGALRM, prev);
    if (rc != 1) {
        printf("waitpid retry failed\n");
        return 1;
    }
    printf("All waitpid_retry tests passed\n");
    return 0;
}
