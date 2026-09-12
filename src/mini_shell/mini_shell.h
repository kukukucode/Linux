#ifndef MINI_SHELL_H
#define MINI_SHELL_H

#include <stddef.h>
#include <sys/types.h>

#define MAX_ARGS 64
#define MAX_COMMANDS 16
#define MAX_JOBS 16
#define MAX_JOB_COMMAND 256

enum JobState {
    JOB_RUNNING,
    JOB_STOPPED
};

struct Job {
    int used;
    int id;
    pid_t pgid;
    enum JobState state;
    size_t remaining;
    char command[MAX_JOB_COMMAND];
};

#endif
