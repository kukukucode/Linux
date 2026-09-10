#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

static int write_all(int fd, const void *buffer, size_t length)
{
    const char *p = buffer;

    while (length > 0) {
        ssize_t written = write(fd, p, length);

        if (written == -1) {
            if (errno == EINTR) {
                continue;
            }

            return -1;
        }

        p += written;
        length -= (size_t)written;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s FILE\n", argv[0]);
        return EXIT_FAILURE;
    }

    int fd = open(argv[1], O_RDONLY);

    if (fd == -1) {
        fprintf(stderr, "open failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    struct stat info;

    if (fstat(fd, &info) == -1) {
        fprintf(stderr, "fstat failed: %s\n", strerror(errno));
        close(fd);
        return EXIT_FAILURE;
    }

    if (info.st_size == 0) {
        fprintf(stderr, "file is empty\n");
        close(fd);
        return EXIT_FAILURE;
    }

    size_t length = (size_t)info.st_size;

    void *mapping = mmap(
        NULL,
        length,
        PROT_READ,
        MAP_PRIVATE,
        fd,
        0
    );

    if (mapping == MAP_FAILED) {
        fprintf(stderr, "mmap failed: %s\n", strerror(errno));
        close(fd);
        return EXIT_FAILURE;
    }

    printf("file size: %zu bytes\n", length);
    printf("mapped address: %p\n", mapping);
    printf("file contents through mapping:\n");
    fflush(stdout);

    if (write_all(STDOUT_FILENO, mapping, length) == -1) {
        fprintf(stderr, "write failed: %s\n", strerror(errno));
        munmap(mapping, length);
        close(fd);
        return EXIT_FAILURE;
    }

    if (munmap(mapping, length) == -1) {
        fprintf(stderr, "munmap failed: %s\n", strerror(errno));
        close(fd);
        return EXIT_FAILURE;
    }

    if (close(fd) == -1) {
        fprintf(stderr, "close failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
