#include <wayland-util.h>
#include <wlr/backend.h>
#include <wayland-client.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>

#include "workspace.h"
#include "tiling.h"
#include "output.h"

void server_output_layout_changed(struct wl_listener *listener, void *data) {
    struct buzzay_server *server = wl_container_of(listener, server, output_layout_change);
    arrange_workspaces(server);
}

void arrange_workspaces(struct buzzay_server *server) {
    struct buzzay_output *output;
    wl_list_for_each(output, &server->outputs, link) {
        struct wlr_box output_box;
        wlr_output_layout_get_box(server->output_layout, output->wlr_output, &output_box);
        
        struct buzzay_workspace *wp = get_workspace_at_index(&server->workspaces, server->current_workspace);
        if (!wp) continue;

        struct buzzay_toplevel *toplevel;
        wl_list_for_each(toplevel, &wp->toplevels, link) {
            if (toplevel->xdg_toplevel == NULL || 
                !toplevel->xdg_toplevel->base->surface->mapped ||
                !toplevel->xdg_toplevel->base->initialized) {
                continue; 
            }

            wlr_xdg_toplevel_set_size(toplevel->xdg_toplevel, output_box.width, output_box.height);
            wlr_scene_node_set_position(&toplevel->scene_tree->node, output_box.x, output_box.y);
        }
    }
}
