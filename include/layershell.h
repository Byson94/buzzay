#pragma once

#include <wlr/backend.h>
#include <wayland-client-core.h>
#include <wlr/types/wlr_layer_shell_v1.h>

struct buzzay_layer_surface {
    struct wlr_layer_surface_v1 *surface;
    struct wlr_scene_layer_surface_v1 *scene_layer;
    struct buzzay_server *server;
    struct wl_listener unmap;
    struct wl_listener commit;
    struct wl_listener destroy;
    struct wl_listener new_popup;
    struct wl_listener destroy_popup;

    enum zwlr_layer_shell_v1_layer current_layer;
};

void server_new_layer_surface(struct wl_listener *listener, void *data);
