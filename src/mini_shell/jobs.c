#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "mini_shell.h"

struct Job *find_job_by_id(struct Job jobs[], int id)
{
    for (size_t i = 0; i < MAX_JOBS; ++i) {
        if (jobs[i].used && jobs[i].id == id) {
            return &jobs[i];
        }
    }

    return NULL;
}

int foreground_job(struct Job *job, pid_t shell_pgid)
{
    /*
     * Give the terminal to the whole job process group.
     */
    if (tcsetpgrp(STDIN_FILENO, job->pgid) == -1) {
        fprintf(stderr, "mini-shell: fg: tcsetpgrp failed: %s\n",
                strerror(errno));
        return -1;
    }

    /*
     * Resume the whole process group if the job was stopped.
     */
    if (job->state == JOB_STOPPED) {
        if (kill(-job->pgid, SIGCONT) == -1) {
            fprintf(stderr, "mini-shell: fg: SIGCONT failed: %s\n",
                    strerror(errno));

            if (tcsetpgrp(STDIN_FILENO, shell_pgid) == -1) {
                fprintf(stderr,
                        "mini-shell: fg: "
                        "could not reclaim terminal: %s\n",
                        strerror(errno));
            }

            return -1;
        }

        job->state = JOB_RUNNING;
    }

    size_t stopped_count = 0;
    int wait_failed = 0;

    /*
     * A job may contain more than one process.
     *
     * Keep waiting for any child in the process group until:
     *
     * - every process has exited, or
     * - every remaining process has stopped.
     */
    while (job->remaining > 0) {
        int status;
        pid_t result;

        for (;;) {
            result = waitpid(-job->pgid, &status, WUNTRACED);

            if (result == -1 && errno == EINTR) {
                continue;
            }

            break;
        }

        if (result == -1) {
            fprintf(stderr, "mini-shell: fg: waitpid failed: %s\n",
                    strerror(errno));
            wait_failed = 1;
            break;
        }

        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            --job->remaining;

            if (job->remaining == 0) {
                job->used = 0;
                break;
            }

            /*
             * Some processes may already be stopped while another
             * process exits. In that case all surviving processes
             * can now be stopped.
             */
            if (stopped_count >= job->remaining) {
                job->state = JOB_STOPPED;

                printf("[%d] Stopped    %s\n", job->id, job->command);
                break;
            }

            continue;
        }

        if (WIFSTOPPED(status)) {
            ++stopped_count;

            /*
             * Ctrl-Z is delivered to the foreground process group.
             * Wait until all remaining members have reported stop.
             */
            if (stopped_count >= job->remaining) {
                job->state = JOB_STOPPED;

                printf("[%d] Stopped    %s\n", job->id, job->command);
                break;
            }
        }
    }

    /*
     * Shell takes the terminal back.
     */
    if (tcsetpgrp(STDIN_FILENO, shell_pgid) == -1) {
        fprintf(stderr,
                "mini-shell: fg: "
                "could not reclaim terminal: %s\n",
                strerror(errno));
        return -1;
    }

    if (wait_failed) {
        return -1;
    }

    return 0;
}

int background_job(struct Job *job)
{
    if (job->state != JOB_STOPPED) {
        fprintf(stderr, "mini-shell: bg: job is not stopped\n");
        return -1;
    }

    /*
     * Send SIGCONT to the whole process group.
     */
    if (kill(-job->pgid, SIGCONT) == -1) {
        fprintf(stderr, "mini-shell: bg: SIGCONT failed: %s\n",
                strerror(errno));
        return -1;
    }

    job->state = JOB_RUNNING;

    printf("[%d] Running    %s\n", job->id, job->command);

    return 0;
}

void print_jobs(const struct Job jobs[])
{
    for (size_t i = 0; i < MAX_JOBS; ++i) {
        if (!jobs[i].used) {
            continue;
        }

        const char *state =
            jobs[i].state == JOB_RUNNING ? "Running" : "Stopped";

        printf("[%d] %-8s %s\n", jobs[i].id, state, jobs[i].command);
    }
}

int add_job_text(struct Job jobs[], int *next_job_id, pid_t pgid,
                 size_t process_count, const char *command)
{
    for (size_t i = 0; i < MAX_JOBS; ++i) {
        if (jobs[i].used) {
            continue;
        }

        jobs[i].used = 1;
        jobs[i].id = *next_job_id;
        jobs[i].pgid = pgid;
        jobs[i].state = JOB_RUNNING;
        jobs[i].remaining = process_count;

        ++(*next_job_id);

        snprintf(jobs[i].command, sizeof(jobs[i].command), "%s", command);

        return jobs[i].id;
    }

    return -1;
}

int add_job(struct Job jobs[], int *next_job_id, pid_t pgid,
            size_t process_count, char *argv[])
{
    char command[MAX_JOB_COMMAND] = "";
    size_t offset = 0;

    for (size_t i = 0; argv[i] != NULL; ++i) {
        int written = snprintf(command + offset, sizeof(command) - offset,
                               "%s%s", i == 0 ? "" : " ", argv[i]);

        if (written < 0) {
            return -1;
        }

        size_t amount = (size_t)written;

        if (amount >= sizeof(command) - offset) {
            break;
        }

        offset += amount;
    }

    return add_job_text(jobs, next_job_id, pgid, process_count, command);
}

void reap_background_children(struct Job jobs[])
{
    for (size_t i = 0; i < MAX_JOBS; ++i) {
        if (!jobs[i].used) {
            continue;
        }

        /*
         * A stopped job has no exiting children to reap
         * until it is continued.
         */
        if (jobs[i].state == JOB_STOPPED) {
            continue;
        }

        for (;;) {
            int status;

            /*
             * Negative PGID means:
             * wait for any child in this process group.
             */
            pid_t pid = waitpid(-jobs[i].pgid, &status, WNOHANG);

            if (pid == 0) {
                break;
            }

            if (pid == -1) {
                if (errno == EINTR) {
                    continue;
                }

                if (errno == ECHILD) {
                    break;
                }

                fprintf(stderr, "background waitpid failed: %s\n",
                        strerror(errno));
                break;
            }

            if (WIFEXITED(status)) {
                printf("[shell] background pid=%ld "
                       "exited with status %d\n",
                       (long)pid, WEXITSTATUS(status));
            } else if (WIFSIGNALED(status)) {
                printf("[shell] background pid=%ld "
                       "terminated by signal %d\n",
                       (long)pid, WTERMSIG(status));
            }

            if (jobs[i].remaining > 0) {
                --jobs[i].remaining;
            }

            /*
             * A job is complete only when every process
             * in its process group has exited.
             */
            if (jobs[i].remaining == 0) {
                printf("[%d] Done    %s\n", jobs[i].id, jobs[i].command);

                jobs[i].used = 0;
                break;
            }
        }
    }
}
