#pragma once

#include <wlr/backend.h>
#include <wayland-client-core.h>

struct buzzay_output {
    struct wl_list link;
    struct buzzay_server *server;
    struct wlr_output *wlr_output;
    struct wl_listener frame;
    struct wl_listener request_state;
    struct wl_listener destroy;
};

void server_new_output(struct wl_listener *listener, void *data);
