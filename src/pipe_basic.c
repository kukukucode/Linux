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

    printf("read end  fd: %d\n", pipefd[0]);
    printf("write end fd: %d\n", pipefd[1]);

    const char *message = "hello through pipe\n";

    ssize_t written = write(
        pipefd[1],
        message,
        strlen(message)
    );

    if (written == -1) {
        fprintf(stderr, "write failed: %s\n", strerror(errno));
        close(pipefd[0]);
        close(pipefd[1]);
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
        close(pipefd[1]);
        return EXIT_FAILURE;
    }

    printf("read %zd bytes: ", bytes_read);

    if (write(STDOUT_FILENO, buffer, (size_t)bytes_read) == -1) {
        fprintf(stderr, "stdout write failed: %s\n", strerror(errno));
        close(pipefd[0]);
        close(pipefd[1]);
        return EXIT_FAILURE;
    }

    if (close(pipefd[0]) == -1) {
        fprintf(stderr, "close read end failed: %s\n", strerror(errno));
        close(pipefd[1]);
        return EXIT_FAILURE;
    }

    if (close(pipefd[1]) == -1) {
        fprintf(stderr, "close write end failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
