#pragma once

#include <wlr/backend.h>
#include <wayland-client-core.h>

struct buzzay_layer_surface {
    struct wlr_layer_surface_v1 *surface;
    struct wlr_scene_layer_surface_v1 *scene_layer;
    struct buzzay_server *server;
    struct wl_listener unmap;
    struct wl_listener commit;
    struct wl_listener destroy;
};

void server_new_layer_surface(struct wl_listener *listener, void *data);
