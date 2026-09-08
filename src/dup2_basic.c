#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void)
{
    int pipefd[2];

    if (pipe(pipefd) == -1) {
        fprintf(stderr, "pipe failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    fprintf(
        stderr,
        "before dup2: read fd = %d, write fd = %d, stdout fd = %d\n",
        pipefd[0],
        pipefd[1],
        STDOUT_FILENO
    );

    if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
        fprintf(stderr, "dup2 failed: %s\n", strerror(errno));
        close(pipefd[0]);
        close(pipefd[1]);
        return EXIT_FAILURE;
    }

    if (close(pipefd[1]) == -1) {
        fprintf(stderr, "close failed: %s\n", strerror(errno));
        close(pipefd[0]);
        return EXIT_FAILURE;
    }

    const char *message = "hello through redirected stdout\n";

    if (write(STDOUT_FILENO, message, strlen(message)) == -1) {
        fprintf(stderr, "stdout write failed: %s\n", strerror(errno));
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
        fprintf(stderr, "read failed: %s\n", strerror(errno));
        close(pipefd[0]);
        return EXIT_FAILURE;
    }

    fprintf(stderr, "read from pipe: ");

    if (write(STDERR_FILENO, buffer, (size_t)bytes_read) == -1) {
        close(pipefd[0]);
        return EXIT_FAILURE;
    }

    if (close(pipefd[0]) == -1) {
        fprintf(stderr, "close failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
