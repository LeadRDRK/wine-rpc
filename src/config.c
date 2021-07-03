#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CONFIG_FILE "config.txt"

int config_load_from_file(Config* config)
{
    FILE* fp = fopen(CONFIG_FILE, "r");
    if (fp == NULL) return 0;

    char c;
    char key[32];
    char value[32];
    int char_index = 0;
    int is_key = 1;
    while ((c = fgetc(fp)) != EOF)
    {
        switch (c)
        {
        case '\n':
        {
            value[char_index] = '\0';
            int v = atoi(value);
            if (strcmp(key, "show_unrecognized_apps") == 0)
                config->show_unrecognized_apps = v;
            else if (strcmp(key, "hide_exec_name") == 0)
                config->hide_exec_name = v;
            
            is_key = 1;
            char_index = 0;
            break;
        }

        case '=':
            key[char_index] = '\0';
            is_key = 0;
            char_index = 0;
            break;

        default:
            if (is_key)
                key[char_index++] = c;
            else
                value[char_index++] = c;
            break;

        }
    }

    fclose(fp);
    return 1;
}

int config_init_file()
{
    static const char default_config[] = ""
    "show_unrecognized_apps=0\n"
    "hide_exec_name=0\n";

    FILE* fp = fopen(CONFIG_FILE, "w");
    if (fp == NULL) return 0;

    fputs(default_config, fp);
    fclose(fp);

    return 1;
}