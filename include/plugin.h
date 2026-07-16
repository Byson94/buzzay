#pragma once

#include "server.h"

struct plugin_data {
    const char *name;
    void *handle;
};

extern struct plugin_data *plugin_array;
extern int plugin_count;
extern int plugin_capacity;

char* resolve_plugin_path(const char *plugin);
void handle_plugin(char *path, const char *plugin_name, struct buzzay_server *buzzay_server);
void msg_plugin(const char *plugin_name, int argc, char **argv);
