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

        int total_windows = 0;
        struct buzzay_toplevel *toplevel;
        wl_list_for_each(toplevel, &wp->toplevels, link) {
            if (toplevel->xdg_toplevel == NULL || 
                !toplevel->xdg_toplevel->base->surface->mapped ||
                !toplevel->xdg_toplevel->base->initialized) {
                continue; 
            }
            total_windows++;
        }

        if (total_windows == 0) continue;

        int gap = server->eyecandies.gap;
        struct wlr_box padded_box = {
            .x = output->usable_area.x + gap,
            .y = output->usable_area.y + gap,
            .width = output_box.width - (gap * 2),
            .height = output_box.height - (gap * 2)
        };

        // apply geometry in second loop
        int i = 0;
        wl_list_for_each(toplevel, &wp->toplevels, link) {
            if (toplevel->xdg_toplevel == NULL || 
                !toplevel->xdg_toplevel->base->surface->mapped ||
                !toplevel->xdg_toplevel->base->initialized) {
                continue; 
            }

            // Ensure the window is enabled in the scene graph
            wlr_scene_node_set_enabled(&toplevel->scene_tree->node, true);

            struct wlr_box box;

            if (total_windows == 1) {
                // one window = whole screen
                box = padded_box;
            } else if (i == 0) {
                // master window takes left half of screen
                int total_available_width = padded_box.width - gap;
                box.x = padded_box.x;
                box.y = padded_box.y;
                box.width = total_available_width / 2;
                box.height = padded_box.height;
            } else {
                // slave stack takes right half and is split vertically
                int total_available_width = padded_box.width - gap;
                int master_width = total_available_width / 2;
                int stack_width = total_available_width - master_width;
                
                int stack_count = total_windows - 1;
                int stack_index = i - 1;

                // Total vertical space available for the stack, accounting for gaps *between* stack items
                int total_stack_height = padded_box.height - (gap * (stack_count - 1));
                int item_height = total_stack_height / stack_count;

                box.x = padded_box.x + master_width + gap;
                box.width = stack_width;
                box.height = item_height;
                box.y = padded_box.y + (item_height + gap) * stack_index;
            }

            wlr_scene_blur_set_size(toplevel->blur, box.width, box.height);
            wlr_xdg_toplevel_set_size(toplevel->xdg_toplevel, box.width, box.height);
            wlr_scene_node_set_position(&toplevel->scene_tree->node, box.x, box.y);

            apply_borders(toplevel, box);

            i++;
        }
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
