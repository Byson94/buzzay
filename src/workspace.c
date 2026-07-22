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

void workspace_insert_toplevel(struct wl_list *workspaces, uint32_t current_workspace, struct wl_list *link) {
    struct buzzay_workspace *workspace = get_workspace_at_index(workspaces, current_workspace);
    wl_list_insert(&workspace->toplevels, link);
}
