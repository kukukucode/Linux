#include <errno.h>
#include <fcntl.h>
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

static int run_command(
    char *argv[],
    pid_t shell_pgid,
    const char *input_path,
    const char *output_path
)
{
    pid_t child_pid = fork();

    if (child_pid == -1) {
        fprintf(stderr, "fork failed: %s\n", strerror(errno));
        return -1;
    }

    if (child_pid == 0) {
        /*
         * Put this command in its own process group.
         */
        if (setpgid(0, 0) == -1) {
            fprintf(stderr, "child setpgid failed: %s\n", strerror(errno));
            _exit(127);
        }

        /*
         * The shell ignores some terminal signals.
         * The child restores their normal behavior before exec().
         */
        if (restore_child_signals() == -1) {
            _exit(127);
        }

        /*
         * Input redirection:
         *
         *     command < file
         *
         * Make FD 0 point to the file.
         */
        if (input_path != NULL) {
            int fd = open(input_path, O_RDONLY);

            if (fd == -1) {
                fprintf(
                    stderr,
                    "mini-shell: %s: %s\n",
                    input_path,
                    strerror(errno)
                );
                _exit(127);
            }

            if (dup2(fd, STDIN_FILENO) == -1) {
                fprintf(stderr, "dup2 failed: %s\n", strerror(errno));
                close(fd);
                _exit(127);
            }

            if (close(fd) == -1) {
                fprintf(stderr, "close failed: %s\n", strerror(errno));
                _exit(127);
            }
        }

        /*
         * Output redirection:
         *
         *     command > file
         *
         * Make FD 1 point to the file.
         */
        if (output_path != NULL) {
            int fd = open(
                output_path,
                O_WRONLY | O_CREAT | O_TRUNC,
                0666
            );

            if (fd == -1) {
                fprintf(
                    stderr,
                    "mini-shell: %s: %s\n",
                    output_path,
                    strerror(errno)
                );
                _exit(127);
            }

            if (dup2(fd, STDOUT_FILENO) == -1) {
                fprintf(stderr, "dup2 failed: %s\n", strerror(errno));
                close(fd);
                _exit(127);
            }

            if (close(fd) == -1) {
                fprintf(stderr, "close failed: %s\n", strerror(errno));
                _exit(127);
            }
        }

        /*
         * Search PATH and replace the child process image.
         */
        execvp(argv[0], argv);

        fprintf(
            stderr,
            "mini-shell: %s: %s\n",
            argv[0],
            strerror(errno)
        );

        _exit(127);
    }

    /*
     * Parent also sets the child's PGID.
     * This avoids depending on which process runs first.
     */
    if (setpgid(child_pid, child_pid) == -1) {
        fprintf(stderr, "parent setpgid failed: %s\n", strerror(errno));
        return -1;
    }

    /*
     * Give the terminal to the foreground command.
     */
    if (tcsetpgrp(STDIN_FILENO, child_pid) == -1) {
        fprintf(stderr, "tcsetpgrp child failed: %s\n", strerror(errno));
        return -1;
    }

    int status;

    for (;;) {
        pid_t result = waitpid(child_pid, &status, WUNTRACED);

        if (result == -1) {
            if (errno == EINTR) {
                continue;
            }

            fprintf(stderr, "waitpid failed: %s\n", strerror(errno));
            return -1;
        }

        break;
    }

    /*
     * The child exited, was killed, or stopped.
     * Give the terminal back to the shell.
     */
    if (tcsetpgrp(STDIN_FILENO, shell_pgid) == -1) {
        fprintf(stderr, "tcsetpgrp shell failed: %s\n", strerror(errno));
        return -1;
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
    } else if (WIFSTOPPED(status)) {
        printf(
            "[shell] child stopped by signal %d\n",
            WSTOPSIG(status)
        );

        /*
         * Temporary behavior.
         *
         * Later, jobs/fg/bg will keep this job instead of
         * immediately destroying it.
         */
        if (kill(-child_pid, SIGCONT) == -1) {
            fprintf(stderr, "SIGCONT failed: %s\n", strerror(errno));
            return -1;
        }

        if (kill(-child_pid, SIGTERM) == -1) {
            fprintf(stderr, "SIGTERM failed: %s\n", strerror(errno));
            return -1;
        }

        if (waitpid(child_pid, NULL, 0) == -1) {
            fprintf(
                stderr,
                "cleanup waitpid failed: %s\n",
                strerror(errno)
            );
            return -1;
        }
    }

    return 0;
}

