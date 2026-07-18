#pragma once

#include <wlr/backend.h>
#include <wayland-client-core.h>

struct buzzay_keyboard {
	struct wl_list link;
	struct buzzay_server *server;
	struct wlr_keyboard *wlr_keyboard;

	struct wl_listener modifiers;
	struct wl_listener key;
	struct wl_listener destroy;
};

void server_new_input(struct wl_listener *listener, void *data);
