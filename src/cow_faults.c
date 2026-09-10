#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

static long minor_faults(void)
{
    struct rusage usage;

    if (getrusage(RUSAGE_SELF, &usage) == -1) {
        fprintf(stderr, "getrusage failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    return usage.ru_minflt;
}

int main(void)
{
    long page_size = sysconf(_SC_PAGESIZE);

    if (page_size == -1) {
        fprintf(stderr, "sysconf failed\n");
        return EXIT_FAILURE;
    }

    const size_t page_count = 4096;
    size_t length = (size_t)page_size * page_count;

    unsigned char *memory = mmap(
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

    /* Materialize the pages before fork. */
    for (size_t page = 0; page < page_count; ++page) {
        memory[page * (size_t)page_size] = 1;
    }

    pid_t child_pid = fork();

    if (child_pid == -1) {
        fprintf(stderr, "fork failed: %s\n", strerror(errno));
        munmap(memory, length);
        return EXIT_FAILURE;
    }

    if (child_pid == 0) {
        long before = minor_faults();

        for (size_t page = 0; page < page_count; ++page) {
            memory[page * (size_t)page_size] = 2;
        }

        long after = minor_faults();

        printf("child minor faults before writes: %ld\n", before);
        printf("child minor faults after writes : %ld\n", after);
        printf("pages written: %zu\n", page_count);

        munmap(memory, length);
        return EXIT_SUCCESS;
    }

    if (waitpid(child_pid, NULL, 0) == -1) {
        fprintf(stderr, "waitpid failed: %s\n", strerror(errno));
        munmap(memory, length);
        return EXIT_FAILURE;
    }

    printf("parent first byte after child: %u\n", memory[0]);

    munmap(memory, length);
    return EXIT_SUCCESS;
}
