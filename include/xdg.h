#pragma once

#include <wlr/backend.h>
#include <wayland-client-core.h>

#include "compositor.h"

void focus_toplevel(struct buzzay_toplevel *toplevel);
void server_new_xdg_toplevel(struct wl_listener *listener, void *data);
void server_new_xdg_popup(struct wl_listener *listener, void *data);
