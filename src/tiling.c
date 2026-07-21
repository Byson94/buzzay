#include <wayland-util.h>
#include <wlr/backend.h>
#include <wayland-client.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>

#include "server.h"
#include "workspace.h"
#include "tiling.h"
#include "output.h"
#include "xdg.h"

void server_output_layout_changed(struct wl_listener *listener, void *data) {
    struct buzzay_server *server = wl_container_of(listener, server, output_layout_change);
    arrange_workspaces(server);
}

void arrange_workspaces_monocle(struct buzzay_server *server) {
    struct buzzay_output *output;
    wl_list_for_each(output, &server->outputs, link) {
        struct wlr_box output_box;
        wlr_output_layout_get_box(server->output_layout, output->wlr_output, &output_box);
        
        struct buzzay_workspace *wp = get_workspace_at_index(&server->workspaces, server->current_workspace);
        if (!wp) continue;

        if (wp->focused_window != NULL) {
            if (wp->focused_window->xdg_toplevel != NULL && 
                wp->focused_window->xdg_toplevel->base->surface->mapped &&
                wp->focused_window->xdg_toplevel->base->initialized) {
                wlr_scene_node_set_enabled(&wp->focused_window->scene_tree->node, true);
                wlr_xdg_toplevel_set_size(wp->focused_window->xdg_toplevel, output_box.width, output_box.height);
                wlr_scene_node_set_position(&wp->focused_window->scene_tree->node, output_box.x, output_box.y);
            }
        }

        struct buzzay_toplevel *toplevel;
        wl_list_for_each(toplevel, &wp->toplevels, link) {
            if (toplevel->xdg_toplevel == NULL || 
                !toplevel->xdg_toplevel->base->surface->mapped ||
                !toplevel->xdg_toplevel->base->initialized) {
                continue; 
            }

            if (toplevel == wp->focused_window) {
                continue;
            }

            wlr_scene_node_set_enabled(&toplevel->scene_tree->node, false);
        }
    }
}

void arrange_workspaces(struct buzzay_server *server) {
    switch (server->window_layout_mode) {
        case BZ_LAYOUT_MONOCLE:
            arrange_workspaces_monocle(server);
            break;
        case BZ_LAYOUT_TILE:
            // arrange_workspaces_tiling(server);
            break;
    };
}

// Behavior

void focus_next_monocle(struct buzzay_server *server) {
    if (server->window_layout_mode != BZ_LAYOUT_MONOCLE) {
        return;
    }

    struct buzzay_output *output;
    wl_list_for_each(output, &server->outputs, link) {
        struct wlr_box output_box;
        wlr_output_layout_get_box(server->output_layout, output->wlr_output, &output_box);
        
        struct buzzay_workspace *wp = get_workspace_at_index(&server->workspaces, server->current_workspace);
        if (!wp) continue;

        struct buzzay_toplevel *current = wp->focused_window;

        // If nothing is active (or list changed), default to the very first window
        if (!current) {
            current = wl_container_of(wp->toplevels.next, current, link);
        }

        struct buzzay_toplevel *next = NULL;
        if (current->link.next == &wp->toplevels) {
            // At the end, wrap back to start
            next = wl_container_of(wp->toplevels.next, next, link);
        } else {
            next = wl_container_of(current->link.next, next, link);
        }

        focus_toplevel(next);
        arrange_workspaces_monocle(server);
    }
}
