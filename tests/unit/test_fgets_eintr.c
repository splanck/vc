#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>

static void handle_alarm(int sig) {
    (void)sig;
}

int main(void) {
    int pipefd[2];
    if (pipe(pipefd) != 0)
        return 1;

    signal(SIGALRM, handle_alarm);

    pid_t pid = fork();
    if (pid == -1)
        return 2;
    if (pid == 0) {
        close(pipefd[0]);
        sleep(2);
        if (write(pipefd[1], "ok\n", 3) != 3)
            _exit(3);
        close(pipefd[1]);
        _exit(0);
    }
    close(pipefd[1]);

    FILE f = { .fd = pipefd[0], .err = 0, .eof = 0 };
    char buf[16];
    alarm(1);
    if (!fgets(buf, sizeof(buf), &f))
        return 4;
    if (buf[0] != 'o' || buf[1] != 'k' || buf[2] != '\n' || buf[3] != '\0')
        return 5;
    if (f.err || f.eof)
        return 6;

    alarm(0);
    close(pipefd[0]);
    waitpid(pid, NULL, 0);
    return 0;
}
