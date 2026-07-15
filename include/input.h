#pragma once

#include <wlr/backend.h>
#include <wayland-client-core.h>

void server_new_input(struct wl_listener *listener, void *data);
