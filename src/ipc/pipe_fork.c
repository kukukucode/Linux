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

    printf(
        "before fork: read fd = %d, write fd = %d\n",
        pipefd[0],
        pipefd[1]
    );
    fflush(stdout);

    pid_t child_pid = fork();

    if (child_pid == -1) {
        fprintf(stderr, "fork failed: %s\n", strerror(errno));
        close(pipefd[0]);
        close(pipefd[1]);
        return EXIT_FAILURE;
    }

    if (child_pid == 0) {
        if (close(pipefd[0]) == -1) {
            fprintf(stderr, "child close failed: %s\n", strerror(errno));
            _exit(1);
        }

        const char *message = "message from child\n";

        if (write(pipefd[1], message, strlen(message)) == -1) {
            fprintf(stderr, "child write failed: %s\n", strerror(errno));
            close(pipefd[1]);
            _exit(1);
        }

        if (close(pipefd[1]) == -1) {
            fprintf(stderr, "child close failed: %s\n", strerror(errno));
            _exit(1);
        }

        _exit(0);
    }

    if (close(pipefd[1]) == -1) {
        fprintf(stderr, "parent close failed: %s\n", strerror(errno));
        close(pipefd[0]);
        return EXIT_FAILURE;
    }

    char buffer[128];

    ssize_t bytes_read = read(
        pipefd[0],
        buffer,
        sizeof(buffer)
    );

    if (bytes_read == -1) {
        fprintf(stderr, "parent read failed: %s\n", strerror(errno));
        close(pipefd[0]);
        return EXIT_FAILURE;
    }

    printf("parent read %zd bytes: ", bytes_read);
    fflush(stdout);

    if (write(STDOUT_FILENO, buffer, (size_t)bytes_read) == -1) {
        fprintf(stderr, "stdout write failed: %s\n", strerror(errno));
        close(pipefd[0]);
        return EXIT_FAILURE;
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

    return EXIT_SUCCESS;
}
