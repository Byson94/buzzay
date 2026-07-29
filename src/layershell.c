#include <stdlib.h>
#include <wayland-server-core.h>
#include <wayland-util.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <scenefx/types/wlr_scene.h>
#include <wlr/types/wlr_output_layout.h>

#include "cursor.h"
#include "macro-utils.h"
#include "server.h"
#include "output.h"
#include "layershell.h"
#include "tiling.h"

void focus_layershell(struct buzzay_layer_surface *layershell) {
	/* Note: this function only deals with keyboard focus. */
	if (layershell == NULL) {
		return;
	}
	struct buzzay_server *server = layershell->server;
	struct wlr_seat *seat = server->seat;
	struct wlr_surface *prev_surface = seat->keyboard_state.focused_surface;
	struct wlr_surface *surface = layershell->surface->surface;
	if (prev_surface == surface) {
		/* Don't re-focus an already focused surface. */
		return;
	}
	struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat);
	/* Move the layershell to the front */
	wlr_scene_node_raise_to_top(&layershell->scene_layer->tree->node);
	/*
	 * Tell the seat to have the keyboard enter this surface. wlroots will keep
	 * track of this and automatically send key events to the appropriate
	 * clients without additional work on your part.
	 */
	if (keyboard != NULL) {
		wlr_seat_keyboard_notify_enter(seat, surface,
			keyboard->keycodes, keyboard->num_keycodes, &keyboard->modifiers);
	}

    update_border_colors(server);
}

void layershell_do_allocations(struct wlr_layer_surface_v1 *layer_surface) {
    if (layer_surface == NULL) return;

    struct wlr_output *mon_output = layer_surface->output;
    struct buzzay_output *output = mon_output->data;
    uint32_t screen_width = mon_output->width;
    uint32_t screen_height = mon_output->height;

    // guard in case we are switching vt
    if (output == NULL) return;

    struct wlr_box full_area = {
        .x = 0,
        .y = 0,
        .width = screen_width, 
        .height = screen_height 
    };

    output->usable_area = full_area;

    struct buzzay_layer_surface *bz_surf;

    // Let the exclusive windows claim the space first.
    // Do it in reverse because idk, it seems to be doing it nicely.
    wl_list_for_each_reverse(bz_surf, &output->layer_surfaces, link) {
        struct wlr_layer_surface_v1 *l_surf = bz_surf->surface;
        
        if (l_surf->current.exclusive_zone <= 0) {
            continue;
        }

        wlr_scene_layer_surface_v1_configure(
            bz_surf->scene_layer,
            &full_area,
            &output->usable_area
        );
    }

    // Now that exclusive windows have the exclusive zone all set up,
    // we can configure non-exclusive windows that will follow the
    // available space.
    wl_list_for_each(bz_surf, &output->layer_surfaces, link) {
        struct wlr_layer_surface_v1 *l_surf = bz_surf->surface;
        
        if (l_surf->current.exclusive_zone > 0) {
            continue;
        }

        wlr_scene_layer_surface_v1_configure(
            bz_surf->scene_layer,
            &full_area,
            &output->usable_area
        );
    }
}

static void layershell_commit(struct wl_listener *listener, void *data) {
    UNUSED(data);

    struct buzzay_layer_surface *bz_layer_surface = wl_container_of(listener, bz_layer_surface, commit);
    struct wlr_layer_surface_v1 *layer_surface = bz_layer_surface->surface;
    struct buzzay_server *server = bz_layer_surface->server;

    if (bz_layer_surface->current_layer != layer_surface->pending.layer) {
        struct wlr_scene_tree *target_tree = NULL;
        switch (layer_surface->pending.layer) {
            case ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND:
                target_tree = server->layers.background;
                break;
            case ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM:
                target_tree = server->layers.bottom;
                break;
            case ZWLR_LAYER_SHELL_V1_LAYER_TOP:
                target_tree = server->layers.top;
                break;
            case ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY:
                target_tree = server->layers.overlay;
                break;
        }

        if (target_tree) {
            wlr_scene_node_reparent(&bz_layer_surface->scene_layer->tree->node, target_tree);
            bz_layer_surface->current_layer = layer_surface->pending.layer;
        }
    }

    layershell_do_allocations(layer_surface);
    arrange_workspaces(bz_layer_surface->server);
}

