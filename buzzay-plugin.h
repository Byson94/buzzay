#pragma once

#ifndef BUZZAY_PLUGIN
#define BUZZAY_PLUGIN

#include <wayland-client-core.h>
#include <wlr/types/wlr_input_device.h>

// MUST increment once every release
// IF a change is made to the file
#define BUZZAY_API_VERSION 1

// Server is the source of all truth
struct buzzay_server {
    struct wl_display *wl_display;
    struct wl_event_loop *wl_event_loop;
    struct wlr_backend *backend;
    struct wlr_renderer *renderer;
    struct wlr_allocator *allocator;
    struct wlr_session *session;
    struct wlr_scene *scene;
	struct wlr_scene_output_layout *scene_layout;

	struct wlr_xdg_shell *xdg_shell;
	struct wl_listener new_xdg_toplevel;
	struct wl_listener new_xdg_popup;
	struct wl_list toplevels;

    struct wlr_cursor *cursor;
    struct wlr_xcursor_manager *cursor_mgr;
	struct wl_listener cursor_motion;
	struct wl_listener cursor_motion_absolute;
	struct wl_listener cursor_button;
	struct wl_listener cursor_axis;
	struct wl_listener cursor_frame;

    struct wlr_seat *seat;
    struct wl_list keyboards;
    struct wl_listener new_input;
	struct wl_listener request_cursor;
	struct wl_listener pointer_focus_change;
	struct wl_listener request_set_selection;

    struct wlr_output_layout *output_layout;
    struct wl_list outputs;
    struct wl_listener new_output;
};

#endif
