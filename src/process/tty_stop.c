#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static int ignore_signal(int signal_number)
{
    struct sigaction action;

    memset(&action, 0, sizeof(action));
    action.sa_handler = SIG_IGN;

    if (sigemptyset(&action.sa_mask) == -1) {
        return -1;
    }

    return sigaction(signal_number, &action, NULL);
}

int main(void)
{
    if (!isatty(STDIN_FILENO)) {
        fprintf(stderr, "stdin is not a terminal\n");
        return EXIT_FAILURE;
    }

    pid_t parent_pgid = getpgrp();

    /*
     * The parent becomes a background process group while the child
     * owns the terminal. Ignore SIGTTOU so the parent can reclaim it.
     */
    if (ignore_signal(SIGTTOU) == -1) {
        fprintf(stderr, "sigaction failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    pid_t child_pid = fork();

    if (child_pid == -1) {
        fprintf(stderr, "fork failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    if (child_pid == 0) {
        if (setpgid(0, 0) == -1) {
            fprintf(stderr, "child setpgid failed: %s\n", strerror(errno));
            _exit(127);
        }

        printf(
            "child: PID=%ld PGID=%ld\n",
            (long)getpid(),
            (long)getpgrp()
        );
        printf("child: press Ctrl-Z to stop me\n");
        fflush(stdout);

        for (;;) {
            pause();
        }
    }

    if (setpgid(child_pid, child_pid) == -1) {
        fprintf(stderr, "parent setpgid failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    printf(
        "parent: giving terminal to child PGID=%ld\n",
        (long)child_pid
    );
    fflush(stdout);

    if (tcsetpgrp(STDIN_FILENO, child_pid) == -1) {
        fprintf(stderr, "tcsetpgrp child failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    int status;

    pid_t result = waitpid(child_pid, &status, WUNTRACED);

    if (result == -1) {
        fprintf(stderr, "waitpid failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    if (tcsetpgrp(STDIN_FILENO, parent_pgid) == -1) {
        fprintf(stderr, "tcsetpgrp parent failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    if (WIFSTOPPED(status)) {
        printf(
            "parent: child stopped by signal %d\n",
            WSTOPSIG(status)
        );
    }

    printf(
        "parent: terminal foreground PGID=%ld\n",
        (long)tcgetpgrp(STDIN_FILENO)
    );

    printf("parent: child remains stopped; press Enter to clean up\n");
    fflush(stdout);
    getchar();

    if (kill(-child_pid, SIGCONT) == -1) {
        fprintf(stderr, "SIGCONT failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    if (kill(-child_pid, SIGTERM) == -1) {
        fprintf(stderr, "SIGTERM failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    if (waitpid(child_pid, NULL, 0) == -1) {
        fprintf(stderr, "final waitpid failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
