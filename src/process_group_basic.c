#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static void print_ids(const char *label)
{
    pid_t foreground_pgid = -1;

    if (isatty(STDIN_FILENO)) {
        foreground_pgid = tcgetpgrp(STDIN_FILENO);
    }

    printf(
        "%s: PID=%ld PPID=%ld PGID=%ld SID=%ld TTY_FG_PGID=%ld\n",
        label,
        (long)getpid(),
        (long)getppid(),
        (long)getpgrp(),
        (long)getsid(0),
        (long)foreground_pgid
    );
    fflush(stdout);
}

int main(void)
{
    print_ids("parent before fork");

    pid_t child_pid = fork();

    if (child_pid == -1) {
        fprintf(stderr, "fork failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    if (child_pid == 0) {
        print_ids("child before setpgid");

        if (setpgid(0, 0) == -1) {
            fprintf(stderr, "setpgid failed: %s\n", strerror(errno));
            return EXIT_FAILURE;
        }

        print_ids("child after setpgid");

        printf("child: sleeping for 5 seconds...\n");
        fflush(stdout);
        sleep(30);

        return EXIT_SUCCESS;
    }

    printf("parent: child PID=%ld\n", (long)child_pid);
    fflush(stdout);

    if (waitpid(child_pid, NULL, 0) == -1) {
        fprintf(stderr, "waitpid failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
