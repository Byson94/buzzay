#pragma once

#include "server.h"

char* resolve_plugin_path(const char* plugin);
void handle_plugin(char *path, struct buzzay_server *buzzay_server);
