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

    pid_t child_pid = fork();

    if (child_pid == -1) {
        fprintf(stderr, "fork failed: %s\n", strerror(errno));
        close(pipefd[0]);
        close(pipefd[1]);
        return EXIT_FAILURE;
    }

    if (child_pid == 0) {
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
            "hello from child through pipe",
            (char *)NULL
        );

        _exit(127);
    }

    if (close(pipefd[1]) == -1) {
        fprintf(stderr, "parent close failed: %s\n", strerror(errno));
        close(pipefd[0]);
        return EXIT_FAILURE;
    }

    char buffer[128];

    for (;;) {
        ssize_t bytes_read = read(
            pipefd[0],
            buffer,
            sizeof(buffer)
        );

        if (bytes_read == -1) {
            fprintf(stderr, "read failed: %s\n", strerror(errno));
            close(pipefd[0]);
            return EXIT_FAILURE;
        }

        if (bytes_read == 0) {
            break;
        }

        if (write(STDOUT_FILENO, buffer, (size_t)bytes_read) == -1) {
            fprintf(stderr, "stdout write failed: %s\n", strerror(errno));
            close(pipefd[0]);
            return EXIT_FAILURE;
        }
    }

    if (close(pipefd[0]) == -1) {
        fprintf(stderr, "parent close failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    int status;

    if (waitpid(child_pid, &status, 0) == -1) {
        fprintf(stderr, "waitpid failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    if (WIFEXITED(status)) {
        printf(
            "child exited with status %d\n",
            WEXITSTATUS(status)
        );
    }

    return EXIT_SUCCESS;
}
