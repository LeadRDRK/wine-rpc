#include "app_list.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <search.h>
#include <fcntl.h>
#include "log.h"

unsigned long djb2(const unsigned char *str)
{
    unsigned long hash = 5381;
    int c;

    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

char get_escaped_char(char c)
{
    // subset of c escape sequences
    switch (c)
    {
    case '"': return '"';
    case 'n': return '\n';
    case 'r': return '\r';
    case 't': return '\t';
    case 'v': return '\v';
    case '\'': return '\'';
    case '\\': return '\\';
    default:
        LOG_ERROR("App list parser: Unknown escape sequence: \\%c", c);
        return '\0';
    }
}

#define BUFFER_SIZE 10
#define MAX_STR_LENGTH 128
size_t app_list_parse_file(const char* filename, AppEntry** entries)
{
    FILE* fp = fopen(filename, "r");
    if (fp == NULL) return -1;

    // allocate a buffer of 10 entries
    int cur_size = BUFFER_SIZE;
    *entries = malloc(sizeof(AppEntry) * cur_size);

    int depth = 0;
    bool in_str = false;
    bool in_entry = false;
    int entry_index = 0;
    int value_index = 0;
    bool value_entered = false;
    char str[128];
    int str_index = 0;
    int c;
    while ((c = fgetc(fp)) != EOF)
    {
        switch (c)
        {
        case '[':
            ++depth;
            if (depth > 2)
            {
                LOG_ERROR("App list parser: Array too deep (> 2)\n");
                free(*entries);
                return -1;
            }
            if (depth == 2) in_entry = true;
            break;

        case ']':
            --depth;
            if (depth < 0)
            {
                LOG_ERROR("App list parser: Syntax error / Trailing array closure\n");
                free(*entries);
                return -1;
            }
            if (depth == 1)
            {
                if (value_index < 2)
                {
                    LOG_ERROR("App list parser: Insufficient entry values (expected 3, got %d)\n", value_index + 1);
                    free(*entries);
                    return -1;
                }
                in_entry = false;
            }
            value_index = 0;
            value_entered = false;
            break;

        case '"':
            in_str = !in_str;
            if (value_entered && in_str)
            {
                LOG_ERROR("App list parser: Syntax error / Unexpected string opening\n");
                free(*entries);
                return -1;
            }
            if (!in_str)
            {
                // write the null terminator
                str[str_index++] = '\0';

                AppEntry* entry = &(*entries)[entry_index];
                switch (value_index)
                {
                case 0:
                    entry->key = djb2((const unsigned char*)str);
                    break;
                case 1:
                    memcpy(entry->name, str, str_index);
                    break;
                case 2:
                    memcpy(entry->image, str, str_index);
                    break;
                }
                str_index = 0;
                value_entered = true;
            }
            break;

        case ',':
            if (in_entry && !value_entered)
            {
                LOG_ERROR("App list parser: Syntax error / Unexpected value separator (,)\n");
                free(*entries);
                return -1;
            }
            if (depth == 1)
            {
                ++entry_index;
                if (cur_size <= entry_index)
                {
                    cur_size += BUFFER_SIZE;
                    *entries = realloc(*entries, sizeof(AppEntry) * cur_size);
                }
            }
            else if (depth == 2)
            {
                value_entered = false;
                ++value_index;
            }
            break;

        // ignore whitespaces
        case ' ':
        case '\n':
        case '\r':
        case '\t':
            if (!in_str) break;
            // if in string then fallthrough...

        default:
            if (in_str)
            {
                if (c == '\\')
                {
                    char escaped = get_escaped_char(fgetc(fp));
                    if (escaped == '\0')
                        return -1;
                    str[str_index++] = escaped;
                }
                else str[str_index++] = c;

                if (str_index > MAX_STR_LENGTH)
                {
                    LOG_ERROR("App list parser: String too long (> %d characters)\n", MAX_STR_LENGTH);
                    free(*entries); 
                    return -1;
                }
                break;
            }

            LOG_ERROR("App list parser: Syntax error / Invalid character '%c'\n", c);
            free(*entries);
            return -1;
        }
    }

    return entry_index + 1;
}

int compare(const void *_l, const void *_r)
{
    const AppEntry* l = _l;
    const AppEntry* r = _r;

    if (l->key > r->key) return 1;
    else if (l->key < r->key) return -1;
    else return 0;
}

int app_list_init(AppList* list)
{
    list->root = NULL;
    size_t count = app_list_parse_file("app_list.txt", &list->entries);
    if (count == -1) return 0;
    LOG_INFO("App list: Loaded %lu entries\n", count);
    // insert into binary tree
    for (int i = 0; i < count; ++i)
    {
        tsearch(&list->entries[i], &list->root, compare);
    }
    return 1;
}

const AppEntry* app_list_get_entry(AppList* list, const char* exec_name)
{
    AppEntry e;
    e.key = djb2((const unsigned char*)exec_name);

    void* r = tfind(&e, &list->root, compare);
    if (r == NULL) return NULL;
    return (*(AppEntry**)r);
}

void app_list_destroy(AppList* list)
{
    tdelete(list->root, &list->root, compare);
    free(list->entries);
}