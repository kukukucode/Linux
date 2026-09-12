#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

int main(void)
{
    size_t length = 4096;

    int *value = mmap(
        NULL,
        length,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1,
        0
    );

    if (value == MAP_FAILED) {
        fprintf(stderr, "mmap failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    *value = 42;
    printf("before mprotect: %d\n", *value);

    if (mprotect(value, length, PROT_READ) == -1) {
        fprintf(stderr, "mprotect failed: %s\n", strerror(errno));
        munmap(value, length);
        return EXIT_FAILURE;
    }

    printf("memory is now read-only\n");
    printf("read still works: %d\n", *value);

    printf("trying to write 99...\n");
    fflush(stdout);

    *value = 99;

    printf("after write: %d\n", *value);

    munmap(value, length);
    return EXIT_SUCCESS;
}
