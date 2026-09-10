#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static volatile sig_atomic_t signal_received = 0;

static void handle_sigusr1(int signal_number)
{
    signal_received = signal_number;
}

int main(void)
{
    struct sigaction action;

    memset(&action, 0, sizeof(action));
    action.sa_handler = handle_sigusr1;

    if (sigemptyset(&action.sa_mask) == -1) {
        fprintf(stderr, "sigemptyset failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    if (sigaction(SIGUSR1, &action, NULL) == -1) {
        fprintf(stderr, "sigaction failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    printf("PID: %ld\n", (long)getpid());
    printf("waiting for SIGUSR1...\n");
    fflush(stdout);

    while (signal_received == 0) {
        pause();
    }

    printf("received signal: %d\n", (int)signal_received);

    return EXIT_SUCCESS;
}
