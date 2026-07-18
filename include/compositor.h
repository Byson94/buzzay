#pragma once

#include "wlr/types/wlr_xdg_decoration_v1.h"
#include <wayland-client-core.h>
#include <wlr/types/wlr_input_device.h>

struct buzzay_output {
    struct wl_list link;
    struct buzzay_server *server;
    struct wlr_output *wlr_output;
    struct wl_listener frame;
    struct wl_listener request_state;
    struct wl_listener destroy;
};

struct buzzay_keyboard {
	struct wl_list link;
	struct buzzay_server *server;
	struct wlr_keyboard *wlr_keyboard;

	struct wl_listener modifiers;
	struct wl_listener key;
	struct wl_listener destroy;
};

struct buzzay_toplevel {
	struct wl_list link;
	struct buzzay_server *server;
	struct wlr_xdg_toplevel *xdg_toplevel;
	struct wlr_scene_tree *scene_tree;
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener commit;
	struct wl_listener destroy;
	struct wl_listener request_move;
	struct wl_listener request_resize;
	struct wl_listener request_maximize;
	struct wl_listener request_fullscreen;
};

struct buzzay_popup {
	struct wlr_xdg_popup *xdg_popup;
	struct wl_listener commit;
	struct wl_listener destroy;
};

struct buzzay_decoration {
    struct buzzay_server *server;
    struct wlr_xdg_toplevel_decoration_v1 *decoration;
    struct wl_listener map;
    struct wl_listener destroy;
};

