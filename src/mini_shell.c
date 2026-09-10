#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_ARGS 64

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

static int run_command(char *argv[])
{
    pid_t child_pid = fork();

    if (child_pid == -1) {
        fprintf(stderr, "fork failed: %s\n", strerror(errno));
        return -1;
    }

    if (child_pid == 0) {
        /*
         * The shell ignores SIGINT so Ctrl-C does not kill the shell.
         * The child restores the normal behavior before exec().
         */
        if (set_signal(SIGINT, SIG_DFL) == -1) {
            _exit(127);
        }

        execvp(argv[0], argv);

        fprintf(
            stderr,
            "mini-shell: %s: %s\n",
            argv[0],
            strerror(errno)
        );

        _exit(127);
    }

    int status;

    for (;;) {
        pid_t result = waitpid(child_pid, &status, 0);

        if (result == -1) {
            if (errno == EINTR) {
                continue;
            }

            fprintf(stderr, "waitpid failed: %s\n", strerror(errno));
            return -1;
        }

        break;
    }

    if (WIFEXITED(status)) {
        printf(
            "[shell] child exited with status %d\n",
            WEXITSTATUS(status)
        );
    } else if (WIFSIGNALED(status)) {
        printf(
            "[shell] child terminated by signal %d\n",
            WTERMSIG(status)
        );
    }

    return 0;
}

int main(void)
{
    if (set_signal(SIGINT, SIG_IGN) == -1) {
        fprintf(stderr, "sigaction failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    char *line = NULL;
    size_t capacity = 0;

    for (;;) {
        printf("mini$ ");
        fflush(stdout);

        errno = 0;
        ssize_t length = getline(&line, &capacity, stdin);

        if (length == -1) {
            if (feof(stdin)) {
                printf("\n");
                break;
            }

            fprintf(stderr, "getline failed: %s\n", strerror(errno));
            free(line);
            return EXIT_FAILURE;
        }

        char *argv[MAX_ARGS];
        size_t argc = 0;

        char *saveptr = NULL;
        char *token = strtok_r(line, " \t\n", &saveptr);

        while (token != NULL && argc < MAX_ARGS - 1) {
            argv[argc++] = token;
            token = strtok_r(NULL, " \t\n", &saveptr);
        }

        argv[argc] = NULL;

        if (argc == 0) {
            continue;
        }

        if (strcmp(argv[0], "exit") == 0) {
            break;
        }

        if (run_command(argv) == -1) {
            free(line);
            return EXIT_FAILURE;
        }
    }

    free(line);
    return EXIT_SUCCESS;
}
