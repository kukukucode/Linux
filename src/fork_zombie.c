#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void)
{
    pid_t child_pid = fork();

    if (child_pid == -1) {
        fprintf(stderr, "fork failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    if (child_pid == 0) {
        printf(
            "child : PID = %ld, PPID = %ld\n",
            (long)getpid(),
            (long)getppid()
        );

        printf("child : exiting now\n");
        return 42;
    }

    printf(
        "parent: PID = %ld, child PID = %ld\n",
        (long)getpid(),
        (long)child_pid
    );

    printf("parent: sleeping before waitpid()\n");
    sleep(60);

    int status;

    if (waitpid(child_pid, &status, 0) == -1) {
        fprintf(stderr, "waitpid failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    printf("parent: child reaped\n");
    printf("parent: sleeping after waitpid()\n");
    sleep(60);

    return EXIT_SUCCESS;
}
