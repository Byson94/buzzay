#pragma once

#include <wayland-server-core.h>
#include <wlr/backend.h>

struct buzzay_gamma {
    struct wlr_output *output;
    struct wl_listener destroy;
};

void server_new_set_gamma(struct wl_listener *listener, void *data);

