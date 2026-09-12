#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void)
{
    printf("before fork: PID = %ld\n", (long)getpid());
    fflush(stdout);

    pid_t result = fork();

    if (result == -1) {
        fprintf(stderr, "fork failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    if (result == 0) {
        printf(
            "child : fork() return = %ld, PID = %ld, PPID = %ld\n",
            (long)result,
            (long)getpid(),
            (long)getppid()
        );

        printf("child : sleeping for 2 seconds...\n");
        sleep(2);

        printf("child : exiting with status 42\n");
        return 42;
    }

    printf(
        "parent: fork() return = %ld, PID = %ld\n",
        (long)result,
        (long)getpid()
    );

    printf("parent: waiting for child PID %ld...\n", (long)result);

    int status;
    pid_t waited_pid = waitpid(result, &status, 0);

    if (waited_pid == -1) {
        fprintf(stderr, "waitpid failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    printf("parent: waitpid() returned PID %ld\n", (long)waited_pid);

    if (WIFEXITED(status)) {
        printf(
            "parent: child exited normally, status = %d\n",
            WEXITSTATUS(status)
        );
    }

    return EXIT_SUCCESS;
}
