#pragma once

#include "buzzay-plugin.h"
#include "server.h"

struct plugin_data {
    struct bz_plugin *plugin;
    void *handle;
};

struct keybinding_data {
    int id;
    struct bz_keybinding binding;
    struct bz_plugin *owner;
};

extern struct plugin_data *plugin_array;
extern int plugin_count;
extern int plugin_capacity;

extern struct keybinding_data *keybinding_arr;
extern int keybinding_count;
extern int keybinding_capacity;

char *resolve_plugin_path(const char *plugin);
void handle_plugin(char *path, const char *plugin_name, struct buzzay_server *buzzay_server, int client_fd);
void msg_plugin(const char *plugin_name, int argc, char **argv, int client_fd);
