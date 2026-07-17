#pragma once

#ifndef BUZZAY_PLUGIN
#define BUZZAY_PLUGIN

#include <wayland-client-core.h>
#include <wlr/types/wlr_input_device.h>

// MUST increment once every release
// IF a change is made to the file
#define BUZZAY_API_VERSION 1
#define BZ_API __attribute__((visibility("default")))

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

// The plugin wrapper
struct bz_plugin {
    const char *plugin_name;
    const char *plugin_path;
    struct buzzay_server *server;
};

// Protocol API's
BZ_API struct wl_global *bz_register_plugin_protocol(
    struct bz_plugin *plugin,
    const struct wl_interface *interface,
    void *data,
    wl_global_bind_func_t bind);

// Keybinding API's
typedef enum {
    BZ_MOD_SHIFT = (1 << 0),
    BZ_MOD_ALT = (1 << 1),
    BZ_MOD_CTRL = (1 << 2),
    BZ_MOD_SUPER = (1 << 3),
} bz_modifier_t;

struct bz_keybinding {
    uint32_t keysym;
    bz_modifier_t modifiers;
    void (*handler)(struct bz_plugin *plugin, void *data);
    void *data;
};

typedef int bz_binding_handle_t;

BZ_API bz_binding_handle_t bz_register_keybinding(
    struct bz_plugin *plugin,
    struct bz_keybinding binding
);

BZ_API void bz_unregister_keybinding(bz_binding_handle_t *handle);

#endif
