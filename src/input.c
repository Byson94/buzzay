#include <stdlib.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <wlr/backend.h>
#include <wlr/backend/session.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_xdg_shell.h>

#include "buzzay-plugin.h"
#include "handle-plugin.h"
#include "server.h"
#include "input.h"

static void keyboard_handle_modifiers(
		struct wl_listener *listener, void *data) {
	/* This event is raised when a modifier key, such as shift or alt, is
	 * pressed. We simply communicate this to the client. */
	struct buzzay_keyboard *keyboard =
		wl_container_of(listener, keyboard, modifiers);
	/*
	 * A seat can only have one keyboard, but this is a limitation of the
	 * Wayland protocol - not wlroots. We assign all connected keyboards to the
	 * same seat. You can swap out the underlying wlr_keyboard like this and
	 * wlr_seat handles this transparently.
	 */
	wlr_seat_set_keyboard(keyboard->server->seat, keyboard->wlr_keyboard);
	/* Send modifiers to the client. */
	wlr_seat_keyboard_notify_modifiers(keyboard->server->seat,
		&keyboard->wlr_keyboard->modifiers);
}

static uint32_t bz_to_wlr_modifiers(uint32_t bz_mods) {
    uint32_t wlr_mods = 0;
    if (bz_mods & BZ_MOD_SHIFT) wlr_mods |= WLR_MODIFIER_SHIFT;
    if (bz_mods & BZ_MOD_ALT)   wlr_mods |= WLR_MODIFIER_ALT;
    if (bz_mods & BZ_MOD_CTRL)  wlr_mods |= WLR_MODIFIER_CTRL;
    if (bz_mods & BZ_MOD_SUPER) wlr_mods |= WLR_MODIFIER_LOGO;
    return wlr_mods;
}

bool handle_keybinding(struct buzzay_server *server, xkb_keysym_t sym, uint32_t modifiers, bool is_release) {
    bool is_passthrough = false;

    for (int i = 0; i < keybinding_count; i++) {
        struct keybinding_data *kb_dat = &keybinding_arr[i];
        struct bz_keybinding *kb = &kb_dat->binding;

        bool binding_wants_release = (kb->flags & BZ_BINDING_ONRELEASE) != 0;
        bool binding_wants_passthrough = (kb->flags & BZ_BINDING_PASSTHROUGH) != 0;

        if (binding_wants_release != is_release) {
            continue;
        }

        if (kb->sym == sym 
                && bz_to_wlr_modifiers(modifiers & BZ_ALLOWED_MODS) 
                == (kb->modifiers & bz_to_wlr_modifiers(BZ_ALLOWED_MODS))) {
            if (kb->handler) {
                kb->handler(kb_dat->owner);

                if (binding_wants_passthrough) {
                    is_passthrough = true;
                } else {
                    return true;
                }
            }
        }
    }
    
    if (is_passthrough) {
        return true;
    }

    return false;
}

static void keyboard_handle_key(struct wl_listener *listener, void *data) {
	/* This event is raised when a key is pressed or released. */
	struct buzzay_keyboard *keyboard =
		wl_container_of(listener, keyboard, key);
	struct buzzay_server *server = keyboard->server;
	struct wlr_keyboard_key_event *event = data;
	struct wlr_seat *seat = server->seat;

	/* Translate libinput keycode -> xkbcommon */
	uint32_t keycode = event->keycode + 8;
	/* Get a list of keysyms based on the keymap for this keyboard */
	const xkb_keysym_t *syms;
	int nsyms = xkb_state_key_get_syms(
			keyboard->wlr_keyboard->xkb_state, keycode, &syms);

    // get kb modifiers
    uint32_t modifiers = 0;
    if (xkb_state_mod_index_is_active(keyboard->wlr_keyboard->xkb_state, 
            xkb_keymap_mod_get_index(keyboard->wlr_keyboard->keymap, XKB_MOD_NAME_SHIFT), 
            XKB_STATE_MODS_EFFECTIVE)) modifiers |= WLR_MODIFIER_SHIFT;
    if (xkb_state_mod_index_is_active(keyboard->wlr_keyboard->xkb_state, 
            xkb_keymap_mod_get_index(keyboard->wlr_keyboard->keymap, XKB_MOD_NAME_ALT), 
            XKB_STATE_MODS_EFFECTIVE)) modifiers |= WLR_MODIFIER_ALT;
    if (xkb_state_mod_index_is_active(keyboard->wlr_keyboard->xkb_state, 
            xkb_keymap_mod_get_index(keyboard->wlr_keyboard->keymap, XKB_MOD_NAME_CTRL), 
            XKB_STATE_MODS_EFFECTIVE)) modifiers |= WLR_MODIFIER_CTRL;
    if (xkb_state_mod_index_is_active(keyboard->wlr_keyboard->xkb_state, 
            xkb_keymap_mod_get_index(keyboard->wlr_keyboard->keymap, XKB_MOD_NAME_LOGO), 
            XKB_STATE_MODS_EFFECTIVE)) modifiers |= WLR_MODIFIER_LOGO;

    // quickly handle tty switching
    if ((modifiers & WLR_MODIFIER_ALT) && 
            (modifiers & WLR_MODIFIER_CTRL) &&
            event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        xkb_keysym_t sym = syms[0];

        if (sym >= XKB_KEY_XF86Switch_VT_1 && sym <= XKB_KEY_XF86Switch_VT_12) {
            unsigned int vt = sym - XKB_KEY_XF86Switch_VT_1 + 1;
            wlr_session_change_vt(server->session, vt);
        }
    }

    // else handling compositor key binding
    bool handled = false;
    bool is_release = false;

    if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        is_release = false;
    } else {
        is_release = true;
    }

    for (int i = 0; i < nsyms; i++) {
        if (handle_keybinding(server, syms[i], modifiers, is_release)) {
            handled = true;
            break;
        }
    }

	if (!handled) {
		/* Otherwise, we pass it along to the client. */
		wlr_seat_set_keyboard(seat, keyboard->wlr_keyboard);
		wlr_seat_keyboard_notify_key(seat, event->time_msec,
			event->keycode, event->state);
	}
}

