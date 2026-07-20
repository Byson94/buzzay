#pragma once

#include <stdint.h>
#include <wayland-client.h>
#include <wayland-util.h>

struct buzzay_workspace {
    uint32_t id;
    struct wl_list link;
    struct wl_list toplevels;
};

struct buzzay_workspace *get_workspace_at_index(struct wl_list *list, uint32_t index);
void workspace_insert_toplevel(struct wl_list *workspaces, uint32_t current_workspace, struct wl_list *link);
