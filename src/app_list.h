#pragma once
#include <stddef.h>

typedef struct AppEntry
{
    unsigned long key;
    char name[128];
    char image[128];
} AppEntry;

typedef struct AppList
{
    void* root;
    AppEntry* entries;
} AppList;


size_t app_list_parse_file(const char* filename, AppEntry** app_list);
int app_list_init(AppList* list);
const AppEntry* app_list_get_entry(AppList* list, const char* exec_name);
void app_list_destroy(AppList* list);