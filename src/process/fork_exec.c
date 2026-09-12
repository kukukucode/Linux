#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void)
{
    printf("parent before fork: PID = %ld\n", (long)getpid());
    fflush(stdout);

    pid_t child_pid = fork();

    if (child_pid == -1) {
        fprintf(stderr, "fork failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    if (child_pid == 0) {
        printf(
            "child before exec: PID = %ld, PPID = %ld\n",
            (long)getpid(),
            (long)getppid()
        );
        fflush(stdout);

        execl(
            "/bin/echo",
            "echo",
            "hello from fork + exec",
            (char *)NULL
        );

        fprintf(stderr, "exec failed: %s\n", strerror(errno));
        _exit(127);
    }

    printf("parent: waiting for child PID %ld\n", (long)child_pid);

    int status;
    pid_t waited_pid = waitpid(child_pid, &status, 0);

    if (waited_pid == -1) {
        fprintf(stderr, "waitpid failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    if (WIFEXITED(status)) {
        printf(
            "parent: child PID %ld exited with status %d\n",
            (long)waited_pid,
            WEXITSTATUS(status)
        );
    }

    return EXIT_SUCCESS;
}
