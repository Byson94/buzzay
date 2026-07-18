#pragma once

#include <wlr/backend.h>
#include <wayland-client-core.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>

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

    struct wlr_xdg_decoration_manager_v1 *xdg_decoration;
    enum wlr_xdg_toplevel_decoration_v1_mode decoration_mode;
    struct wl_listener new_toplevel_decoration;

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

    struct wlr_layer_shell_v1 *layer_shell;
    struct wl_listener new_layer_surface;
};

#define bz_server_extract(plugin) ({ \
    size_t expected_size = sizeof(struct buzzay_server); \
    if (expected_size != (plugin)->_inner_server_size) { \
        fprintf(stderr, "Plugin '%s' binary mismatch! (Header: %zu, Server: %zu). Aborting.\n", \
                (plugin)->plugin_name, expected_size, (plugin)->_inner_server_size); \
        return; \
    } \
    (struct buzzay_server *)((plugin)->_inner_server); \
})
