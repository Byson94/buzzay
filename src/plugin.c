#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <limits.h>
#include <dlfcn.h>

#include "plugin.h"
#include "server.h"

struct plugin_data *plugin_array = NULL;
int plugin_count = 0;
int plugin_capacity = 0;

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

void handle_plugin(char *path, const char *plugin_name, struct buzzay_server *server) {
    void *handle = dlopen(path, RTLD_LAZY);
    if (!handle) {
        printf("Failed to dlopen plugin: '%s'. Reason: %s\n", path, dlerror());
        dlclose(handle);
        return;
    }

    // verify version
    char *api_sym_str = "buzzay_api_version";
    void *api_sym = dlsym(handle, api_sym_str);
    if (!api_sym) {
        printf("Symbol not found: '%s'\n", api_sym_str);
        dlclose(handle);
        return;
    }

    typedef int (*api_version)();
    api_version api_func = (api_version) api_sym;
    int api_ver = api_func();

    if (api_ver != BUZZAY_API_VERSION) {
        printf("Mismatch in buzzay API version found.\n");
        dlclose(handle);
        return;
    }

    // init plugin
    char *init_sym_str = "init_plugin";
    void *init_sym = dlsym(handle, init_sym_str);
    if (!init_sym) {
        printf("Symbol not found: '%s'\n", init_sym_str);
        dlclose(handle);
        return;
    }

    typedef void (*init_plugin)(struct buzzay_server *server);
    init_plugin init_func = init_sym;
    init_func(server);

    // save the plugin
    if (plugin_count >= plugin_capacity) {
        int new_capacity = (plugin_capacity == 0) ? 1 : plugin_capacity * 2;
        struct plugin_data *temp = realloc(plugin_array, new_capacity * sizeof(struct plugin_data));
        
        if (temp == NULL) {
            fprintf(stderr, "Failed to grow array\n");
            return; 
        }
        
        plugin_array = temp;
        plugin_capacity = new_capacity;
    }

    plugin_array[plugin_count].name = plugin_name;
    plugin_array[plugin_count].handle = handle;

    plugin_count++;
}

void msg_plugin(const char *plugin_name, int argc, char **argv) {
    void *handle = NULL;
    for (int i = 0; i <= plugin_count; i++) {
        if (strcmp(plugin_array[i].name, plugin_name) == 0) {
            handle = plugin_array[i].handle;
        }
    }

    if (handle == NULL) {
        printf("Plugin with name '%s' not found.\n", plugin_name);
        return;
    }

    char *msg_sym_str = "msg_request";
    void *msg_sym = dlsym(handle, msg_sym_str);
    if (!msg_sym) {
        printf("Message endpoint not found in: '%s'\n", msg_sym_str);
        return;
    }

    typedef void (*msg_request)(int argc, char **argv);
    msg_request msg_func = msg_sym;
    msg_func(argc, argv);
}