static void layershell_unmap(struct wl_listener *listener, void *data) {
    UNUSED(data);

    struct buzzay_layer_surface *bz_layer_surface = wl_container_of(listener, bz_layer_surface, unmap);
    wlr_scene_node_set_enabled(&bz_layer_surface->scene_layer->tree->node, 0);
}

static void layershell_destroy(struct wl_listener *listener, void *data) {
    UNUSED(data);

    struct buzzay_layer_surface *bz_layer_surface = wl_container_of(listener, bz_layer_surface, destroy);

    if (bz_layer_surface->underlying_surface) {
        free(bz_layer_surface->underlying_surface);
        bz_layer_surface->underlying_surface = NULL;
    }

    wl_list_remove(&bz_layer_surface->link);
    wl_list_remove(&bz_layer_surface->commit.link);
    wl_list_remove(&bz_layer_surface->unmap.link);
    wl_list_remove(&bz_layer_surface->destroy.link);
    wl_list_remove(&bz_layer_surface->new_popup.link);

    wlr_seat_pointer_clear_focus(bz_layer_surface->server->seat);
    wlr_seat_keyboard_notify_clear_focus(bz_layer_surface->server->seat);

    layershell_do_allocations(bz_layer_surface->surface);
    arrange_workspaces(bz_layer_surface->server);

    free(bz_layer_surface);
}

static void layershell_new_popup(struct wl_listener *listener, void *data) {
    struct buzzay_layer_surface *bz_layer_surface = wl_container_of(listener, bz_layer_surface, new_popup);
    struct wlr_xdg_popup *popup = data;
    struct wlr_scene_tree *parent_tree = bz_layer_surface->scene_layer->tree;
    wlr_scene_xdg_surface_create(parent_tree, popup->base);
}

void server_new_layer_surface(struct wl_listener *listener, void *data) {
    struct buzzay_server *server = wl_container_of(listener, server, new_layer_surface);
    struct wlr_layer_surface_v1 *layer_surface = data;

	if (!layer_surface->output) {
        if (wl_list_empty(&server->outputs)) {
            wlr_layer_surface_v1_destroy(layer_surface);
            return;
        }
        struct buzzay_output *first_output = wl_container_of(server->outputs.next, first_output, link);
        layer_surface->output = first_output->wlr_output;
    }

    struct wlr_scene_tree *target_tree;
    switch (layer_surface->pending.layer) {
        case ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND: target_tree = server->layers.background; break;
        case ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM: target_tree = server->layers.bottom; break;
        case ZWLR_LAYER_SHELL_V1_LAYER_TOP: target_tree = server->layers.top; break;
        case ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY: target_tree = server->layers.overlay; break;
    }

    struct buzzay_layer_surface *bz_layer_surface = calloc(1, sizeof(*bz_layer_surface));
    bz_layer_surface->surface = layer_surface;
    bz_layer_surface->scene_layer = wlr_scene_layer_surface_v1_create(target_tree, layer_surface);

    struct bz_underlying_surface *surface_under = calloc(1, sizeof(*surface_under));
    surface_under->type = BUZZAY_SURFACE_LAYERSHELL;
    surface_under->item = bz_layer_surface;

    bz_layer_surface->underlying_surface = surface_under;
    bz_layer_surface->scene_layer->tree->node.data = surface_under;
    bz_layer_surface->current_layer = layer_surface->pending.layer;
    bz_layer_surface->server = server;

    struct buzzay_output *output = layer_surface->output->data;
    wl_list_insert(&output->layer_surfaces, &bz_layer_surface->link);

    bz_layer_surface->commit.notify = layershell_commit;
    wl_signal_add(&layer_surface->surface->events.commit, &bz_layer_surface->commit);

    bz_layer_surface->unmap.notify = layershell_unmap;
    wl_signal_add(&layer_surface->surface->events.unmap, &bz_layer_surface->unmap);

    bz_layer_surface->destroy.notify = layershell_destroy;
    wl_signal_add(&layer_surface->events.destroy, &bz_layer_surface->destroy);

    bz_layer_surface->new_popup.notify = layershell_new_popup;
    wl_signal_add(&layer_surface->events.new_popup, &bz_layer_surface->new_popup);

    focus_layershell(bz_layer_surface);
}
