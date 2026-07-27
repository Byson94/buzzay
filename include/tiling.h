#pragma once

#include "server.h"

void server_output_layout_changed(struct wl_listener *listener, void *data);
void apply_borders(struct buzzay_toplevel *toplevel, struct wlr_box box);
void arrange_workspaces_monocle(struct buzzay_server *server);
void arrange_workspaces_tiling(struct buzzay_server *server);
void arrange_workspaces(struct buzzay_server *server);

void focus_next_monocle(struct buzzay_server *server, bool is_anti_clockwise);
void update_border_colors(struct buzzay_server *server);
