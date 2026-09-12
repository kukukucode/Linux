#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static int set_disposition(int signal_number, void (*handler)(int))
{
    struct sigaction action;

    memset(&action, 0, sizeof(action));
    action.sa_handler = handler;

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
     * The parent will temporarily become a background process group.
     * Ignore SIGTTOU so it can later reclaim the terminal.
     */
    if (set_disposition(SIGTTOU, SIG_IGN) == -1) {
        fprintf(stderr, "sigaction SIGTTOU failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    /*
     * Keep the parent alive if SIGINT accidentally reaches it.
     * The child resets SIGINT to the default action.
     */
    if (set_disposition(SIGINT, SIG_IGN) == -1) {
        fprintf(stderr, "sigaction SIGINT failed: %s\n", strerror(errno));
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

        if (set_disposition(SIGINT, SIG_DFL) == -1) {
            _exit(127);
        }

        if (set_disposition(SIGTTOU, SIG_DFL) == -1) {
            _exit(127);
        }

        printf(
            "child: PID=%ld PGID=%ld\n",
            (long)getpid(),
            (long)getpgrp()
        );
        printf("child: press Ctrl-C to terminate me\n");
        fflush(stdout);

        for (;;) {
            pause();
        }
    }

    /*
     * Shells normally call setpgid() in both parent and child
     * to avoid a race over which one runs first.
     */
    if (setpgid(child_pid, child_pid) == -1) {
        fprintf(stderr, "parent setpgid failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    printf(
        "parent: PGID=%ld, giving terminal to child PGID=%ld\n",
        (long)parent_pgid,
        (long)child_pid
    );
    fflush(stdout);

    if (tcsetpgrp(STDIN_FILENO, child_pid) == -1) {
        fprintf(stderr, "tcsetpgrp child failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    int status;

    if (waitpid(child_pid, &status, 0) == -1) {
        fprintf(stderr, "waitpid failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    if (tcsetpgrp(STDIN_FILENO, parent_pgid) == -1) {
        fprintf(stderr, "tcsetpgrp parent failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    if (WIFSIGNALED(status)) {
        printf(
            "parent: child terminated by signal %d\n",
            WTERMSIG(status)
        );
    }

    printf(
        "parent: terminal foreground PGID=%ld\n",
        (long)tcgetpgrp(STDIN_FILENO)
    );

    return EXIT_SUCCESS;
}
