#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static int set_signal(int signal_number, void (*handler)(int))
{
    struct sigaction action;

    memset(&action, 0, sizeof(action));
    action.sa_handler = handler;

    if (sigemptyset(&action.sa_mask) == -1) {
        return -1;
    }

    return sigaction(signal_number, &action, NULL);
}

static int restore_child_signals(void)
{
    if (set_signal(SIGINT, SIG_DFL) == -1) {
        return -1;
    }

    if (set_signal(SIGTSTP, SIG_DFL) == -1) {
        return -1;
    }

    if (set_signal(SIGTTOU, SIG_DFL) == -1) {
        return -1;
    }

    return 0;
}

int main(void)
{
    if (!isatty(STDIN_FILENO)) {
        fprintf(stderr, "stdin is not a terminal\n");
        return EXIT_FAILURE;
    }

    pid_t shell_pgid = getpgrp();

    /*
     * A shell must survive terminal-generated signals itself while it
     * manages another foreground process group.
     */
    if (set_signal(SIGINT, SIG_IGN) == -1 ||
        set_signal(SIGTSTP, SIG_IGN) == -1 ||
        set_signal(SIGTTOU, SIG_IGN) == -1) {
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

        if (restore_child_signals() == -1) {
            _exit(127);
        }

        printf(
            "child: PID=%ld PGID=%ld\n",
            (long)getpid(),
            (long)getpgrp()
        );
        printf("child: press Ctrl-Z to stop this foreground job\n");
        fflush(stdout);

        for (;;) {
            pause();
        }
    }

    if (setpgid(child_pid, child_pid) == -1) {
        fprintf(stderr, "parent setpgid failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    if (tcsetpgrp(STDIN_FILENO, child_pid) == -1) {
        fprintf(stderr, "tcsetpgrp child failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    int status;

    if (waitpid(child_pid, &status, WUNTRACED) == -1) {
        fprintf(stderr, "waitpid failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    if (tcsetpgrp(STDIN_FILENO, shell_pgid) == -1) {
        fprintf(stderr, "tcsetpgrp shell failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    if (!WIFSTOPPED(status)) {
        fprintf(stderr, "child did not stop as expected\n");
        return EXIT_FAILURE;
    }

    printf(
        "shell: child stopped by signal %d\n",
        WSTOPSIG(status)
    );
    printf(
        "shell: terminal foreground PGID=%ld\n",
        (long)tcgetpgrp(STDIN_FILENO)
    );

    printf("shell: press Enter to run child in background...\n");
    fflush(stdout);
    getchar();

    /*
     * bg:
     * Resume the process group, but DO NOT give it the terminal.
     */
    if (kill(-child_pid, SIGCONT) == -1) {
        fprintf(stderr, "SIGCONT failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    printf("shell: child resumed in background\n");
    printf(
        "shell: terminal foreground PGID is still %ld\n",
        (long)tcgetpgrp(STDIN_FILENO)
    );
    printf("shell: inspect ps now, then press Enter for fg...\n");
    fflush(stdout);
    getchar();

    /*
     * fg:
     * Give the terminal to the job and ensure it is running.
     */
    if (tcsetpgrp(STDIN_FILENO, child_pid) == -1) {
        fprintf(stderr, "tcsetpgrp child failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    if (kill(-child_pid, SIGCONT) == -1) {
        fprintf(stderr, "SIGCONT failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    if (waitpid(child_pid, &status, WUNTRACED) == -1) {
        fprintf(stderr, "waitpid failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    if (tcsetpgrp(STDIN_FILENO, shell_pgid) == -1) {
        fprintf(stderr, "tcsetpgrp shell failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    if (WIFSIGNALED(status)) {
        printf(
            "shell: child terminated by signal %d\n",
            WTERMSIG(status)
        );
    } else if (WIFSTOPPED(status)) {
        printf(
            "shell: child stopped again by signal %d\n",
            WSTOPSIG(status)
        );
    }

    printf(
        "shell: terminal foreground PGID=%ld\n",
        (long)tcgetpgrp(STDIN_FILENO)
    );

    return EXIT_SUCCESS;
}
