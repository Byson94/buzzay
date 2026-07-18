#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <limits.h>
#include <dlfcn.h>
#include <wayland-server-core.h>

#include "buzzay-plugin.h"
#include "handle-plugin.h"
#include "ipc.h"

struct plugin_data *plugin_array = NULL;
int plugin_count = 0;
int plugin_capacity = 0;

char *resolve_plugin_path(const char* plugin) {
    char plugin_base[105];
    snprintf(plugin_base, sizeof(plugin_base), "%s.so", plugin);

    const char *dir_path = "/usr/lib/buzzay-plugins/";
    DIR *dir = opendir(dir_path);

    if (dir == NULL) {
        perror("Unable to open directory");
        return NULL;
    }

    struct dirent *entry;
    char *result = NULL;

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG && strcmp(entry->d_name, plugin_base) == 0) {
            size_t full_path_len = strlen(dir_path) + strlen(plugin_base) + 1;
            result = malloc(full_path_len);
            
            if (result) {
                snprintf(result, full_path_len, "%s%s", dir_path, plugin_base);
            }
            break;
        }
    }

    closedir(dir);
    return result;
}

void handle_plugin(char *path, const char *plugin_name, struct buzzay_server *server, int client_fd) {
    void *handle = dlopen(path, RTLD_LAZY);
    if (!handle) {
        char buffer[1024];
        sprintf(buffer, "Failed to dlopen plugin: '%s'. Reason: %s\n", path, dlerror());

        send_msg_back(client_fd, buffer);
        dlclose(handle);
        return;
    }

    // verify version
    char *api_sym_str = "buzzay_api_version";
    void *api_sym = dlsym(handle, api_sym_str);
    if (!api_sym) {
        char buffer[1024];
        sprintf(buffer, "Symbol not found: '%s'\n", api_sym_str);

        send_msg_back(client_fd, buffer);
        dlclose(handle);
        return;
    }

    typedef int (*api_version)();
    api_version api_func = (api_version) api_sym;
    int api_ver = api_func();

    if (api_ver != BUZZAY_API_VERSION) {
        send_msg_back(client_fd, "Mismatch in buzzay API version found.\n");
        dlclose(handle);
        return;
    }

    // init plugin
    char *init_sym_str = "init_plugin";
    void *init_sym = dlsym(handle, init_sym_str);
    if (!init_sym) {
        char buffer[1024];
        sprintf(buffer, "Symbol not found: '%s'\n", init_sym_str);
        send_msg_back(client_fd, buffer);

        dlclose(handle);
        return;
    }

    typedef void (*init_plugin)(struct bz_plugin *plugin);
    init_plugin init_func = init_sym;

    struct bz_plugin plugin = {
        .plugin_name = plugin_name,
        .plugin_path = path,
        .server = server,
    };

    init_func(&plugin);

    // save the plugin
    if (plugin_count >= plugin_capacity) {
        int new_capacity = (plugin_capacity == 0) ? 1 : plugin_capacity * 2;
        struct plugin_data *temp = realloc(plugin_array, new_capacity * sizeof(struct plugin_data));
        
        if (temp == NULL) {
            const char *msg = "Failed to grow array\n";
            fprintf(stderr, "%s", msg);
            send_msg_back(client_fd, msg);
            return; 
        }
        
        plugin_array = temp;
        plugin_capacity = new_capacity;
    }

    plugin_array[plugin_count].name = strdup(plugin_name); 
    if (plugin_array[plugin_count].name == NULL) {
        const char *msg = "Failed to allocate memory for plugin name\n";
        fprintf(stderr, "%s", msg);
        send_msg_back(client_fd, msg);
        return;
    }
    plugin_array[plugin_count].handle = handle;
    plugin_count++;

    send_msg_back(client_fd, "OK!\n");
}

void msg_plugin(const char *plugin_name, int argc, char **argv, int client_fd) {
    void *handle = NULL;
    for (int i = 0; i < plugin_count; i++) {
        if (strcmp(plugin_array[i].name, plugin_name) == 0) {
            handle = plugin_array[i].handle;
        }
    }

    if (handle == NULL) {
        char buffer[1024];
        sprintf(buffer, "Plugin with name '%s' not found.\n", plugin_name);
        send_msg_back(client_fd, buffer);
        return;
    }

    char *msg_sym_str = "msg_request";
    void *msg_sym = dlsym(handle, msg_sym_str);
    if (!msg_sym) {
        char buffer[1024];
        sprintf(buffer, "Message endpoint not found in: '%s'\n", msg_sym_str);
        send_msg_back(client_fd, buffer);
        return;
    }

    typedef void (*msg_request)(int argc, char **argv);
    msg_request msg_func = msg_sym;
    msg_func(argc, argv);

    send_msg_back(client_fd, "OK!\n");
}

// == Implement Helpers ==

BZ_API void bz_quit(struct bz_plugin *plugin) {
    wl_display_terminate(plugin->server->wl_display);
}

struct keybinding_data *keybinding_arr = NULL;
int keybinding_count = 0;
int keybinding_capacity = 0;
static int kb_next_id = 0;

BZ_API bz_binding_handle_t bz_register_keybinding(
    struct bz_plugin *plugin,
    struct bz_keybinding binding
) {
    // save the keybinding
    if (keybinding_count >= keybinding_capacity) {
        int new_capacity = (keybinding_capacity == 0) ? 1 : keybinding_capacity * 2;
        struct keybinding_data *temp = realloc(keybinding_arr, new_capacity * sizeof(struct keybinding_data));
        
        if (temp == NULL) {
            fprintf(stderr, "Failed to grow array\n");
            return -1; 
        }
        
        keybinding_arr = temp;
        keybinding_capacity = new_capacity;
    }

    int id = kb_next_id++;
    keybinding_arr[keybinding_count].id = id;
    keybinding_arr[keybinding_count].binding = binding;
    keybinding_arr[keybinding_count].owner = plugin;
    keybinding_count++;

    return id;
}

BZ_API void bz_unregister_keybinding(bz_binding_handle_t handle) {
    for (int i = 0; i < keybinding_count; i++) {
        if (keybinding_arr[i].id == handle) {
            keybinding_arr[i] = keybinding_arr[keybinding_count - 1];
            keybinding_count--;
            return;
        }
    }
}
