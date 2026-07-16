#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <limits.h>

#include "plugin.h"

char *resolve_plugin_path(const char* plugin) {
    // plugin is in the following format:
    // - plugin_name
    char plugin_base[105];
    strncpy(plugin_base, plugin, sizeof(plugin_base) - 1);
    plugin_base[sizeof(plugin_base) - 1] = '\0';
    strcat(plugin_base, ".so");

    // list files
    char path[PATH_MAX] = "/usr/lib/buzzay-plugins/";
    struct dirent *entry;
    DIR *dir = opendir(path);

    if (dir == NULL) {
        perror("Unable to open directory");
        return NULL;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            printf("File: %s\n", entry->d_name);
            if (strcmp(entry->d_name, plugin_base) == 0) {
                strcat(path, plugin_base);
            }
        }
    }


    char *final_path = path;
    closedir(dir);

    return final_path;
}

void handle_plugin(char *path, struct buzzay_server *buzzay_server) {
    // TODO: Handle plugin;
}
