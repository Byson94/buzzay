#pragma once

#ifndef BUZZAY_PLUGIN
#define BUZZAY_PLUGIN

#include <wayland-client-core.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_xdg_shell.h>

// MUST increment once every release
// IF a change is made to the file
#define BUZZAY_API_VERSION 1
#define BZ_API __attribute__((visibility("default")))

// Server is the source of all truth
/**
 * The compositor server that contains IMPORTANT
 * data like the `wl_display` that are crucial for 
 * the functioning of the compositor.
 */
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

/**
 *  The plugin wrapper structure that contains important
 *  metadata like plugin name, path, and the buzzay server.
 */
struct bz_plugin {
    const char *plugin_name; /**< The name of the plugin. */
    const char *plugin_path; /**< The version number of the plugin. */

    /** 
     * The buzzay server that is required for all the API function calls.
     * Only access and modify this if you know what you are doing and there are no
     * API's to do what you want.
     */
    struct buzzay_server *server;
};

// General

/**
 * Quit buzzay.
 */
BZ_API void bz_quit(struct bz_plugin *plugin);

// Keybinding API's
#define BZ_ALLOWED_MODS (BZ_MOD_SHIFT | BZ_MOD_ALT | BZ_MOD_CTRL | BZ_MOD_SUPER)

typedef enum {
    BZ_MOD_SHIFT = WLR_MODIFIER_SHIFT,
    BZ_MOD_ALT   = WLR_MODIFIER_ALT,
    BZ_MOD_CTRL  = WLR_MODIFIER_CTRL,
    BZ_MOD_SUPER = WLR_MODIFIER_LOGO,
} bz_modifier_t;

typedef enum {
    BZ_BINDING_NONE = 0, /**< No flags. */
    BZ_BINDING_ONRELEASE = 1 << 0, /**< Trigger on release. */
    BZ_BINDING_PASSTHROUGH = 1 << 1, /**< Let the event pass to other keybindings. */
} bz_binding_flags_t;

/**
 * Buzzay keybinding information.
 */
struct bz_keybinding {
    /**
     * They key to listen to.
     */
    xkb_keysym_t sym;
    /**
     * The modifier keys to listen to.
     *
     * You can listen to multiple modifier keys by using `|`. For example, `BZ_MOD_SHIFT | BZ_MOD_CTRL`.
     */
    bz_modifier_t modifiers;
    bz_binding_flags_t flags;
    void (*handler)(struct bz_plugin *plugin, void *data); /**< Function to execute when keybinding is used. */
    void *data; /**< Data to pass into the handler. */
};

/** The binding handle that can be used to unregister a binding. **/
typedef int bz_binding_handle_t;

/**
 * Register a keybinding into buzzay.
 * 
 * **Example:**
 * 
 * ```c
 * void my_handle(struct bz_plugin *plugin, void *data) {
 *     // Do something...
 * }
 *
 * void init_plugin(struct bz_plugin *plugin) {
 *    struct bz_keybinding binding = {
 *        .sym = XKB_KEY_Q,
 *        .modifiers = BZ_MOD_SHIFT | BZ_MOD_SUPER,
 *        .flags = BZ_BINDING_NONE,
 *        .handler = my_handle,
 *        .data = NULL
 *    };
 *    bz_register_keybinding(plugin, binding);
 * }
```
*/
BZ_API bz_binding_handle_t bz_register_keybinding(
    struct bz_plugin *plugin,
    struct bz_keybinding binding
);

/**
 * Unregister a binding you registered into buzzay.
 *
 * **Example:**
 *
 * ```c
 * void init_plugin(struct bz_plugin *plugin) {
 *    // ...
 *    bz_binding_handle_t handle = bz_register_keybinding(plugin, binding);
 *
 *    // Unregister with the handle
 *    bz_unregister_keybinding(handle);
 * }
 * ```
 */
BZ_API void bz_unregister_keybinding(bz_binding_handle_t handle);

#endif
