#pragma once

#include <wayland-server-core.h>
#include <wlr/backend.h>

// forward declaration
struct buzzay_server;

enum buzzay_cursor_mode : unsigned int {
	BUZZAY_CURSOR_PASSTHROUGH,
	BUZZAY_CURSOR_MOVE,
	BUZZAY_CURSOR_RESIZE,
};

enum bz_underlying_surface_type {
    BUZZAY_SURFACE_TOPLEVEL,
    BUZZAY_SURFACE_LAYERSHELL
};

struct bz_underlying_surface {
    enum bz_underlying_surface_type type;
    void *item;
};

struct drag_icon_state {
    struct buzzay_server *server;
    struct wlr_scene_tree *scene_tree;
    struct wlr_drag_icon *icon;
    struct wl_listener destroy;
};

void process_cursor_motion(struct buzzay_server *server, uint32_t *time);
void server_cursor_motion(struct wl_listener *listener, void *data);
void server_cursor_motion_absolute(struct wl_listener *listener, void *data);
void server_cursor_button(struct wl_listener *listener, void *data);
void server_cursor_axis(struct wl_listener *listener, void *data);
void server_cursor_frame(struct wl_listener *listener, void *data);
void server_handle_request_start_drag(struct wl_listener *listener, void *data);
void server_handle_start_drag(struct wl_listener *listener, void *data);

void seat_request_cursor(struct wl_listener *listener, void *data);
void seat_pointer_focus_change(struct wl_listener *listener, void *data);
void seat_request_set_selection(struct wl_listener *listener, void *data);
void seat_set_primary_selection(struct wl_listener *listener, void *data);

// Cursor Shape protocol
void set_cursor_shape_forced(struct buzzay_server *server, const char *shape);
void set_cursor_shape(struct buzzay_server *server, const char *shape);
void server_new_request_cursor_set_shape(struct wl_listener *listener, void *data);


