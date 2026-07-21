#pragma once

#include <wlr/backend.h>
#include <wayland-client-core.h>

#include "workspace.h"

struct buzzay_toplevel {
	struct wl_list link;
	struct buzzay_server *server;
	struct wlr_xdg_toplevel *xdg_toplevel;
	struct wlr_scene_tree *scene_tree;
    struct wlr_scene_rect *border_rect;
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener commit;
	struct wl_listener destroy;
	struct wl_listener request_move;
	struct wl_listener request_resize;
	struct wl_listener request_maximize;
	struct wl_listener request_fullscreen;

    bool is_floating;
    struct buzzay_workspace *in_workspace;
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

enum window_active_evt {
    WINDOW_ACTIVE_ON_HOVER,
    WINDOW_ACTIVE_ON_CLICK,
};

void reset_cursor_mode(struct buzzay_server *server);

void focus_toplevel(struct buzzay_toplevel *toplevel);
void server_new_xdg_toplevel(struct wl_listener *listener, void *data);
void server_new_xdg_popup(struct wl_listener *listener, void *data);
void server_new_toplevel_decoration(struct wl_listener *listener, void *data);
