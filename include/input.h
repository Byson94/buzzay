#pragma once

#include "wlr/types/wlr_xdg_decoration_v1.h"
#include <wlr/backend.h>
#include <wayland-client-core.h>
#include <xkbcommon/xkbcommon.h>
#include <uthash.h>

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

struct keybinding_config {
    bool repeat;
};

struct keybinding {
    bool is_keycode;
    union {
        xkb_keysym_t sym;
        xkb_keycode_t code;
    } key;
    struct keybinding_config config;
    enum wlr_keyboard_modifier modifiers;
    void (*handler)(struct buzzay_server *server, void *data);
    void *data;
};

struct keybinding_entry {
    uint64_t key;
    struct keybinding kb;
    UT_hash_handle hh;
};

extern struct keybinding_entry *keybindings_map;

void apply_keyboard_config_to_device(struct wlr_keyboard *keyboard, const char *layout, const char *variant, const char *options);
void register_keybinding(struct keybinding binding);
void clear_all_keybinding();
struct keybinding_entry *safe_clear_all_keybindings(void);
void commit_keybinding_clear(struct keybinding_entry *backup_list);
void revert_keybinding_clear(struct keybinding_entry *backup_list);
void server_new_input(struct wl_listener *listener, void *data);
int handle_kb_repeat_timer(void *data);
