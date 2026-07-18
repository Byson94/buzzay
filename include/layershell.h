#pragma once

#include <wlr/backend.h>
#include <wayland-client-core.h>

struct buzzay_layer_surface {
    struct buzzay_server *server;
    struct wl_listener new_popup;
    struct wl_listener destroy;
};

void server_new_layer_surface(struct wl_listener *listener, void *data);
