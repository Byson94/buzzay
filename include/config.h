#pragma once

struct buzzay_server;

int handle_config_only_envs(const char *path);
int handle_config_only_cursor(const char *path, struct buzzay_server *server);
int handle_config(const char *path, struct buzzay_server *server);
