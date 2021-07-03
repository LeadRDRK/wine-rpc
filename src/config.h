#pragma once

typedef struct Config
{
    int show_unrecognized_apps;
    int hide_exec_name;
} Config;

int config_load_from_file(Config* config);
int config_init_file();