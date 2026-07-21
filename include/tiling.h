#pragma once

#include "server.h"

void server_output_layout_changed(struct wl_listener *listener, void *data);
void arrange_workspaces_monocle(struct buzzay_server *server);
void arrange_workspaces_tiling(struct buzzay_server *server);
void arrange_workspaces(struct buzzay_server *server);

void focus_next_monocle(struct buzzay_server *server);
void update_border_colors(struct buzzay_server *server);
