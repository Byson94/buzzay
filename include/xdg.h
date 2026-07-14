#pragma once

#include <wlr/backend.h>
#include <wayland-client-core.h>

static void server_new_xdg_toplevel(struct wl_listener *listener, void *data);
static void server_new_xdg_popup(struct wl_listener *listener, void *data);