int main(void)
{
    if (!isatty(STDIN_FILENO)) {
        fprintf(stderr, "mini-shell: stdin is not a terminal\n");
        return EXIT_FAILURE;
    }

    pid_t shell_pgid = getpgrp();

    /*
     * The interactive shell survives terminal-generated signals.
     * Foreground children restore their default behavior.
     */
    if (set_signal(SIGINT, SIG_IGN) == -1 ||
        set_signal(SIGTSTP, SIG_IGN) == -1 ||
        set_signal(SIGTTOU, SIG_IGN) == -1) {
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

        char *input_path = NULL;
        char *output_path = NULL;

        int syntax_error = 0;

        char *saveptr = NULL;
        char *token = strtok_r(line, " \t\n", &saveptr);

        while (token != NULL) {
            /*
             * Input redirection.
             */
            if (strcmp(token, "<") == 0) {
                if (input_path != NULL) {
                    fprintf(
                        stderr,
                        "mini-shell: multiple input redirects\n"
                    );
                    syntax_error = 1;
                    break;
                }

                token = strtok_r(NULL, " \t\n", &saveptr);

                if (token == NULL ||
                    strcmp(token, "<") == 0 ||
                    strcmp(token, ">") == 0) {
                    fprintf(
                        stderr,
                        "mini-shell: expected filename after <\n"
                    );
                    syntax_error = 1;
                    break;
                }

                input_path = token;
            }

            /*
             * Output redirection.
             */
            else if (strcmp(token, ">") == 0) {
                if (output_path != NULL) {
                    fprintf(
                        stderr,
                        "mini-shell: multiple output redirects\n"
                    );
                    syntax_error = 1;
                    break;
                }

                token = strtok_r(NULL, " \t\n", &saveptr);

                if (token == NULL ||
                    strcmp(token, "<") == 0 ||
                    strcmp(token, ">") == 0) {
                    fprintf(
                        stderr,
                        "mini-shell: expected filename after >\n"
                    );
                    syntax_error = 1;
                    break;
                }

                output_path = token;
            }

            /*
             * Normal command argument.
             */
            else {
                if (argc >= MAX_ARGS - 1) {
                    fprintf(
                        stderr,
                        "mini-shell: too many arguments\n"
                    );
                    syntax_error = 1;
                    break;
                }

                argv[argc++] = token;
            }

            token = strtok_r(NULL, " \t\n", &saveptr);
        }

        if (syntax_error) {
            continue;
        }

        argv[argc] = NULL;

        if (argc == 0) {
            continue;
        }

        /*
         * Builtin: exit
         */
        if (strcmp(argv[0], "exit") == 0) {
            if (input_path != NULL || output_path != NULL) {
                fprintf(
                    stderr,
                    "mini-shell: redirection for builtins "
                    "is not supported yet\n"
                );
                continue;
            }

            break;
        }

        /*
         * Builtin: cd
         */
        if (strcmp(argv[0], "cd") == 0) {
            if (input_path != NULL || output_path != NULL) {
                fprintf(
                    stderr,
                    "mini-shell: redirection for builtins "
                    "is not supported yet\n"
                );
                continue;
            }

            if (argc > 2) {
                fprintf(
                    stderr,
                    "mini-shell: cd: too many arguments\n"
                );
                continue;
            }

            const char *directory;

            if (argc == 1) {
                directory = getenv("HOME");

                if (directory == NULL) {
                    fprintf(
                        stderr,
                        "mini-shell: cd: HOME is not set\n"
                    );
                    continue;
                }
            } else {
                directory = argv[1];
            }

            if (chdir(directory) == -1) {
                fprintf(
                    stderr,
                    "mini-shell: cd: %s: %s\n",
                    directory,
                    strerror(errno)
                );
            }

            continue;
        }

        /*
         * External command.
         */
        if (run_command(
                argv,
                shell_pgid,
                input_path,
                output_path
            ) == -1) {
            free(line);
            return EXIT_FAILURE;
        }
    }

    free(line);
    return EXIT_SUCCESS;
}
