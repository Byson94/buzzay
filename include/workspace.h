#pragma once

#include <stdint.h>
#include <wayland-client.h>
#include <wayland-util.h>

#include "xdg.h"

enum node_split_type {
    SPLIT_NONE, // This contains actual window
    SPLIT_HORIZ,
    SPLIT_VERT
};

struct layout_node {
    enum node_split_type split_type;
    
    // SPLIT_NONE points to window:
    struct buzzay_toplevel *toplevel; 
    
    // If not SPLIT_NONE, it has two children
    struct layout_node *first_child;
    struct layout_node *second_child;
    
    struct wlr_box box; 
    float split_ratio;
};

struct buzzay_workspace {
    uint32_t id;
    struct wl_list link;
    struct wl_list toplevels;
    struct buzzay_toplevel *focused_window;
    struct layout_node layout;
};

struct buzzay_workspace *get_workspace_at_index(struct wl_list *list, uint32_t index);
void workspace_insert_toplevel(struct wl_list *workspaces, uint32_t current_workspace, struct buzzay_toplevel *toplevel);
void workspace_remove_toplevel(struct buzzay_toplevel *toplevel);
void workspace_init(struct buzzay_workspace *wp);
