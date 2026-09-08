#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void)
{
    int pipefd[2];

    if (pipe(pipefd) == -1) {
        fprintf(stderr, "pipe failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    pid_t producer_pid = fork();

    if (producer_pid == -1) {
        fprintf(stderr, "first fork failed: %s\n", strerror(errno));
        close(pipefd[0]);
        close(pipefd[1]);
        return EXIT_FAILURE;
    }

    if (producer_pid == 0) {
        if (close(pipefd[0]) == -1) {
            _exit(1);
        }

        if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
            _exit(1);
        }

        if (close(pipefd[1]) == -1) {
            _exit(1);
        }

        execl(
            "/bin/echo",
            "echo",
            "hello",
            (char *)NULL
        );

        _exit(127);
    }

    pid_t consumer_pid = fork();

    if (consumer_pid == -1) {
        fprintf(stderr, "second fork failed: %s\n", strerror(errno));
        close(pipefd[0]);
        close(pipefd[1]);
        waitpid(producer_pid, NULL, 0);
        return EXIT_FAILURE;
    }

    if (consumer_pid == 0) {
        if (close(pipefd[1]) == -1) {
            _exit(1);
        }

        if (dup2(pipefd[0], STDIN_FILENO) == -1) {
            _exit(1);
        }

        if (close(pipefd[0]) == -1) {
            _exit(1);
        }

        execl(
            "/usr/bin/wc",
            "wc",
            "-c",
            (char *)NULL
        );

        _exit(127);
    }

    if (close(pipefd[0]) == -1) {
        fprintf(stderr, "parent close read end failed: %s\n", strerror(errno));
        close(pipefd[1]);
        return EXIT_FAILURE;
    }

    if (close(pipefd[1]) == -1) {
        fprintf(stderr, "parent close write end failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    int producer_status;
    int consumer_status;

    if (waitpid(producer_pid, &producer_status, 0) == -1) {
        fprintf(stderr, "waitpid producer failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    if (waitpid(consumer_pid, &consumer_status, 0) == -1) {
        fprintf(stderr, "waitpid consumer failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    if (WIFEXITED(producer_status)) {
        printf(
            "producer exited with status %d\n",
            WEXITSTATUS(producer_status)
        );
    }

    if (WIFEXITED(consumer_status)) {
        printf(
            "consumer exited with status %d\n",
            WEXITSTATUS(consumer_status)
        );
    }

    return EXIT_SUCCESS;
}
