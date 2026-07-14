#pragma once

#include <wlr/backend.h>
#include <wayland-client-core.h>

static void output_frame(struct wl_listener *listener, void *data);
static void output_request_state(struct wl_listener *listener, void *data);
static void output_destroy(struct wl_listener *listener, void *data);
static void server_new_output(struct wl_listener *listener, void *data);
