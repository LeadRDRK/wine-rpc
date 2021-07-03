#pragma once
#include <stddef.h>
#include <limits.h>
#include <stdint.h>
#include <sys/types.h>

typedef struct Process
{
    pid_t pid;
    char exe[128];
    char argv0[PATH_MAX];
} Process;

int procs_get(Process** procs, size_t* count, size_t* buf_size);
int procs_is_running(pid_t pid);