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
#define MAX_COMMANDS 16

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

static void reap_background_children(void)
{
    for (;;) {
        int status;

        pid_t pid = waitpid(
            -1,
            &status,
            WNOHANG
        );

        if (pid == 0) {
            return;
        }

        if (pid == -1) {
            if (errno == ECHILD) {
                return;
            }

            if (errno == EINTR) {
                continue;
            }

            fprintf(
                stderr,
                "background waitpid failed: %s\n",
                strerror(errno)
            );
            return;
        }

        if (WIFEXITED(status)) {
            printf(
                "[shell] background pid=%ld "
                "exited with status %d\n",
                (long)pid,
                WEXITSTATUS(status)
            );
        } else if (WIFSIGNALED(status)) {
            printf(
                "[shell] background pid=%ld "
                "terminated by signal %d\n",
                (long)pid,
                WTERMSIG(status)
            );
        }
    }
}

static int run_command(
    char *argv[],
    pid_t shell_pgid,
    const char *input_path,
    const char *output_path,
    int background
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
if (background) {
    printf(
        "[shell] background pid=%ld pgid=%ld\n",
        (long)child_pid,
        (long)child_pid
    );

    return 0;
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

static void close_all_pipes(
    int pipes[][2],
    size_t pipe_count
)
{
    for (size_t i = 0; i < pipe_count; ++i) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
}

static void exec_pipeline_child(
    char *argv[],
    pid_t pgid,
    size_t index,
    size_t command_count,
    int pipes[][2],
    const char *input_path,
    const char *output_path
)
{
    if (setpgid(0, pgid) == -1) {
        fprintf(stderr, "setpgid failed: %s\n", strerror(errno));
        _exit(127);
    }

    if (restore_child_signals() == -1) {
        _exit(127);
    }

    /*
     * stdin
     */
    if (index == 0) {
        /*
         * First command:
         *
         *     file < cmd0 | cmd1 | ...
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
    } else {
        /*
         * All commands except the first read from
         * the previous pipe.
         */
        if (dup2(
                pipes[index - 1][0],
                STDIN_FILENO
            ) == -1) {
            fprintf(
                stderr,
                "dup2 stdin failed: %s\n",
                strerror(errno)
            );
            _exit(127);
        }
    }

    /*
     * stdout
     */
    if (index + 1 == command_count) {
        /*
         * Last command:
         *
         *     ... | cmdN > file
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
    } else {
        /*
         * All commands except the last write to
         * the next pipe.
         */
        if (dup2(
                pipes[index][1],
                STDOUT_FILENO
            ) == -1) {
            fprintf(
                stderr,
                "dup2 stdout failed: %s\n",
                strerror(errno)
            );
            _exit(127);
        }
    }

    /*
     * After dup2(), the child no longer needs any
     * original pipe descriptors.
     */
    close_all_pipes(
        pipes,
        command_count - 1
    );

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
    char *commands[][MAX_ARGS],
    size_t command_count,
    pid_t shell_pgid,
    const char *input_path,
    const char *output_path
)
{
    int pipes[MAX_COMMANDS - 1][2];
    size_t pipe_count = command_count - 1;

    /*
     * N commands need N - 1 pipes.
     */
    for (size_t i = 0; i < pipe_count; ++i) {
        if (pipe(pipes[i]) == -1) {
            fprintf(
                stderr,
                "pipe failed: %s\n",
                strerror(errno)
            );

            close_all_pipes(pipes, i);
            return -1;
        }
    }

    pid_t pipeline_pgid = 0;

    /*
     * Create one process for every command.
     */
    for (size_t i = 0; i < command_count; ++i) {
        pid_t pid = fork();

        if (pid == -1) {
            fprintf(
                stderr,
                "fork failed: %s\n",
                strerror(errno)
            );

            close_all_pipes(pipes, pipe_count);

            if (pipeline_pgid != 0) {
                kill(-pipeline_pgid, SIGTERM);

                while (
                    waitpid(
                        -pipeline_pgid,
                        NULL,
                        0
                    ) != -1
                ) {
                }
            }

            return -1;
        }

        if (pid == 0) {
            /*
             * First child creates the PGID.
             * Later children join it.
             */
            pid_t child_pgid =
                pipeline_pgid == 0
                    ? 0
                    : pipeline_pgid;

            exec_pipeline_child(
                commands[i],
                child_pgid,
                i,
                command_count,
                pipes,
                input_path,
                output_path
            );
        }

        /*
         * PID of the first process becomes
         * the PGID of the whole pipeline.
         */
        if (pipeline_pgid == 0) {
            pipeline_pgid = pid;
        }

        /*
         * Parent also calls setpgid()
         * to avoid relying on scheduling order.
         */
        if (setpgid(pid, pipeline_pgid) == -1 &&
            errno != EACCES) {
            fprintf(
                stderr,
                "parent setpgid failed: %s\n",
                strerror(errno)
            );

            close_all_pipes(pipes, pipe_count);

            kill(-pipeline_pgid, SIGTERM);

            while (
                waitpid(
                    -pipeline_pgid,
                    NULL,
                    0
                ) != -1
            ) {
            }

            return -1;
        }
    }

    /*
     * The shell must not keep pipe ends open.
     */
    close_all_pipes(pipes, pipe_count);

    /*
     * Give the terminal to the whole pipeline.
     */
    if (tcsetpgrp(
            STDIN_FILENO,
            pipeline_pgid
        ) == -1) {
        fprintf(
            stderr,
            "tcsetpgrp pipeline failed: %s\n",
            strerror(errno)
        );
        return -1;
    }

    size_t remaining = command_count;
    int stopped = 0;

    /*
     * Negative PID means:
     * wait for any child in this process group.
     */
    while (remaining > 0) {
        int status;

        pid_t result = waitpid(
            -pipeline_pgid,
            &status,
            WUNTRACED
        );

        if (result == -1) {
            if (errno == EINTR) {
                continue;
            }

            fprintf(
                stderr,
                "pipeline waitpid failed: %s\n",
                strerror(errno)
            );
            break;
        }

        if (WIFSTOPPED(status)) {
            printf(
                "[shell] pipeline process %ld "
                "stopped by signal %d\n",
                (long)result,
                WSTOPSIG(status)
            );

            stopped = 1;
            break;
        }

        if (WIFEXITED(status) ||
            WIFSIGNALED(status)) {
            --remaining;
        }
    }

    /*
     * Shell takes the terminal back.
     */
    if (tcsetpgrp(
            STDIN_FILENO,
            shell_pgid
        ) == -1) {
        fprintf(
            stderr,
            "tcsetpgrp shell failed: %s\n",
            strerror(errno)
        );
        return -1;
    }

    if (stopped) {
        if (cleanup_stopped_job(
                pipeline_pgid
            ) == -1) {
            return -1;
        }

        while (
            waitpid(
                -pipeline_pgid,
                NULL,
                0
            ) != -1
        ) {
        }

        if (errno != ECHILD) {
            fprintf(
                stderr,
                "cleanup waitpid failed: %s\n",
                strerror(errno)
            );
            return -1;
        }
    }

    printf("[shell] pipeline finished\n");

    return 0;
}

int main(void)
{
    if (!isatty(STDIN_FILENO)) {
        fprintf(
            stderr,
            "mini-shell: stdin is not a terminal\n"
        );
        return EXIT_FAILURE;
    }

    pid_t shell_pgid = getpgrp();

    if (set_signal(SIGINT, SIG_IGN) == -1 ||
        set_signal(SIGTSTP, SIG_IGN) == -1 ||
        set_signal(SIGTTOU, SIG_IGN) == -1) {
        fprintf(
            stderr,
            "sigaction failed: %s\n",
            strerror(errno)
        );
        return EXIT_FAILURE;
    }

    char *line = NULL;
    size_t capacity = 0;

    for (;;) {
    reap_background_children();

    printf("mini$ ");
        fflush(stdout);

        errno = 0;

        ssize_t length =
            getline(&line, &capacity, stdin);

        if (length == -1) {
            if (feof(stdin)) {
                printf("\n");
                break;
            }

            fprintf(
                stderr,
                "getline failed: %s\n",
                strerror(errno)
            );

            free(line);
            return EXIT_FAILURE;
        }

        /*
         * commands[command][argument]
         *
         * Example:
         *
         * echo hello | tr a-z A-Z | wc -c
         *
         * commands[0] = {"echo", "hello", NULL}
         * commands[1] = {"tr", "a-z", "A-Z", NULL}
         * commands[2] = {"wc", "-c", NULL}
         */
        char *commands[MAX_COMMANDS][MAX_ARGS];
        size_t argcs[MAX_COMMANDS] = {0};
        size_t command_count = 1;

        char *input_path = NULL;
        char *output_path = NULL;

        /*
         * Remember which command contained
         * each redirection.
         */
        size_t input_command = 0;
        size_t output_command = 0;

        int syntax_error = 0;
        int background = 0;

        char *saveptr = NULL;

        char *token =
            strtok_r(
                line,
                " \t\n",
                &saveptr
            );

        while (token != NULL) {
            size_t current =
                command_count - 1;

            if (strcmp(token, "&") == 0) {
                if (background) {
                    fprintf(
                        stderr,
                        "mini-shell: multiple & operators\n"
                    );
                    syntax_error = 1;
                    break;
                }

                background = 1;

                token = strtok_r(
                    NULL,
                    " \t\n",
                    &saveptr
                );

                if (token != NULL) {
                    fprintf(
                        stderr,
                        "mini-shell: & must appear at the end\n"
                    );
                    syntax_error = 1;
                }

                break;
            } else if (strcmp(token, "|") == 0) {
                if (argcs[current] == 0) {
                    fprintf(
                        stderr,
                        "mini-shell: expected command before |\n"
                    );
                    syntax_error = 1;
                    break;
                }

                if (command_count >= MAX_COMMANDS) {
                    fprintf(
                        stderr,
                        "mini-shell: too many pipeline commands\n"
                    );
                    syntax_error = 1;
                    break;
                }

                ++command_count;
            } else if (
                strcmp(token, "<") == 0
            ) {
                if (input_path != NULL) {
                    fprintf(
                        stderr,
                        "mini-shell: multiple "
                        "input redirects\n"
                    );

                    syntax_error = 1;
                    break;
                }

                token =
                    strtok_r(
                        NULL,
                        " \t\n",
                        &saveptr
                    );

                if (token == NULL ||
                    strcmp(token, "<") == 0 ||
                    strcmp(token, ">") == 0 ||
                    strcmp(token, "|") == 0) {
                    fprintf(
                        stderr,
                        "mini-shell: expected "
                        "filename after <\n"
                    );

                    syntax_error = 1;
                    break;
                }

                input_path = token;
                input_command = current;
            } else if (
                strcmp(token, ">") == 0
            ) {
                if (output_path != NULL) {
                    fprintf(
                        stderr,
                        "mini-shell: multiple "
                        "output redirects\n"
                    );

                    syntax_error = 1;
                    break;
                }

                token =
                    strtok_r(
                        NULL,
                        " \t\n",
                        &saveptr
                    );

                if (token == NULL ||
                    strcmp(token, "<") == 0 ||
                    strcmp(token, ">") == 0 ||
                    strcmp(token, "|") == 0) {
                    fprintf(
                        stderr,
                        "mini-shell: expected "
                        "filename after >\n"
                    );

                    syntax_error = 1;
                    break;
                }

                output_path = token;
                output_command = current;
            } else {
                if (argcs[current] >=
                    MAX_ARGS - 1) {
                    fprintf(
                        stderr,
                        "mini-shell: too many "
                        "arguments\n"
                    );

                    syntax_error = 1;
                    break;
                }

                commands[current]
                        [argcs[current]++] =
                    token;
            }

            token =
                strtok_r(
                    NULL,
                    " \t\n",
                    &saveptr
                );
        }

        if (syntax_error) {
            continue;
        }

        /*
         * execvp() requires NULL-terminated argv.
         */
        for (
            size_t i = 0;
            i < command_count;
            ++i
        ) {
            commands[i][argcs[i]] = NULL;
        }

        if (argcs[0] == 0) {
            continue;
        }

        if (command_count > 1) {
            if (background) {
    fprintf(
        stderr,
        "mini-shell: background pipelines "
        "are not supported yet\n"
    );
    continue;
}
            if (
                argcs[command_count - 1] == 0
            ) {
                fprintf(
                    stderr,
                    "mini-shell: expected "
                    "command after |\n"
                );

                continue;
            }

            /*
             * For now:
             *
             * input redirect  → first command only
             * output redirect → last command only
             */
            if (input_path != NULL &&
                input_command != 0) {
                fprintf(
                    stderr,
                    "mini-shell: input redirection "
                    "is only supported on the first "
                    "pipeline command\n"
                );

                continue;
            }

            if (output_path != NULL &&
                output_command !=
                    command_count - 1) {
                fprintf(
                    stderr,
                    "mini-shell: output redirection "
                    "is only supported on the last "
                    "pipeline command\n"
                );

                continue;
            }
            if (background &&
    (strcmp(commands[0][0], "exit") == 0 ||
     strcmp(commands[0][0], "cd") == 0)) {
    fprintf(
        stderr,
        "mini-shell: builtins cannot run in background yet\n"
    );
    continue;
}

            int builtin_in_pipeline = 0;

            for (
                size_t i = 0;
                i < command_count;
                ++i
            ) {
                if (
                    strcmp(
                        commands[i][0],
                        "cd"
                    ) == 0 ||
                    strcmp(
                        commands[i][0],
                        "exit"
                    ) == 0
                ) {
                    builtin_in_pipeline = 1;
                    break;
                }
            }

            if (builtin_in_pipeline) {
                fprintf(
                    stderr,
                    "mini-shell: builtins in "
                    "pipelines are not "
                    "supported yet\n"
                );

                continue;
            }

            if (run_pipeline(
                    commands,
                    command_count,
                    shell_pgid,
                    input_path,
                    output_path
                ) == -1) {
                free(line);
                return EXIT_FAILURE;
            }

            continue;
        }

        /*
         * exit builtin
         */
        if (
            strcmp(
                commands[0][0],
                "exit"
            ) == 0
        ) {
            if (input_path != NULL ||
                output_path != NULL) {
                fprintf(
                    stderr,
                    "mini-shell: redirection for "
                    "builtins is not supported yet\n"
                );

                continue;
            }

            break;
        }

        /*
         * cd builtin
         */
        if (
            strcmp(
                commands[0][0],
                "cd"
            ) == 0
        ) {
            if (input_path != NULL ||
                output_path != NULL) {
                fprintf(
                    stderr,
                    "mini-shell: redirection for "
                    "builtins is not supported yet\n"
                );

                continue;
            }

            if (argcs[0] > 2) {
                fprintf(
                    stderr,
                    "mini-shell: cd: "
                    "too many arguments\n"
                );

                continue;
            }

            const char *directory;

            if (argcs[0] == 1) {
                directory = getenv("HOME");

                if (directory == NULL) {
                    fprintf(
                        stderr,
                        "mini-shell: cd: "
                        "HOME is not set\n"
                    );

                    continue;
                }
            } else {
                directory =
                    commands[0][1];
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
         * Single external command.
         */
        if (run_command(
        commands[0],
        shell_pgid,
        input_path,
        output_path,
        background
    ) == -1) {
            free(line);
            return EXIT_FAILURE;
        }
    }

    free(line);
    return EXIT_SUCCESS;
}

