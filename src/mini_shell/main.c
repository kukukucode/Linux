#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "mini_shell.h"

static int is_builtin(const char *name)
{
    return strcmp(name, "exit") == 0 || strcmp(name, "cd") == 0 ||
           strcmp(name, "jobs") == 0 || strcmp(name, "fg") == 0 ||
           strcmp(name, "bg") == 0;
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
    struct Job jobs[MAX_JOBS] = {0};
    int next_job_id = 1;

    char *line = NULL;
    size_t capacity = 0;

    for (;;) {
        reap_background_children(jobs);

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
        struct ParsedLine parsed;

        if (parse_line(line, &parsed) == -1) {
            continue;
        }

        /*
         * Local aliases keep the rest of main()
         * unchanged while parsing lives in parser.c.
         */
        char *(*commands)[MAX_ARGS] = parsed.commands;

        size_t *argcs = parsed.argcs;

        size_t command_count = parsed.command_count;

        char *input_path = parsed.input_path;

        char *output_path = parsed.output_path;

        size_t input_command = parsed.input_command;

        size_t output_command = parsed.output_command;

        int background = parsed.background;

        if (argcs[0] == 0) {
            continue;
        }

        /*
         * Builtins run inside the shell process.
         * For now they cannot be background jobs.
         */
        if (background && is_builtin(commands[0][0])) {
            fprintf(stderr,
                    "mini-shell: builtins cannot run in background yet\n");
            continue;
        }

        /*
         * Pipeline.
         */

        if (command_count > 1) {
            if (argcs[command_count - 1] == 0) {
                fprintf(stderr, "mini-shell: expected "
                                "command after |\n");

                continue;
            }

            /*
             * For now:
             *
             * input redirect  → first command only
             * output redirect → last command only
             */
            if (input_path != NULL && input_command != 0) {
                fprintf(stderr, "mini-shell: input redirection "
                                "is only supported on the first "
                                "pipeline command\n");

                continue;
            }

            if (output_path != NULL && output_command != command_count - 1) {
                fprintf(stderr, "mini-shell: output redirection "
                                "is only supported on the last "
                                "pipeline command\n");

                continue;
            }

            int builtin_in_pipeline = 0;

            for (size_t i = 0; i < command_count; ++i) {
                if (is_builtin(commands[i][0])) {
                    builtin_in_pipeline = 1;
                    break;
                }
            }

            if (builtin_in_pipeline) {
                fprintf(stderr, "mini-shell: builtins in "
                                "pipelines are not "
                                "supported yet\n");

                continue;
            }

            if (run_pipeline(commands, command_count, shell_pgid, input_path,
                             output_path, background, jobs,
                             &next_job_id) == -1) {
                free(line);
                return EXIT_FAILURE;
            }

            continue;
        }

        /*
         * jobs builtin
         */
        if (strcmp(commands[0][0], "jobs") == 0) {
            if (input_path != NULL || output_path != NULL) {
                fprintf(stderr, "mini-shell: redirection for "
                                "builtins is not supported yet\n");

                continue;
            }

            if (argcs[0] != 1) {
                fprintf(stderr, "mini-shell: jobs: "
                                "too many arguments\n");

                continue;
            }

            print_jobs(jobs);
            continue;
        }

        /*
         * bg builtin
         */
        if (strcmp(commands[0][0], "bg") == 0) {
            if (input_path != NULL || output_path != NULL) {
                fprintf(stderr, "mini-shell: redirection for "
                                "builtins is not supported yet\n");
                continue;
            }

            if (argcs[0] != 2) {
                fprintf(stderr, "mini-shell: usage: bg JOB_ID\n");
                continue;
            }

            char *end = NULL;
            errno = 0;

            long job_id = strtol(commands[0][1], &end, 10);

            if (errno != 0 || end == commands[0][1] || *end != '\0' ||
                job_id <= 0) {
                fprintf(stderr, "mini-shell: bg: invalid job id: %s\n",
                        commands[0][1]);
                continue;
            }

            struct Job *job = find_job_by_id(jobs, (int)job_id);

            if (job == NULL) {
                fprintf(stderr, "mini-shell: bg: no such job: %ld\n", job_id);
                continue;
            }

            if (background_job(job) == -1) {
                continue;
            }

            continue;
        }

        /*
         * fg builtin
         */
        if (strcmp(commands[0][0], "fg") == 0) {
            if (input_path != NULL || output_path != NULL) {
                fprintf(stderr, "mini-shell: redirection for "
                                "builtins is not supported yet\n");
                continue;
            }

            if (argcs[0] != 2) {
                fprintf(stderr, "mini-shell: usage: fg JOB_ID\n");
                continue;
            }

            char *end = NULL;
            errno = 0;

            long job_id = strtol(commands[0][1], &end, 10);

            if (errno != 0 || end == commands[0][1] || *end != '\0' ||
                job_id <= 0) {
                fprintf(stderr, "mini-shell: fg: invalid job id: %s\n",
                        commands[0][1]);
                continue;
            }

            struct Job *job = find_job_by_id(jobs, (int)job_id);

            if (job == NULL) {
                fprintf(stderr, "mini-shell: fg: no such job: %ld\n", job_id);
                continue;
            }

            printf("%s\n", job->command);
            fflush(stdout);

            if (foreground_job(job, shell_pgid) == -1) {
                free(line);
                return EXIT_FAILURE;
            }

            continue;
        }

        /*
         * exit builtin
         */
        if (strcmp(commands[0][0], "exit") == 0) {
            if (input_path != NULL || output_path != NULL) {
                fprintf(stderr, "mini-shell: redirection for "
                                "builtins is not supported yet\n");

                continue;
            }

            break;
        }

        /*
         * cd builtin
         */
        if (strcmp(commands[0][0], "cd") == 0) {
            if (input_path != NULL || output_path != NULL) {
                fprintf(stderr, "mini-shell: redirection for "
                                "builtins is not supported yet\n");

                continue;
            }

            if (argcs[0] > 2) {
                fprintf(stderr, "mini-shell: cd: "
                                "too many arguments\n");

                continue;
            }

            const char *directory;

            if (argcs[0] == 1) {
                directory = getenv("HOME");

                if (directory == NULL) {
                    fprintf(stderr, "mini-shell: cd: "
                                    "HOME is not set\n");

                    continue;
                }
            } else {
                directory = commands[0][1];
            }

            if (chdir(directory) == -1) {
                fprintf(stderr, "mini-shell: cd: %s: %s\n", directory,
                        strerror(errno));
            }

            continue;
        }

        /*
         * Single external command.
         */
        if (run_command(commands[0], shell_pgid, input_path, output_path,
                        background, jobs, &next_job_id) == -1) {
            free(line);
            return EXIT_FAILURE;
        }
    }

    free(line);
    return EXIT_SUCCESS;
}
