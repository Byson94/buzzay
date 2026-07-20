#pragma once

#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wayland-client-core.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>

#include "cursor.h"
#include "xdg.h"

struct scene_layers {
    struct wlr_scene_tree *background;
    struct wlr_scene_tree *bottom;
    struct wlr_scene_tree *workspace;
    struct wlr_scene_tree *top;
    struct wlr_scene_tree *overlay;
};

struct buzzay_server {
    struct wl_display *wl_display;
    struct wl_event_loop *wl_event_loop;
    struct wlr_backend *backend;
    struct wlr_renderer *renderer;
    struct wlr_allocator *allocator;
    struct wlr_session *session;
    struct wlr_scene *scene;
    struct scene_layers layers;
	struct wlr_scene_output_layout *scene_layout;
    struct wl_listener session_active;

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

    struct wlr_cursor_shape_manager_v1 *cursor_shape_mgr;
    struct wl_listener cursor_request_set_shape;

    struct wlr_seat *seat;
    struct wl_list keyboards;
    struct wl_listener new_input;
	struct wl_listener request_cursor;
	struct wl_listener pointer_focus_change;
	struct wl_listener request_set_selection;
	enum buzzay_cursor_mode cursor_mode;
	struct buzzay_toplevel *grabbed_toplevel;
	double grab_x, grab_y;
	struct wlr_box grab_geobox;
	uint32_t resize_edges;
    uint32_t last_serial;
    bool cursor_recently_reset;

    struct wlr_output_layout *output_layout;
    struct wl_list outputs;
    struct wl_listener new_output;

    struct wlr_layer_shell_v1 *layer_shell;
    struct wl_listener new_layer_surface;

    struct wlr_gamma_control_manager_v1 *gamma_mgr;
    struct wl_listener set_gamma;

    struct wlr_idle_inhibit_manager_v1 *idle_inhibit_mgr;
    struct wl_listener idle_new_inhibitor;
    struct wlr_idle_notifier_v1 *idle_notifier;
    int idle_inhibit_count;

    // Config
    bool enable_xdg_interactive; // actions like move & resize
    enum window_active_evt window_active_on;

    // Config Exclusive (only used when specific configs are true)
    struct buzzay_toplevel *hovered_toplevel; // window_active_on == WINDOW_ACTIVE_ON_HOVER
};

