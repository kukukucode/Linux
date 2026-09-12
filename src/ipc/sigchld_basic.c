#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static volatile sig_atomic_t got_sigchld = 0;

static void handle_sigchld(int signal_number)
{
    got_sigchld = signal_number;
}

int main(void)
{
    struct sigaction action;

    memset(&action, 0, sizeof(action));
    action.sa_handler = handle_sigchld;

    if (sigemptyset(&action.sa_mask) == -1) {
        fprintf(stderr, "sigemptyset failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    if (sigaction(SIGCHLD, &action, NULL) == -1) {
        fprintf(stderr, "sigaction failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    pid_t child_pid = fork();

    if (child_pid == -1) {
        fprintf(stderr, "fork failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    if (child_pid == 0) {
        printf("child: PID = %ld\n", (long)getpid());
        sleep(2);
        printf("child: exiting\n");
        return 42;
    }

    printf(
        "parent: PID = %ld, child PID = %ld\n",
        (long)getpid(),
        (long)child_pid
    );

    while (got_sigchld == 0) {
        pause();
    }

    printf("parent: received SIGCHLD = %d\n", (int)got_sigchld);

    int status;

    if (waitpid(child_pid, &status, 0) == -1) {
        fprintf(stderr, "waitpid failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    if (WIFEXITED(status)) {
        printf(
            "parent: child exit status = %d\n",
            WEXITSTATUS(status)
        );
    }

    return EXIT_SUCCESS;
}
