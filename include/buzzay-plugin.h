#pragma once

#ifndef BUZZAY_PLUGIN
#define BUZZAY_PLUGIN

#include <xkbcommon/xkbcommon.h>

// MUST increment once every release
// IF a change is made to the file
#define BUZZAY_API_VERSION 1
#define BZ_API __attribute__((visibility("default")))

/**
 *  The plugin wrapper structure that contains important
 *  metadata like plugin name, path, and the buzzay server.
 */
struct bz_plugin {
    const char *plugin_name; /**< The name of the plugin. */
    const char *plugin_path; /**< The version number of the plugin. */

    /**
     * Any data that you can insert into the plugin.
     */
    void *data;

    /**
     * Internal compositor server.
     */
    void *_internal_server;
};

// General

/**
 * Quit buzzay.
 */
BZ_API void bz_quit(struct bz_plugin *plugin);

/** Decoration modes **/
enum bz_decoration_mode {
    /** Let the client draw their own decoration **/
    BZ_DECORATION_CLIENT_SIDE,
    /** Does not apply any decoration **/
    BZ_DECORATION_SERVER_SIDE,
};

/**
 * Set window decoration mode.
 */
BZ_API void bz_set_decoration_mode(struct bz_plugin *plugin, enum bz_decoration_mode mode);

// Keybinding API's
#define BZ_ALLOWED_MODS (BZ_MOD_SHIFT | BZ_MOD_ALT | BZ_MOD_CTRL | BZ_MOD_SUPER)

typedef enum {
    BZ_MOD_SHIFT = 1 << 0, // WLR_MODIFIER_SHIFT
    BZ_MOD_ALT   = 1 << 1, // WLR_MODIFIER_ALT
    BZ_MOD_CTRL  = 1 << 2, // WLR_MODIFIER_CTRL
    BZ_MOD_SUPER = 1 << 3, // WLR_MODIFIER_LOGO
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
    void (*handler)(struct bz_plugin *plugin); /**< Function to execute when keybinding is used. */
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

// Window Manager API'S

enum bz_window_hook {
    BZ_WINDOW_RESIZE,
    BZ_WINDOW_MOVE,
    BZ_WINDOW_MAXIMIZE,
    BZ_WINDOW_FULLSCREEN,
};

BZ_API void bz_window_hook(
        struct bz_plugin *plugin, 
        enum bz_window_hook hook,
        void (*callback)(struct bz_plugin *plugin));

BZ_API void bz_get_active_window(struct bz_plugin *plugin);

enum bz_window_event {
    BZ_WINDOW_HOVER,
    BZ_WINDOW_CLICK
};

BZ_API void bz_window_active_on(struct bz_plugin *plugin, enum bz_window_event mode);

#endif
