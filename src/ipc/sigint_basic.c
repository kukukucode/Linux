#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static volatile sig_atomic_t got_sigint = 0;

static void handle_sigint(int signal_number)
{
    got_sigint = signal_number;
}

int main(void)
{
    struct sigaction action;

    memset(&action, 0, sizeof(action));
    action.sa_handler = handle_sigint;

    if (sigemptyset(&action.sa_mask) == -1) {
        fprintf(stderr, "sigemptyset failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    if (sigaction(SIGINT, &action, NULL) == -1) {
        fprintf(stderr, "sigaction failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    printf("PID: %ld\n", (long)getpid());
    printf("Press Ctrl-C...\n");
    fflush(stdout);

    while (got_sigint == 0) {
        pause();
    }

    printf("received SIGINT: %d\n", (int)got_sigint);

    return EXIT_SUCCESS;
}
