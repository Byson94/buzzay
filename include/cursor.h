#pragma once

#include <wlr/backend.h>

enum buzzay_cursor_mode {
	BUZZAY_CURSOR_PASSTHROUGH,
	BUZZAY_CURSOR_MOVE,
	BUZZAY_CURSOR_RESIZE,
};

void server_cursor_motion(struct wl_listener *listener, void *data);
void server_cursor_motion_absolute(struct wl_listener *listener, void *data);
void server_cursor_button(struct wl_listener *listener, void *data);
void server_cursor_axis(struct wl_listener *listener, void *data);
void server_cursor_frame(struct wl_listener *listener, void *data);

void seat_request_cursor(struct wl_listener *listener, void *data);
void seat_pointer_focus_change(struct wl_listener *listener, void *data);
void seat_request_set_selection(struct wl_listener *listener, void *data);

// Cursor Shape protocol
void server_new_request_cursor_set_shape(struct wl_listener *listener, void *data);


