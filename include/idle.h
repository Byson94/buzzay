#pragma once

#include <wlr/backend.h>
#include "server.h"

struct buzzay_inhibitor {
    struct buzzay_server *server;
    struct wlr_idle_inhibitor_v1 *inhibitor;
    struct wl_listener destroy;
};

void server_new_idle_new_inhibitor(struct wl_listener *listener, void *data);
