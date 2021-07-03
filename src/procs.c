#include "procs.h"
#define BUFFER_SIZE 10
#define PATH_LEN 32
#define PROCFS_PATH "/proc"

#if defined(__linux__)

#include <sys/stat.h>
#include <dirent.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

int procs_get(Process** procs, size_t* count, size_t* buf_size)
{
    *count = 0;
    if (*buf_size < BUFFER_SIZE) *buf_size = BUFFER_SIZE;
    if (*procs == NULL) *procs = malloc(sizeof(Process) * (*buf_size));
    else *procs = realloc(*procs, sizeof(Process) * (*buf_size));

    DIR* d = opendir(PROCFS_PATH);
    if (d == NULL) return 0;

    struct dirent *dir;
    char path[PATH_LEN];
    char exe_link_path[PATH_LEN];
    char cmdline_path[PATH_LEN];
    while ((dir = readdir(d)) != NULL) {
        // get the full path
        // if this is too long it's probably not a pid dir
        if (snprintf(path, PATH_LEN, PROCFS_PATH "/%s", dir->d_name) >= PATH_LEN) continue;
        pid_t pid = atoi(dir->d_name);
        if (pid == 0) continue;

        // on unsupported file systems this will be unknown
        if (dir->d_type != DT_UNKNOWN)
        {
            if (dir->d_type != DT_DIR) continue;
        }
        else // in which case we need to stat it
        {
            struct stat sb;
            if (stat(path, &sb) == -1) continue;
            if (!S_ISDIR(sb.st_mode)) continue;
        }
        // but does it happen in procfs????

        if (snprintf(exe_link_path, PATH_LEN, "%s/%s", path, "exe") >= PATH_LEN) continue;
        if (snprintf(cmdline_path, PATH_LEN, "%s/%s", path, "cmdline") >= PATH_LEN) continue;

        if (access(exe_link_path, R_OK) == 0 && access(cmdline_path, R_OK) == 0)
        {
            // read the entry's values
            char* exe_path = realpath(exe_link_path, NULL);
            if (exe_path == NULL) continue;

            FILE* fp = fopen(cmdline_path, "r");
            if (fp == NULL) continue;

            if (++(*count) > *buf_size)
            {
                *buf_size += BUFFER_SIZE;
                *procs = realloc(*procs, sizeof(Process) * (*buf_size));
            }
            Process* entry = &((*procs)[*count - 1]);

            // write the entry
            entry->pid = pid;
            strcpy(entry->exe, exe_path);
            free(exe_path);

            // read until \0 (end of the first argv)
            int i = 0;
            char c;
            while (1)
            {
                c = fgetc(fp);
                entry->argv0[i++] = c;
                if (c == '\0') break;
            }

            fclose(fp);
        }
    }
    closedir(d);

    return 1;
}

int procs_is_running(pid_t pid)
{
    char path[PATH_LEN];
    if (snprintf(path, PATH_LEN, PROCFS_PATH "/%u", pid) >= PATH_LEN) return 0;
    return access(path, F_OK) == 0;
}

#else
#error "Unsupported platform, can't determine implementation for getting processes."
#endif