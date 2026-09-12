#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void)
{
    const char *path = "build/fd_demo.txt";
    const char *message = "hello from file descriptor\n";

    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);

    if (fd == -1) {
        fprintf(stderr, "open failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    printf("opened fd: %d\n", fd);

    ssize_t written = write(fd, message, strlen(message));

    if (written == -1) {
        fprintf(stderr, "write failed: %s\n", strerror(errno));
        close(fd);
        return EXIT_FAILURE;
    }

    printf("written: %zd bytes\n", written);

    printf("PID: %ld\n", (long)getpid());
    puts("Press Enter to close the file descriptor...");
    getchar();

    if (close(fd) == -1) {
        fprintf(stderr, "close failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
