#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void)
{
    const char *path = "build/this_file_does_not_exist.txt";

    int fd = open(path, O_RDONLY);

    if (fd == -1) {
        printf("open failed\n");
        printf("errno       : %d\n", errno);
        printf("error string: %s\n", strerror(errno));

        return EXIT_SUCCESS;
    }

    close(fd);

    return EXIT_SUCCESS;
}
