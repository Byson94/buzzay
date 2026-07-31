#pragma once

#include <stdint.h>
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wayland-client-core.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>

enum buzzay_cursor_mode : unsigned int;
enum window_active_evt : unsigned int;

struct scene_layers {
    struct wlr_scene_tree *background;
    struct wlr_scene_tree *bottom;
    struct wlr_scene_optimized_blur *blur;
    struct wlr_scene_tree *workspace;
    struct wlr_scene_tree *top;
    struct wlr_scene_tree *overlay;

    // Overlay used to draw compositor popups
    struct wlr_scene_tree *native_overlay;
};

struct buzzay_eyecandies {
    uint32_t gap;
    float active_border[4];
    float inactive_border[4];
    uint32_t border_thickness;
    uint32_t corner_radius;
    float window_opacity;

    float blur_strength;
    float blur_alpha;
    int blur_passes;
    float blur_noise;
};

enum buzzay_layout_mode {
    BZ_LAYOUT_MONOCLE,
    BZ_LAYOUT_TILE
};

struct active_keyrepeat {
    struct keybinding *kb;
    uint32_t keycode;
    uint32_t modifiers;
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
    struct wl_list workspaces;
    uint32_t current_workspace;

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
    const char *current_cursor_shape;
	uint32_t resize_edges;
    uint32_t last_serial;
    bool cursor_recently_reset;
    struct buzzay_layer_surface *focused_layersehll;

    struct wlr_output_layout *output_layout;
    struct wl_listener output_layout_change;
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

    struct wl_event_source *kb_repeat_timer;
    struct active_keyrepeat current_repeat;
    int32_t repeat_delay;
    int32_t repeat_rate;

    // Config
    const char *config_file;
    bool server_first_load;
    bool enable_xdg_interactive; // actions like move & resize
    enum window_active_evt window_active_on;
    enum buzzay_layout_mode window_layout_mode;
    struct buzzay_eyecandies eyecandies;
    const char *xcursor_theme;
    uint32_t xcursor_size;

    // Config Exclusive (only used when specific configs are true)
    struct buzzay_toplevel *hovered_toplevel; // window_active_on == WINDOW_ACTIVE_ON_HOVER
};


