#include <string.h>
#define VERSION "1.0.0"
// change this if you want to use your own application
#define CLIENT_ID 859779025593434122

#define PROC_INTERVAL 3
#define CALLBACK_INTERVAL 3
#define PF_BUFFER_SIZE 10

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <search.h>
#include <discord_game_sdk.h>
#include "app_list.h"
#include "log.h"
#include "procs.h"
#include "config.h"

int get_wine_version(char* buf, size_t buf_size)
{
    FILE *fp = popen("wine --version", "r");
    if (fp == NULL) return 0;
    fgets(buf, buf_size, fp);

    return 1;
}

const char* get_filename(const char* argv0)
{
    size_t i = strlen(argv0);
    // read backwards from the null terminator to find the last slash
    while (argv0[i] != '\\' && argv0[i] != '/')
    {
        if (i == 0) return argv0;
        --i;
    }
    // return the pointer starting from the character next to the slash
    return argv0 + (++i);
}

int is_system_proc(const char* exec_name)
{
    static const char* system_procs[] = {
        "wine",
        "wine-preloader",
        "wineboot.exe",
        "winemenubuilder.exe",
        "tart.exe",
        "services.exe",
        "svchost.exe",
        "plugplay.exe",
        "winedevice.exe",
        "rpcss.exe",
        "explorer.exe"
    };
    static const int procs_count = sizeof(system_procs) / sizeof(char*);

    for (int i = 0; i < procs_count; ++i)
    {
        if (strcmp(exec_name, system_procs[i]) == 0) return 1;
    }
    return 0;
}

int init_discord(struct IDiscordCore** core)
{
    struct DiscordCreateParams params;
    memset(&params, 0, sizeof(params));
    params.client_id = CLIENT_ID;
    params.flags = DiscordCreateFlags_Default;

    enum EDiscordResult result = DiscordCreate(DISCORD_VERSION, &params, core);
    if (result != DiscordResult_Ok)
        return 0;

    return 1;
}

void daemonize()
{
    // 1st fork
    pid_t pid;
    pid = fork();
    
    if (pid == -1)
    {
        LOG_ERROR("Failed to fork off parent process\n");
        exit(1);
    }
    else if (pid > 0)
    {
        LOG_INFO("Daemon started\n");
        exit(0);
    }

    if (setsid() < 0)
        exit(1);
    
    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);
    
    // 2nd fork
    pid = fork();

    if (pid == -1)
        exit(1);
    else if (pid > 0)
        exit(0);
    
    // close parent's inherited fds
    for (int x = sysconf(_SC_OPEN_MAX); x >= 0; x--)
    {
        close(x);
    }
}

void print_help()
{
    printf(""
        "Usage: wine-rpc [option]\n"
        "Options:\n"
        "    -d: start as a daemon\n"
        "    -k: kill all running daemons\n"
        "    --help: print this help message\n"
    );
}

typedef struct PresenceFork
{
    pid_t app_pid;
    pid_t fork_pid;
} PresenceFork;

int pf_compare(const void *_l, const void *_r)
{
    const PresenceFork* l = _l;
    const PresenceFork* r = _r;

    return l->app_pid - r->app_pid;
}

