#pragma once

#include "wlr/types/wlr_xdg_decoration_v1.h"
#include <wlr/backend.h>
#include <wayland-client-core.h>
#include <xkbcommon/xkbcommon.h>

struct buzzay_keyboard {
	struct wl_list link;
	struct buzzay_server *server;
	struct wlr_keyboard *wlr_keyboard;

	struct wl_listener modifiers;
	struct wl_listener key;
	struct wl_listener destroy;
};

#define BZ_ALLOWED_MODS (WLR_MODIFIER_SHIFT | WLR_MODIFIER_CAPS | \
        WLR_MODIFIER_CTRL | WLR_MODIFIER_MOD2 | WLR_MODIFIER_MOD3 | \
        WLR_MODIFIER_LOGO | WLR_MODIFIER_ALT | WLR_MODIFIER_MOD5)

struct keybinding {
    xkb_keysym_t sym;
    enum wlr_keyboard_modifier modifiers;
    void (*handler)(struct buzzay_server *server, void *data);
    void *data;
};

extern struct keybinding *keybinding_arr;
extern int keybinding_count;
extern int keybinding_capacity;

void apply_keyboard_config_to_device(struct wlr_keyboard *keyboard, const char *layout, const char *variant, const char *options);
void register_keybinding(struct keybinding binding);
void server_new_input(struct wl_listener *listener, void *data);
