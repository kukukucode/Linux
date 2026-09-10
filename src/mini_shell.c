#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
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

static int redirect_fd(
    const char *path,
    int flags,
    mode_t mode,
    int target_fd
)
{
    int fd;

    if ((flags & O_CREAT) != 0) {
        fd = open(path, flags, mode);
    } else {
        fd = open(path, flags);
    }

    if (fd == -1) {
        fprintf(
            stderr,
            "mini-shell: %s: %s\n",
            path,
            strerror(errno)
        );
        return -1;
    }

    if (dup2(fd, target_fd) == -1) {
        fprintf(stderr, "dup2 failed: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    if (close(fd) == -1) {
        fprintf(stderr, "close failed: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

static int wait_one(pid_t pid, int *status)
{
    for (;;) {
        pid_t result = waitpid(pid, status, WUNTRACED);

        if (result == -1) {
            if (errno == EINTR) {
                continue;
            }

            fprintf(stderr, "waitpid failed: %s\n", strerror(errno));
            return -1;
        }

        return 0;
    }
}

static int cleanup_stopped_job(pid_t pgid)
{
    if (kill(-pgid, SIGCONT) == -1 && errno != ESRCH) {
        fprintf(stderr, "SIGCONT failed: %s\n", strerror(errno));
        return -1;
    }

    if (kill(-pgid, SIGTERM) == -1 && errno != ESRCH) {
        fprintf(stderr, "SIGTERM failed: %s\n", strerror(errno));
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
        if (setpgid(0, 0) == -1) {
            fprintf(stderr, "child setpgid failed: %s\n", strerror(errno));
            _exit(127);
        }

        if (restore_child_signals() == -1) {
            _exit(127);
        }

        if (input_path != NULL) {
            if (redirect_fd(
                    input_path,
                    O_RDONLY,
                    0,
                    STDIN_FILENO
                ) == -1) {
                _exit(127);
            }
        }

        if (output_path != NULL) {
            if (redirect_fd(
                    output_path,
                    O_WRONLY | O_CREAT | O_TRUNC,
                    0666,
                    STDOUT_FILENO
                ) == -1) {
                _exit(127);
            }
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

    if (setpgid(child_pid, child_pid) == -1) {
        fprintf(stderr, "parent setpgid failed: %s\n", strerror(errno));
        return -1;
    }

    if (tcsetpgrp(STDIN_FILENO, child_pid) == -1) {
        fprintf(stderr, "tcsetpgrp child failed: %s\n", strerror(errno));
        return -1;
    }

    int status;

    if (wait_one(child_pid, &status) == -1) {
        return -1;
    }

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

        if (cleanup_stopped_job(child_pid) == -1) {
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

static void exec_pipeline_child(
    char *argv[],
    pid_t pgid,
    int input_fd,
    int output_fd,
    int pipe_read_fd,
    int pipe_write_fd
)
{
    if (setpgid(0, pgid) == -1) {
        fprintf(stderr, "setpgid failed: %s\n", strerror(errno));
        _exit(127);
    }

    if (restore_child_signals() == -1) {
        _exit(127);
    }

    if (input_fd != STDIN_FILENO) {
        if (dup2(input_fd, STDIN_FILENO) == -1) {
            fprintf(stderr, "dup2 stdin failed: %s\n", strerror(errno));
            _exit(127);
        }
    }

    if (output_fd != STDOUT_FILENO) {
        if (dup2(output_fd, STDOUT_FILENO) == -1) {
            fprintf(stderr, "dup2 stdout failed: %s\n", strerror(errno));
            _exit(127);
        }
    }

    if (pipe_read_fd != STDIN_FILENO) {
        close(pipe_read_fd);
    }

    if (pipe_write_fd != STDOUT_FILENO) {
        close(pipe_write_fd);
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

static int run_pipeline(
    char *left_argv[],
    char *right_argv[],
    pid_t shell_pgid,
    const char *input_path,
    const char *output_path
)
{
    int pipefd[2];

    if (pipe(pipefd) == -1) {
        fprintf(stderr, "pipe failed: %s\n", strerror(errno));
        return -1;
    }

    pid_t left_pid = fork();

    if (left_pid == -1) {
        fprintf(stderr, "fork failed: %s\n", strerror(errno));
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }

if (left_pid == 0) {
    /*
     * For:
     *
     *     command < file | command
     *
     * redirect the left process's stdin first.
     */
    if (input_path != NULL) {
        if (redirect_fd(
                input_path,
                O_RDONLY,
                0,
                STDIN_FILENO
            ) == -1) {
            _exit(127);
        }
    }

    exec_pipeline_child(
        left_argv,
        0,
        STDIN_FILENO,
        pipefd[1],
        pipefd[0],
        pipefd[1]
    );
}

    if (setpgid(left_pid, left_pid) == -1) {
        fprintf(stderr, "left setpgid failed: %s\n", strerror(errno));
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }

    pid_t right_pid = fork();

    if (right_pid == -1) {
        fprintf(stderr, "fork failed: %s\n", strerror(errno));
        close(pipefd[0]);
        close(pipefd[1]);
        kill(-left_pid, SIGTERM);
        waitpid(left_pid, NULL, 0);
        return -1;
    }

if (right_pid == 0) {
    /*
     * For:
     *
     *     command | command > file
     *
     * redirect the right process's stdout.
     */
    if (output_path != NULL) {
        if (redirect_fd(
                output_path,
                O_WRONLY | O_CREAT | O_TRUNC,
                0666,
                STDOUT_FILENO
            ) == -1) {
            _exit(127);
        }
    }

    exec_pipeline_child(
        right_argv,
        left_pid,
        pipefd[0],
        STDOUT_FILENO,
        pipefd[0],
        pipefd[1]
    );
}

    if (setpgid(right_pid, left_pid) == -1) {
        fprintf(stderr, "right setpgid failed: %s\n", strerror(errno));
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }

    /*
     * The shell itself must not keep either pipe end open.
     */
    close(pipefd[0]);
    close(pipefd[1]);

    /*
     * The whole pipeline is one foreground job.
     */
    if (tcsetpgrp(STDIN_FILENO, left_pid) == -1) {
        fprintf(stderr, "tcsetpgrp pipeline failed: %s\n", strerror(errno));
        return -1;
    }

    int remaining = 2;
    int stopped = 0;

    while (remaining > 0) {
        int status;

        pid_t result = waitpid(
            -left_pid,
            &status,
            WUNTRACED
        );

        if (result == -1) {
            if (errno == EINTR) {
                continue;
            }

            fprintf(stderr, "pipeline waitpid failed: %s\n", strerror(errno));
            break;
        }

        if (WIFSTOPPED(status)) {
            printf(
                "[shell] pipeline process %ld stopped by signal %d\n",
                (long)result,
                WSTOPSIG(status)
            );
            stopped = 1;
            break;
        }

        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            --remaining;
        }
    }

    if (tcsetpgrp(STDIN_FILENO, shell_pgid) == -1) {
        fprintf(stderr, "tcsetpgrp shell failed: %s\n", strerror(errno));
        return -1;
    }

    if (stopped) {
        if (cleanup_stopped_job(left_pid) == -1) {
            return -1;
        }

        while (waitpid(-left_pid, NULL, 0) != -1) {
        }

        if (errno != ECHILD) {
            fprintf(stderr, "cleanup waitpid failed: %s\n", strerror(errno));
            return -1;
        }
    }

    printf("[shell] pipeline finished\n");

    return 0;
}

int main(void)
{
    if (!isatty(STDIN_FILENO)) {
        fprintf(stderr, "mini-shell: stdin is not a terminal\n");
        return EXIT_FAILURE;
    }

    pid_t shell_pgid = getpgrp();

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

        char *left_argv[MAX_ARGS];
        char *right_argv[MAX_ARGS];

        size_t left_argc = 0;
        size_t right_argc = 0;

        char *input_path = NULL;
        char *output_path = NULL;

        int pipe_seen = 0;
        int syntax_error = 0;

        int input_after_pipe = 0;
        int output_before_pipe = 0;

        char *saveptr = NULL;
        char *token = strtok_r(line, " \t\n", &saveptr);

        while (token != NULL) {
            if (strcmp(token, "|") == 0) {
                if (pipe_seen) {
                    fprintf(
                        stderr,
                        "mini-shell: only one pipe is supported for now\n"
                    );
                    syntax_error = 1;
                    break;
                }

                if (left_argc == 0) {
                    fprintf(
                        stderr,
                        "mini-shell: expected command before |\n"
                    );
                    syntax_error = 1;
                    break;
                }

                pipe_seen = 1;
            } else if (strcmp(token, "<") == 0) {
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
                    strcmp(token, ">") == 0 ||
                    strcmp(token, "|") == 0) {
                    fprintf(
                        stderr,
                        "mini-shell: expected filename after <\n"
                    );
                    syntax_error = 1;
                    break;
                }

                input_path = token;
                input_after_pipe = pipe_seen;
            } else if (strcmp(token, ">") == 0) {
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
                    strcmp(token, ">") == 0 ||
                    strcmp(token, "|") == 0) {
                    fprintf(
                        stderr,
                        "mini-shell: expected filename after >\n"
                    );
                    syntax_error = 1;
                    break;
                }

                output_path = token;
            } else if (!pipe_seen) {
                if (left_argc >= MAX_ARGS - 1) {
                    fprintf(stderr, "mini-shell: too many arguments\n");
                    syntax_error = 1;
                    break;
                }

                left_argv[left_argc++] = token;
            } else {
                if (right_argc >= MAX_ARGS - 1) {
                    fprintf(stderr, "mini-shell: too many arguments\n");
                    syntax_error = 1;
                    break;
                }

                right_argv[right_argc++] = token;
            }

            token = strtok_r(NULL, " \t\n", &saveptr);
        }

        if (syntax_error) {
            continue;
        }

        left_argv[left_argc] = NULL;
        right_argv[right_argc] = NULL;

        if (left_argc == 0) {
            continue;
        }

        if (pipe_seen) {
            if (input_after_pipe) {
    fprintf(
        stderr,
        "mini-shell: input redirection after | "
        "is not supported yet\n"
    );
    continue;
}

if (output_before_pipe) {
    fprintf(
        stderr,
        "mini-shell: output redirection before | "
        "is not supported yet\n"
    );
    continue;
}


            if (strcmp(left_argv[0], "cd") == 0 ||
                strcmp(left_argv[0], "exit") == 0 ||
                strcmp(right_argv[0], "cd") == 0 ||
                strcmp(right_argv[0], "exit") == 0) {
                fprintf(
                    stderr,
                    "mini-shell: builtins in pipelines "
                    "are not supported yet\n"
                );
                continue;
            }

            if (run_pipeline(
            left_argv,
            right_argv,
            shell_pgid,
            input_path,
            output_path
        ) == -1) {
                free(line);
                return EXIT_FAILURE;
            }

            continue;
        }

        if (strcmp(left_argv[0], "exit") == 0) {
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

        if (strcmp(left_argv[0], "cd") == 0) {
            if (input_path != NULL || output_path != NULL) {
                fprintf(
                    stderr,
                    "mini-shell: redirection for builtins "
                    "is not supported yet\n"
                );
                continue;
            }

            if (left_argc > 2) {
                fprintf(
                    stderr,
                    "mini-shell: cd: too many arguments\n"
                );
                continue;
            }

            const char *directory;

            if (left_argc == 1) {
                directory = getenv("HOME");

                if (directory == NULL) {
                    fprintf(
                        stderr,
                        "mini-shell: cd: HOME is not set\n"
                    );
                    continue;
                }
            } else {
                directory = left_argv[1];
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

        if (run_command(
                left_argv,
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