int main(int argc, char** argv)
{
    LOG_INFO("wine-rpc v" VERSION " by LeadRDRK\n");
    // check args
    if (argc > 1)
    {
        if (strcmp(argv[1], "-d") == 0)
        {
            daemonize();
        }
        else if (strcmp(argv[1], "-k") == 0)
        {
            // i am very lazy
            system("killall -9 wine-rpc");
            return 0;
        }
        else if (strcmp(argv[1], "--help") == 0)
        {
            print_help();
            exit(0);
        }
        else
        {
            LOG_ERROR("Unrecognized option: %s", argv[1]);
            print_help();
            exit(1);
        }
    }
    
    Config config;
    memset(&config, 0, sizeof(config));
    if (!config_load_from_file(&config))
    {
        LOG_WARN("Config: Failed to load from config.txt. Attempting to create new config\n");
        if (config_init_file())
            LOG_INFO("Config: New config initialized in config.txt\n");
        else
            LOG_WARN("Config: Failed to create new config file\n");
    }
    else LOG_INFO("Config: Loaded from config.txt\n");

    AppList list;
    if (!app_list_init(&list))
    {
        LOG_ERROR("Failed to load app_list.txt\n");
        return 1;
    }

    char wine_version[128];
    if (!get_wine_version(wine_version, 128))
        LOG_WARN("Failed to get Wine version.\n");
    LOG_INFO("Found Wine version: %s", wine_version);

    Process* procs = NULL;
    size_t count = 0;
    size_t buf_size = 0;
    void* pf_root = NULL;
    int pf_size = PF_BUFFER_SIZE;
    PresenceFork* pf_list = malloc(pf_size * sizeof(PresenceFork));
    int pf_count = 0;

    while (1)
    {
        // clean up forks
        for (int i = 0; i < pf_count; ++i)
        {
            if (!procs_is_running(pf_list[i].app_pid))
            {
                // kill the process and remove from binary tree
                kill(pf_list[i].fork_pid, SIGKILL);
                tdelete(&pf_list[i], &pf_root, pf_compare);
                LOG_INFO("Wine process (PID %d) ended\n", pf_list[i].app_pid);
                // move all elements after this one backwards
                for (int x = i + 1; x < pf_count; ++x)
                {
                    pf_list[x - 1] = pf_list[x];
                }
                // and change the pf count
                --pf_count;
                --i;
            }
        }

        // find new processes
        if (!procs_get(&procs, &count, &buf_size))
        {
            LOG_ERROR("Failed to get running processes\n");
            return 1;
        }

        for (int i = 0; i < count; ++i)
        {
            const char* exe = get_filename(procs[i].exe);
            if (strcmp(exe, "wine-preloader") != 0 && strcmp(exe, "wine64-preloader") != 0)
                continue;
            
            const char* exec_name = get_filename(procs[i].argv0);
            if (is_system_proc(exec_name)) continue;

            const AppEntry* entry = app_list_get_entry(&list, exec_name);
            if (!config.show_unrecognized_apps && entry == NULL) continue;

            // check if a presence is already running for this pid
            PresenceFork search;
            search.app_pid = procs[i].pid;
            if (tfind(&search, &pf_root, pf_compare) != NULL) continue;
            
            LOG_INFO("Found new Wine process: %s, PID %u\n", exec_name, procs[i].pid);

            // fork() into a new process to create a discord rich presence
            pid_t fork_pid = fork();
            int status;
            switch (fork_pid)
            {
            case -1:
                LOG_ERROR("Failed to fork into a new process, exiting\n");
                return 1;

            case 0: // child
            {
                struct IDiscordCore* core = NULL;
                if (!init_discord(&core))
                {
                    LOG_ERROR("Failed to initialize Discord.\n");
                    return 1;
                }
                struct IDiscordActivityManager* activities = core->get_activity_manager(core);

                struct DiscordActivity activity;
                memset(&activity, 0, sizeof(activity));
                strcpy(activity.state, exec_name);
                if (entry != NULL)
                {
                    LOG_INFO("Found app list entry: %s\n", entry->name);
                    strcpy(activity.details, entry->name);
                    strcpy(activity.assets.large_image, entry->image);
                    strcpy(activity.assets.large_text, entry->name);
                    strcpy(activity.assets.small_image, "wine_logo");
                    strcpy(activity.assets.small_text, wine_version);
                    if (config.hide_exec_name)
                        activity.state[0] = '\0';
                }
                else {
                    strcpy(activity.assets.large_image, "wine_logo");
                    strcpy(activity.assets.large_text, wine_version);
                }
                activity.timestamps.start = time(NULL);

                activities->update_activity(activities, &activity, NULL, NULL);

                // keep the forked process as light as possible by freeing
                // stuff we don't need anymore
                free(procs);
                app_list_destroy(&list);
                tdelete(pf_root, &pf_root, pf_compare);
                free(pf_list);

                while (1)
                {
                    core->run_callbacks(core);
                    sleep(CALLBACK_INTERVAL);
                }
                return 0;
            }
            }

            ++pf_count;
            if (pf_count == pf_size)
            {
                pf_size += PF_BUFFER_SIZE;
                pf_list = realloc(pf_list, pf_size * sizeof(PresenceFork));
            }
            size_t x = pf_count - 1;
            pf_list[x].app_pid = procs[i].pid;
            pf_list[x].fork_pid = fork_pid;
            tsearch(&pf_list[x], &pf_root, pf_compare);
        }

        sleep(PROC_INTERVAL);
    }

    free(procs);
    free(pf_list);
    app_list_destroy(&list);
    return 0;
}