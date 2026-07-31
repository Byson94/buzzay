#include <stdlib.h>
#include <string.h>
#include <wayland-client.h>
#include <wayland-util.h>

#include "workspace.h"
#include "xdg.h"

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

struct layout_node *find_target_node(struct layout_node *node, struct buzzay_toplevel *target_toplevel) {
    if (!node) return NULL;
    
    if (node->split_type == SPLIT_NONE) {
        if (node->toplevel == target_toplevel) return node;
        return NULL;
    }

    struct layout_node *found = find_target_node(node->first_child, target_toplevel);
    if (found) return found;
    return find_target_node(node->second_child, target_toplevel);
}

void workspace_insert_toplevel(struct wl_list *workspaces, uint32_t current_workspace, struct buzzay_toplevel *toplevel) {
    struct buzzay_workspace *workspace = get_workspace_at_index(workspaces, current_workspace);
    wl_list_insert(&workspace->toplevels, &toplevel->link);

    struct layout_node *root = &workspace->layout;

    if (root->split_type == SPLIT_NONE && root->toplevel == NULL) {
        // First window takes over root
        root->toplevel = toplevel;
    } else {
        // Find the target node and add the window to it
        struct layout_node *target = find_target_node(root, workspace->focused_window);
        if (!target) {
            target = root;
        }

        // Create a new leaf node for the incoming window
        struct layout_node *new_leaf = calloc(1, sizeof(struct layout_node));
        new_leaf->split_type = SPLIT_NONE;
        new_leaf->toplevel = toplevel;
        new_leaf->split_ratio = 0.5f;

        struct layout_node *old_child = calloc(1, sizeof(struct layout_node));
        *old_child = *target; 

        // Set the target split direction
        if (target->box.width >= target->box.height) {
            target->split_type = SPLIT_HORIZ;
        } else {
            target->split_type = SPLIT_VERT;
        }

        target->split_ratio = 0.5f;
        target->toplevel = NULL;
        
        target->first_child = old_child;
        target->second_child = new_leaf;
    }
}

static struct layout_node *remove_node_recursive(struct layout_node *node, struct buzzay_toplevel *toplevel) {
    if (!node) return NULL;

    if (node->split_type == SPLIT_NONE) {
        if (node->toplevel == toplevel) {
            if (!node->is_root) free(node);
            return NULL;
        }
        return node;
    }

    node->first_child = remove_node_recursive(node->first_child, toplevel);
    node->second_child = remove_node_recursive(node->second_child, toplevel);

    if (node->first_child == NULL) {
        struct layout_node *surviving_child = node->second_child;
        if (!node->is_root) free(node);
        return surviving_child;
    }
    
    if (node->second_child == NULL) {
        struct layout_node *surviving_child = node->first_child;
        if (!node->is_root) free(node);
        return surviving_child;
    }

    return node;
}

void workspace_remove_toplevel(struct buzzay_toplevel *toplevel) {
    struct buzzay_workspace *workspace = toplevel->in_workspace;
    if (!workspace) return;

    struct layout_node *root = &workspace->layout;
    if (root->split_type == SPLIT_NONE && root->toplevel == toplevel) {
        root->toplevel = NULL;
        return;
    }

    struct layout_node *new_root = remove_node_recursive(root, toplevel);
    if (new_root != root && new_root != NULL) {
        *root = *new_root;
        free(new_root);
    } else if (new_root == NULL) {
        root->split_type = SPLIT_NONE;
        root->toplevel = NULL;
        root->first_child = NULL;
        root->second_child = NULL;
    }
}

void workspace_init(struct buzzay_workspace *wp) {
    wp->layout.split_type = SPLIT_NONE;
    wp->layout.toplevel = NULL;
    wp->layout.split_ratio = 0.5f;
    wp->layout.first_child = NULL;
    wp->layout.second_child = NULL;
    wp->layout.is_root = true;
}

struct layout_node *find_node_in_direction(struct layout_node *root, struct wlr_box current_box, const char *direction) {
    if (!root) return NULL;

    if (root->split_type == SPLIT_NONE) {
        if (root->box.x == current_box.x && root->box.y == current_box.y) {
            return NULL;
        }

        if (strcmp(direction, "left") == 0) {
            if (root->box.x + root->box.width <= current_box.x) return root;
        } else if (strcmp(direction, "right") == 0) {
            if (root->box.x >= current_box.x + current_box.width) return root;
        } else if (strcmp(direction, "up") == 0) {
            if (root->box.y + root->box.height <= current_box.y) return root;
        } else if (strcmp(direction, "down") == 0) {
            if (root->box.y >= current_box.y + current_box.height) return root;
        }
        return NULL;
    }

    struct layout_node *first = find_node_in_direction(root->first_child, current_box, direction);
    struct layout_node *second = find_node_in_direction(root->second_child, current_box, direction);

    if (first && second) {
        int c_center_x = current_box.x + current_box.width / 2;
        int c_center_y = current_box.y + current_box.height / 2;
        
        int dist1 = abs((first->box.x + first->box.width/2) - c_center_x) + abs((first->box.y + first->box.height/2) - c_center_y);
        int dist2 = abs((second->box.x + second->box.width/2) - c_center_x) + abs((second->box.y + second->box.height/2) - c_center_y);
        
        return (dist1 < dist2) ? first : second;
    }

    return first ? first : second;
}

struct layout_node *find_node_for_toplevel(struct layout_node *node, struct buzzay_toplevel *target_toplevel) {
    if (!node) {
        return NULL;
    }

    if (node->split_type == SPLIT_NONE) {
        if (node->toplevel == target_toplevel) {
            return node;
        }
        return NULL;
    }

    struct layout_node *found = find_node_for_toplevel(node->first_child, target_toplevel);
    if (found) {
        return found;
    }

    return find_node_for_toplevel(node->second_child, target_toplevel);
}
