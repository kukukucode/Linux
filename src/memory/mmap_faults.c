#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <unistd.h>

static int get_usage(struct rusage *usage)
{
    if (getrusage(RUSAGE_SELF, usage) == -1) {
        fprintf(stderr, "getrusage failed: %s\n", strerror(errno));
        return -1;
    }

    return 0;
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

    struct rusage before;
    struct rusage after_mmap;
    struct rusage after_touch;

    if (get_usage(&before) == -1) {
        return EXIT_FAILURE;
    }

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

    if (get_usage(&after_mmap) == -1) {
        munmap(memory, length);
        return EXIT_FAILURE;
    }

    for (size_t page = 0; page < page_count; ++page) {
        size_t offset = page * (size_t)page_size;
        memory[offset] = 1;
    }

    if (get_usage(&after_touch) == -1) {
        munmap(memory, length);
        return EXIT_FAILURE;
    }

    printf("page size: %ld\n", page_size);
    printf("pages mapped: %zu\n", page_count);

    printf(
        "before mmap : minor=%ld major=%ld\n",
        before.ru_minflt,
        before.ru_majflt
    );

    printf(
        "after mmap  : minor=%ld major=%ld\n",
        after_mmap.ru_minflt,
        after_mmap.ru_majflt
    );

    printf(
        "after touch : minor=%ld major=%ld\n",
        after_touch.ru_minflt,
        after_touch.ru_majflt
    );

    if (munmap(memory, length) == -1) {
        fprintf(stderr, "munmap failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
