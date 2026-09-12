#ifndef MINI_SHELL_H
#define MINI_SHELL_H

#include <stddef.h>
#include <sys/types.h>

#define MAX_ARGS 64
#define MAX_COMMANDS 16
#define MAX_JOBS 16
#define MAX_JOB_COMMAND 256

enum JobState { JOB_RUNNING, JOB_STOPPED };

struct Job {
    int used;
    int id;
    pid_t pgid;
    enum JobState state;
    size_t remaining;
    char command[MAX_JOB_COMMAND];
};

struct ParsedLine {
    char *commands[MAX_COMMANDS][MAX_ARGS];
    size_t argcs[MAX_COMMANDS];
    size_t command_count;

    char *input_path;
    char *output_path;

    size_t input_command;
    size_t output_command;

    int background;
};

int parse_line(char *line, struct ParsedLine *parsed);

struct Job *find_job_by_id(struct Job jobs[], int id);

int wait_for_process_group(pid_t pgid, size_t *remaining, int *stopped);

int foreground_job(struct Job *job, pid_t shell_pgid);

int background_job(struct Job *job);

void print_jobs(const struct Job jobs[]);

int add_job_text(struct Job jobs[], int *next_job_id, pid_t pgid,
                 size_t process_count, const char *command);

int add_job(struct Job jobs[], int *next_job_id, pid_t pgid,
            size_t process_count, char *argv[]);

void reap_background_children(struct Job jobs[]);

int set_signal(int signal_number, void (*handler)(int));

int run_command(char *argv[], pid_t shell_pgid, const char *input_path,
                const char *output_path, int background, struct Job jobs[],
                int *next_job_id);

int run_pipeline(char *commands[][MAX_ARGS], size_t command_count,
                 pid_t shell_pgid, const char *input_path,
                 const char *output_path, int background, struct Job jobs[],
                 int *next_job_id);

#endif
