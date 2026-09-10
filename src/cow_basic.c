#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
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

    printf(
        "before fork: PID=%ld address=%p value=%d\n",
        (long)getpid(),
        (void *)value,
        *value
    );
    fflush(stdout);

    pid_t child_pid = fork();

    if (child_pid == -1) {
        fprintf(stderr, "fork failed: %s\n", strerror(errno));
        munmap(value, length);
        return EXIT_FAILURE;
    }

    if (child_pid == 0) {
        printf(
            "child before write: PID=%ld address=%p value=%d\n",
            (long)getpid(),
            (void *)value,
            *value
        );

        *value = 99;

        printf(
            "child after write : PID=%ld address=%p value=%d\n",
            (long)getpid(),
            (void *)value,
            *value
        );

        munmap(value, length);
        return EXIT_SUCCESS;
    }

    if (waitpid(child_pid, NULL, 0) == -1) {
        fprintf(stderr, "waitpid failed: %s\n", strerror(errno));
        munmap(value, length);
        return EXIT_FAILURE;
    }

    printf(
        "parent after child: PID=%ld address=%p value=%d\n",
        (long)getpid(),
        (void *)value,
        *value
    );

    if (munmap(value, length) == -1) {
        fprintf(stderr, "munmap failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
