#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void)
{
    const char *path = "build/fd_demo.txt";
    char buffer[128];

    int fd = open(path, O_RDONLY);

    if (fd == -1) {
        fprintf(stderr, "open failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    printf("opened fd: %d\n", fd);

    ssize_t nread = read(fd, buffer, sizeof(buffer));

    if (nread == -1) {
        fprintf(stderr, "read failed: %s\n", strerror(errno));
        close(fd);
        return EXIT_FAILURE;
    }

    if (write(STDOUT_FILENO, buffer, (size_t)nread) == -1) {
        fprintf(stderr, "write failed: %s\n", strerror(errno));
        close(fd);
        return EXIT_FAILURE;
    }

    if (close(fd) == -1) {
        fprintf(stderr, "close failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
