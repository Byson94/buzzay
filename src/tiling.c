#include <stdint.h>
#include <wayland-util.h>
#include <wlr/backend.h>
#include <wayland-client.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <scenefx/types/wlr_scene.h>

#include "macro-utils.h"
#include "server.h"
#include "workspace.h"
#include "tiling.h"
#include "output.h"
#include "xdg.h"

void server_output_layout_changed(struct wl_listener *listener, void *data) {
    UNUSED(data);

    struct buzzay_server *server = wl_container_of(listener, server, output_layout_change);
    arrange_workspaces(server);
}

static void apply_borders(struct buzzay_toplevel *toplevel, struct wlr_box box) {
    uint32_t border_thickness = toplevel->server->eyecandies.border_thickness;
    wlr_scene_rect_set_clipped_region(toplevel->border_rect, (struct clipped_region) {
        .corners = corner_radii_all(toplevel->server->eyecandies.corner_radius),
        .area = { border_thickness, border_thickness, box.width, box.height }
    });

    wlr_scene_rect_set_size(
        toplevel->border_rect, 
        box.width + (border_thickness * 2), 
        box.height + (border_thickness * 2)
    );

    wlr_scene_node_set_position(
        &toplevel->border_rect->node, 
        -border_thickness, 
        -border_thickness
    );
}

static void arrange_node_recursive(struct layout_node *node, struct wlr_box box, struct buzzay_eyecandies *eyecandies) {
    if (!node) return;

    node->box = box;
    uint32_t gap = eyecandies->gap;

    if (node->split_type == SPLIT_NONE) {
        if (node->toplevel && node->toplevel->xdg_toplevel) {
            wlr_scene_node_set_enabled(&node->toplevel->scene_tree->node, true);

            wlr_scene_blur_set_size(node->toplevel->blur, box.width, box.height);
            wlr_xdg_toplevel_set_size(node->toplevel->xdg_toplevel, box.width, box.height);
            wlr_scene_node_set_position(&node->toplevel->scene_tree->node, box.x, box.y);
            apply_borders(node->toplevel, box);
        }
        return;
    }

    if (!node->first_child || !node->second_child) return;

    struct wlr_box box1 = box;
    struct wlr_box box2 = box;

    float ratio = (node->split_ratio > 0.0f) ? node->split_ratio : 0.5f;

    if (node->split_type == SPLIT_HORIZ) {
        // Horizontal split (Left / Right)
        int total_width = box.width - gap;
        box1.width = (int)(total_width * ratio);
        box2.x = box.x + box1.width + gap;
        box2.width = box.width - box1.width - gap;
    } else if (node->split_type == SPLIT_VERT) {
        // Vertical split (Top / Bottom)
        int total_height = box.height - gap;
        box1.height = (int)(total_height * ratio);
        box2.y = box.y + box1.height + gap;
        box2.height = box.height - box1.height - gap;
    }

    arrange_node_recursive(node->first_child, box1, eyecandies);
    arrange_node_recursive(node->second_child, box2, eyecandies);
}

void arrange_workspaces_tiling(struct buzzay_server *server) {
    struct buzzay_output *output;
    wl_list_for_each(output, &server->outputs, link) {
        struct wlr_box output_box;
        wlr_output_layout_get_box(server->output_layout, output->wlr_output, &output_box);
        
        if (output->usable_area.width > 0) {
            output_box.width = output->usable_area.width;
        }
        if (output->usable_area.height > 0) {
            output_box.height = output->usable_area.height;
        }

        struct buzzay_workspace *wp = get_workspace_at_index(&server->workspaces, server->current_workspace);
        if (!wp) continue;

        int gap = server->eyecandies.gap;
        struct wlr_box padded_box = {
            .x = output->usable_area.x + gap,
            .y = output->usable_area.y + gap,
            .width = output_box.width - (gap * 2),
            .height = output_box.height - (gap * 2)
        };

        arrange_node_recursive(&wp->layout, padded_box, &server->eyecandies);
    }
}

void arrange_workspaces_monocle(struct buzzay_server *server) {
    struct buzzay_output *output;
    wl_list_for_each(output, &server->outputs, link) {
        struct wlr_box output_box;
        wlr_output_layout_get_box(server->output_layout, output->wlr_output, &output_box);

        if (output->usable_area.width > 0) {
            output_box.width = output->usable_area.width;
        }
        if (output->usable_area.height > 0) {
            output_box.height = output->usable_area.height;
        }
        
        struct buzzay_workspace *wp = get_workspace_at_index(&server->workspaces, server->current_workspace);
        if (!wp) continue;

        if (wp->focused_window != NULL) {
            if (wp->focused_window->xdg_toplevel != NULL && 
                wp->focused_window->xdg_toplevel->base->surface->mapped &&
                wp->focused_window->xdg_toplevel->base->initialized) {

                uint32_t gap = wp->focused_window->server->eyecandies.gap;
                struct wlr_box padded_box = {
                    .x = output->usable_area.x + gap,
                    .y = output->usable_area.y + gap,
                    .width = output_box.width  - (gap * 2),
                    .height = output_box.height - (gap * 2)
                };

                wlr_scene_node_set_enabled(&wp->focused_window->scene_tree->node, true);

                wlr_scene_blur_set_size(wp->focused_window->blur, padded_box.width, padded_box.height);
                wlr_xdg_toplevel_set_size(wp->focused_window->xdg_toplevel, padded_box.width, padded_box.height);
                wlr_scene_node_set_position(&wp->focused_window->scene_tree->node, padded_box.x, padded_box.y);

                apply_borders(wp->focused_window, padded_box);
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
            arrange_workspaces_tiling(server);
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

void update_border_colors(struct buzzay_server *server) {
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

            if (toplevel == wp->focused_window) {
                wlr_scene_rect_set_color(toplevel->border_rect, server->eyecandies.active_border);
            } else {
                wlr_scene_rect_set_color(toplevel->border_rect, server->eyecandies.inactive_border);
            }
        }
    }
}
