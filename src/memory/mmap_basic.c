#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

int main(void)
{
    size_t length = 4096;

    void *memory = mmap(
        NULL,
        length,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1,
        0
    );

    if (memory == MAP_FAILED) {
        fprintf(stderr, "mmap failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    printf("PID: %ld\n", (long)getpid());
    printf("mapped address: %p\n", memory);

    int *value = memory;
    *value = 42;

    printf("value: %d\n", *value);

    printf("Press Enter to unmap...\n");
    fflush(stdout);
    getchar();

    if (munmap(memory, length) == -1) {
        fprintf(stderr, "munmap failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
