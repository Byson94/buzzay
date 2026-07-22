#include <stdlib.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_output_layout.h>

#include "macro-utils.h"
#include "server.h"
#include "output.h"
#include "layershell.h"

static void layershell_commit(struct wl_listener *listener, void *data) {
    UNUSED(data);

    struct buzzay_layer_surface *bz_layer_surface = wl_container_of(listener, bz_layer_surface, commit);
    struct wlr_layer_surface_v1 *layer_surface = bz_layer_surface->surface;

    if (layer_surface->current.actual_width == layer_surface->pending.desired_width &&
        layer_surface->current.actual_height == layer_surface->pending.desired_height &&
        layer_surface->surface->mapped) {
        return;
    }

    if (layer_surface->initial_commit) {
        struct wlr_output *mon_output = layer_surface->output;
        uint32_t screen_width = mon_output->width;
        uint32_t screen_height = mon_output->height;

        struct wlr_box full_area = { .width = screen_width, .height = screen_height };
        wlr_scene_layer_surface_v1_configure(bz_layer_surface->scene_layer, &full_area, &full_area);
    }
}

static void layershell_unmap(struct wl_listener *listener, void *data) {
    UNUSED(data);

    struct buzzay_layer_surface *bz_layer_surface = wl_container_of(listener, bz_layer_surface, unmap);
    wlr_scene_node_set_enabled(&bz_layer_surface->scene_layer->tree->node, 0);
}

static void layershell_destroy(struct wl_listener *listener, void *data) {
    UNUSED(data);

    struct buzzay_layer_surface *bz_layer_surface = wl_container_of(listener, bz_layer_surface, destroy);

    wl_list_remove(&bz_layer_surface->commit.link);
    wl_list_remove(&bz_layer_surface->unmap.link);
    wl_list_remove(&bz_layer_surface->destroy.link);

    free(bz_layer_surface);
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
    bz_layer_surface->server = server;

    bz_layer_surface->commit.notify = layershell_commit;
    wl_signal_add(&layer_surface->surface->events.commit, &bz_layer_surface->commit);

    bz_layer_surface->unmap.notify = layershell_unmap;
    wl_signal_add(&layer_surface->surface->events.unmap, &bz_layer_surface->unmap);

    bz_layer_surface->destroy.notify = layershell_destroy;
    wl_signal_add(&layer_surface->events.destroy, &bz_layer_surface->destroy);
}