static void keyboard_handle_destroy(struct wl_listener *listener, void *data) {
	/* This event is raised by the keyboard base wlr_input_device to signal
	 * the destruction of the wlr_keyboard. It will no longer receive events
	 * and should be destroyed.
	 */
	struct buzzay_keyboard *keyboard =
		wl_container_of(listener, keyboard, destroy);
	wl_list_remove(&keyboard->modifiers.link);
	wl_list_remove(&keyboard->key.link);
	wl_list_remove(&keyboard->destroy.link);
	wl_list_remove(&keyboard->link);
	free(keyboard);
}

static void server_new_keyboard(struct buzzay_server *server,
		struct wlr_input_device *device) {
	struct wlr_keyboard *wlr_keyboard = wlr_keyboard_from_input_device(device);

	struct buzzay_keyboard *keyboard = calloc(1, sizeof(*keyboard));
	keyboard->server = server;
	keyboard->wlr_keyboard = wlr_keyboard;

	/* We need to prepare an XKB keymap and assign it to the keyboard. This
	 * assumes the defaults (e.g. layout = "us"). */
	struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	struct xkb_keymap *keymap = xkb_keymap_new_from_names(context, NULL,
		XKB_KEYMAP_COMPILE_NO_FLAGS);

	wlr_keyboard_set_keymap(wlr_keyboard, keymap);
	xkb_keymap_unref(keymap);
	xkb_context_unref(context);
	wlr_keyboard_set_repeat_info(wlr_keyboard, 25, 600);

	/* Here we set up listeners for keyboard events. */
	keyboard->modifiers.notify = keyboard_handle_modifiers;
	wl_signal_add(&wlr_keyboard->events.modifiers, &keyboard->modifiers);
	keyboard->key.notify = keyboard_handle_key;
	wl_signal_add(&wlr_keyboard->events.key, &keyboard->key);
	keyboard->destroy.notify = keyboard_handle_destroy;
	wl_signal_add(&device->events.destroy, &keyboard->destroy);

	wlr_seat_set_keyboard(server->seat, keyboard->wlr_keyboard);

	/* And add the keyboard to our list of keyboards */
	wl_list_insert(&server->keyboards, &keyboard->link);
}

static void server_new_pointer(struct buzzay_server *server,
		struct wlr_input_device *device) {
	/* We don't do anything special with pointers. All of our pointer handling
	 * is proxied through wlr_cursor. On another compositor, you might take this
	 * opportunity to do libinput configuration on the device to set
	 * acceleration, etc. */
	wlr_cursor_attach_input_device(server->cursor, device);
}

void server_new_input(struct wl_listener *listener, void *data) {
	/* This event is raised by the backend when a new input device becomes
	 * available. */
	struct buzzay_server *server =
		wl_container_of(listener, server, new_input);
	struct wlr_input_device *device = data;
	switch (device->type) {
	case WLR_INPUT_DEVICE_KEYBOARD:
		server_new_keyboard(server, device);
		break;
	case WLR_INPUT_DEVICE_POINTER:
		server_new_pointer(server, device);
		break;
	default:
		break;
	}
	/* We need to let the wlr_seat know what our capabilities are, which is
	 * communiciated to the client. In TinyWL we always have a cursor, even if
	 * there are no pointer devices, so we always include that capability. */
	uint32_t caps = WL_SEAT_CAPABILITY_POINTER;
	if (!wl_list_empty(&server->keyboards)) {
		caps |= WL_SEAT_CAPABILITY_KEYBOARD;
	}
	wlr_seat_set_capabilities(server->seat, caps);
}

