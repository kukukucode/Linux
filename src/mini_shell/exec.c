#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "mini_shell.h"

int set_signal(int signal_number, void (*handler)(int))
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

static int redirect_fd(const char *path, int flags, mode_t mode, int target_fd)
{
    int fd;

    if ((flags & O_CREAT) != 0) {
        fd = open(path, flags, mode);
    } else {
        fd = open(path, flags);
    }

    if (fd == -1) {
        fprintf(stderr, "mini-shell: %s: %s\n", path, strerror(errno));
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

static int set_child_process_group(pid_t pid, pid_t pgid)
{
    if (setpgid(pid, pgid) == -1) {
        /*
         * The child may have already called exec().
         * In that case the parent's setpgid() loses the race,
         * but the child's own setpgid() has already done the work.
         */
        if (errno == EACCES) {
            return 0;
        }

        fprintf(stderr, "parent setpgid failed: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

int run_command(char *argv[], pid_t shell_pgid, const char *input_path,
                const char *output_path, int background, struct Job jobs[],
                int *next_job_id)
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
            if (redirect_fd(input_path, O_RDONLY, 0, STDIN_FILENO) == -1) {
                _exit(127);
            }
        }

        if (output_path != NULL) {
            if (redirect_fd(output_path, O_WRONLY | O_CREAT | O_TRUNC, 0666,
                            STDOUT_FILENO) == -1) {
                _exit(127);
            }
        }

        execvp(argv[0], argv);

        fprintf(stderr, "mini-shell: %s: %s\n", argv[0], strerror(errno));

        _exit(127);
    }

    if (set_child_process_group(child_pid, child_pid) == -1) {
        return -1;
    }
    if (background) {
        int job_id = add_job(jobs, next_job_id, child_pid, 1, argv);

        if (job_id == -1) {
            fprintf(stderr, "mini-shell: job table is full\n");

            kill(-child_pid, SIGTERM);
            waitpid(child_pid, NULL, 0);
            return -1;
        }

        printf("[%d] %ld\n", job_id, (long)child_pid);

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
        printf("[shell] child exited with status %d\n", WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        printf("[shell] child terminated by signal %d\n", WTERMSIG(status));
    } else if (WIFSTOPPED(status)) {
        int job_id = add_job(jobs, next_job_id, child_pid, 1, argv);

        if (job_id == -1) {
            fprintf(stderr, "mini-shell: job table is full\n");

            /*
             * Do not leave an untracked stopped child.
             */
            if (cleanup_stopped_job(child_pid) == -1) {
                return -1;
            }

            if (waitpid(child_pid, NULL, 0) == -1) {
                fprintf(stderr, "cleanup waitpid failed: %s\n",
                        strerror(errno));
                return -1;
            }

            return -1;
        }

        struct Job *job = find_job_by_id(jobs, job_id);

        if (job == NULL) {
            fprintf(stderr, "mini-shell: internal job lookup failed\n");
            return -1;
        }

        job->state = JOB_STOPPED;

        printf("[%d] Stopped    %s\n", job->id, job->command);
    }

    return 0;
}

static void close_all_pipes(int pipes[][2], size_t pipe_count)
{
    for (size_t i = 0; i < pipe_count; ++i) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
}

static void exec_pipeline_child(char *argv[], pid_t pgid, size_t index,
                                size_t command_count, int pipes[][2],
                                const char *input_path, const char *output_path)
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
            if (redirect_fd(input_path, O_RDONLY, 0, STDIN_FILENO) == -1) {
                _exit(127);
            }
        }
    } else {
        /*
         * All commands except the first read from
         * the previous pipe.
         */
        if (dup2(pipes[index - 1][0], STDIN_FILENO) == -1) {
            fprintf(stderr, "dup2 stdin failed: %s\n", strerror(errno));
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
            if (redirect_fd(output_path, O_WRONLY | O_CREAT | O_TRUNC, 0666,
                            STDOUT_FILENO) == -1) {
                _exit(127);
            }
        }
    } else {
        /*
         * All commands except the last write to
         * the next pipe.
         */
        if (dup2(pipes[index][1], STDOUT_FILENO) == -1) {
            fprintf(stderr, "dup2 stdout failed: %s\n", strerror(errno));
            _exit(127);
        }
    }

    /*
     * After dup2(), the child no longer needs any
     * original pipe descriptors.
     */
    close_all_pipes(pipes, command_count - 1);

    execvp(argv[0], argv);

    fprintf(stderr, "mini-shell: %s: %s\n", argv[0], strerror(errno));

    _exit(127);
}

int run_pipeline(char *commands[][MAX_ARGS], size_t command_count,
                 pid_t shell_pgid, const char *input_path,
                 const char *output_path)
{
    int pipes[MAX_COMMANDS - 1][2];
    size_t pipe_count = command_count - 1;

    /*
     * N commands need N - 1 pipes.
     */
    for (size_t i = 0; i < pipe_count; ++i) {
        if (pipe(pipes[i]) == -1) {
            fprintf(stderr, "pipe failed: %s\n", strerror(errno));

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
            fprintf(stderr, "fork failed: %s\n", strerror(errno));

            close_all_pipes(pipes, pipe_count);

            if (pipeline_pgid != 0) {
                kill(-pipeline_pgid, SIGTERM);

                while (waitpid(-pipeline_pgid, NULL, 0) != -1) {
                }
            }

            return -1;
        }

        if (pid == 0) {
            /*
             * First child creates the PGID.
             * Later children join it.
             */
            pid_t child_pgid = pipeline_pgid == 0 ? 0 : pipeline_pgid;

            exec_pipeline_child(commands[i], child_pgid, i, command_count,
                                pipes, input_path, output_path);
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
        if (set_child_process_group(pid, pipeline_pgid) == -1) {
            close_all_pipes(pipes, pipe_count);

            kill(-pipeline_pgid, SIGTERM);

            while (waitpid(-pipeline_pgid, NULL, 0) != -1) {
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
    if (tcsetpgrp(STDIN_FILENO, pipeline_pgid) == -1) {
        fprintf(stderr, "tcsetpgrp pipeline failed: %s\n", strerror(errno));
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

        pid_t result = waitpid(-pipeline_pgid, &status, WUNTRACED);

        if (result == -1) {
            if (errno == EINTR) {
                continue;
            }

            fprintf(stderr, "pipeline waitpid failed: %s\n", strerror(errno));
            break;
        }

        if (WIFSTOPPED(status)) {
            printf("[shell] pipeline process %ld "
                   "stopped by signal %d\n",
                   (long)result, WSTOPSIG(status));

            stopped = 1;
            break;
        }

        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            --remaining;
        }
    }

    /*
     * Shell takes the terminal back.
     */
    if (tcsetpgrp(STDIN_FILENO, shell_pgid) == -1) {
        fprintf(stderr, "tcsetpgrp shell failed: %s\n", strerror(errno));
        return -1;
    }

    if (stopped) {
        if (cleanup_stopped_job(pipeline_pgid) == -1) {
            return -1;
        }

        while (waitpid(-pipeline_pgid, NULL, 0) != -1) {
        }

        if (errno != ECHILD) {
            fprintf(stderr, "cleanup waitpid failed: %s\n", strerror(errno));
            return -1;
        }
    }

    printf("[shell] pipeline finished\n");

    return 0;
}
