#include <stdlib.h>
#include <wayland-client.h>
#include <wayland-util.h>
#include "workspace.h"

struct buzzay_workspace *get_workspace_at_index(struct wl_list *list, uint32_t index) {
    struct buzzay_workspace *workspace;
    uint32_t current_index = 0;

    wl_list_for_each(workspace, list, link) {
        if (current_index == index) {
            return workspace;
        }
        current_index++;
    }

    return NULL;
}

void workspace_insert_toplevel(struct wl_list *workspaces, uint32_t current_workspace, struct buzzay_toplevel *toplevel) {
    struct buzzay_workspace *workspace = get_workspace_at_index(workspaces, current_workspace);
    wl_list_insert(&workspace->toplevels, &toplevel->link);

    struct layout_node *root = &workspace->layout;

    if (root->split_type == SPLIT_NONE && root->toplevel == NULL) {
        // First window takes over root
        root->toplevel = toplevel;
    } else {
        // Create a leaf node to store the window
        struct layout_node *new_leaf = calloc(1, sizeof(struct layout_node));
        new_leaf->split_type = SPLIT_NONE;
        new_leaf->toplevel = toplevel;
        new_leaf->split_ratio = 0.5f;

        // Turn the current root node into a split container
        struct layout_node *old_child = calloc(1, sizeof(struct layout_node));
        *old_child = *root;

        root->split_type = SPLIT_HORIZ;
        root->split_ratio = 0.5f;
        // Container does not hold any pointer to a toplevel
        root->toplevel = NULL; 
        
        root->first_child = old_child;
        root->second_child = new_leaf;
    }
}

static struct layout_node *remove_node_recursive(struct layout_node *node, struct buzzay_toplevel *toplevel, bool *removed) {
    if (!node) return NULL;

    if (node->split_type == SPLIT_NONE) {
        if (node->toplevel == toplevel) {
            *removed = true;
            free(node);
            return NULL;
        }
        return node; 
    }

    bool left_removed = false;
    bool right_removed = false;

    node->first_child = remove_node_recursive(node->first_child, toplevel, &left_removed);
    node->second_child = remove_node_recursive(node->second_child, toplevel, &right_removed);

    if (left_removed || right_removed) {
        *removed = true;
        
        struct layout_node *surviving_child = NULL;
        if (node->first_child != NULL) {
            surviving_child = node->first_child;
        } else {
            surviving_child = node->second_child;
        }

        if (surviving_child) {
            struct layout_node temp = *surviving_child;
            free(surviving_child);
            *node = temp;
        } else {
            node->split_type = SPLIT_NONE;
            node->toplevel = NULL;
        }
    }

    return node;
}

void workspace_remove_toplevel(struct buzzay_toplevel *toplevel) {
    struct buzzay_workspace *workspace = toplevel->in_workspace;
    if (!workspace) return;

    bool removed = false;
    struct layout_node *root = &workspace->layout;

    if (root->split_type == SPLIT_NONE && root->toplevel == toplevel) {
        root->toplevel = NULL;
        return;
    }

    remove_node_recursive(root, toplevel, &removed);
}

void workspace_init(struct buzzay_workspace *wp) {
    wp->layout.split_type = SPLIT_NONE;
    wp->layout.toplevel = NULL;
    wp->layout.split_ratio = 0.5f;
    wp->layout.first_child = NULL;
    wp->layout.second_child = NULL;
}
