#include <stdlib.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <wlr/backend.h>
#include <wlr/backend/session.h>
#include <wlr/types/wlr_cursor.h>
#include <scenefx/types/wlr_scene.h>
#include <wlr/types/wlr_idle_notify_v1.h>
#include <xkbcommon/xkbcommon.h>

#include "macro-utils.h"
#include "server.h"
#include "input.h"

struct keybinding_entry *keybindings_map = NULL;

void apply_keyboard_config_to_device(struct wlr_keyboard *keyboard, const char *layout, const char *variant, const char *options) {
    struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (!context) {
        return;
    }

    struct xkb_rule_names rules = {
        .model = "pc105",
        .layout = layout,
        .variant = variant,
        .options = options
    };

    struct xkb_keymap *keymap = xkb_keymap_new_from_names(
        context, &rules, XKB_KEYMAP_COMPILE_NO_FLAGS
    );

    if (keymap) {
        wlr_keyboard_set_keymap(keyboard, keymap);
        xkb_keymap_unref(keymap);
    }

    xkb_context_unref(context);
}

static inline uint64_t make_binding_key(uint32_t modifiers, uint32_t key_or_sym, bool is_keycode) {
    uint64_t type_bit = is_keycode ? (1ULL << 63) : 0ULL;
    return type_bit | ((uint64_t)(modifiers & BZ_ALLOWED_MODS) << 32) | key_or_sym;
}

static struct keybinding *find_keybinding_entry(uint32_t modifiers, uint32_t key_or_sym, bool is_keycode) {
    uint64_t lookup_key = make_binding_key(modifiers, key_or_sym, is_keycode);
    struct keybinding_entry *entry = NULL;

    HASH_FIND(hh, keybindings_map, &lookup_key, sizeof(uint64_t), entry);
    return entry ? &entry->kb : NULL;
}

void register_keybinding(struct keybinding binding) {
    uint32_t key_or_sym;

    if (binding.is_keycode) {
        key_or_sym = binding.key.code;
    } else {
        key_or_sym = xkb_keysym_to_lower(binding.key.sym);
    }

    uint64_t lookup_key = make_binding_key(binding.modifiers, key_or_sym, binding.is_keycode);

    struct keybinding_entry *existing = NULL;
    HASH_FIND(hh, keybindings_map, &lookup_key, sizeof(uint64_t), existing);
    if (existing) {
        existing->kb = binding;
        return;
    }

    struct keybinding_entry *entry = malloc(sizeof(*entry));
    if (!entry) {
        fprintf(stderr, "Failed to allocate memory for keybinding entry\n");
        return;
    }

    entry->key = lookup_key;
    entry->kb = binding;

    HASH_ADD(hh, keybindings_map, key, sizeof(uint64_t), entry);
    return;
}

void clear_all_keybinding() {
    struct keybinding_entry *current, *tmp;

    HASH_ITER(hh, keybindings_map, current, tmp) {
        HASH_DEL(keybindings_map, current);
        free(current);
    }
}

static void keyboard_handle_modifiers(struct wl_listener *listener, void *data) {
    UNUSED(data);

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

int handle_kb_repeat_timer(void *data) {
    struct buzzay_server *server = data;
    struct keybinding *kb = server->current_repeat.kb;

    if (kb && kb->handler) {
        kb->handler(server, kb->data);
        wl_event_source_timer_update(server->kb_repeat_timer, server->repeat_rate);
    }

    return 0;
}

struct keybinding *handle_keybinding(struct buzzay_server *server, xkb_keycode_t keycode, xkb_keysym_t sym, uint32_t modifiers) {
    uint32_t clean_mods = modifiers & BZ_ALLOWED_MODS;
    struct keybinding *kb = find_keybinding_entry(clean_mods, keycode, true);

    if (!kb && sym != XKB_KEY_NoSymbol) {
        xkb_keysym_t lower_sym = xkb_keysym_to_lower(sym);
        kb = find_keybinding_entry(clean_mods, lower_sym, false);
    }

    if (kb && kb->handler) {
        kb->handler(server, kb->data);
        return kb;
    }

    return NULL;
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

    // Handle release by clearing timer
    if (event->state == WL_KEYBOARD_KEY_STATE_RELEASED) {
        if (server->current_repeat.keycode == keycode) {
            wl_event_source_timer_update(server->kb_repeat_timer, 0);
            server->current_repeat.kb = NULL;
            server->current_repeat.keycode = 0;
        }
        
        wlr_seat_set_keyboard(seat, keyboard->wlr_keyboard);
        wlr_seat_keyboard_notify_key(seat, event->time_msec, event->keycode, event->state);
        return;
    }

	/* Get a list of keysyms based on the keymap for this keyboard */
	const xkb_keysym_t *syms;
	int nsyms = xkb_state_key_get_syms(
			keyboard->wlr_keyboard->xkb_state, keycode, &syms);

    // get kb modifiers
    uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard->wlr_keyboard);
    xkb_mod_mask_t consumed_mods = xkb_state_key_get_consumed_mods(
            keyboard->wlr_keyboard->xkb_state, keycode);

    // quickly handle tty switching
    if ((modifiers & WLR_MODIFIER_ALT) && 
            (modifiers & WLR_MODIFIER_CTRL) &&
            event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        xkb_keysym_t sym = syms[0];

        if (sym >= XKB_KEY_XF86Switch_VT_1 && sym <= XKB_KEY_XF86Switch_VT_12) {
            unsigned int vt = sym - XKB_KEY_XF86Switch_VT_1 + 1;
            wlr_session_change_vt(server->session, vt);
            return;
        }
    }

    // Clear all timers 
    wl_event_source_timer_update(server->kb_repeat_timer, 0);
    server->current_repeat.kb = NULL;
    server->current_repeat.keycode = 0;

    struct keybinding *matched_kb = NULL;

    // Check keysyms with consumed modifiers stripped
    uint32_t unconsumed_modifiers = modifiers & ~consumed_mods;
    for (int i = 0; i < nsyms; i++) {
        matched_kb = handle_keybinding(server, keycode, syms[i], unconsumed_modifiers);
        if (matched_kb) break;
    }

    // Check raw keybindings
    if (!matched_kb) {
        const xkb_keysym_t *syms_raw;
        int nsyms_raw = xkb_keymap_key_get_syms_by_level(
            keyboard->wlr_keyboard->keymap, 
            keycode, 
            xkb_state_key_get_layout(keyboard->wlr_keyboard->xkb_state, keycode),
            0,
            &syms_raw);

        for (int i = 0; i < nsyms_raw; i++) {
            matched_kb = handle_keybinding(server, keycode, syms_raw[i], modifiers);
            if (matched_kb) break;
        }
    }

    if (matched_kb) {
        if (matched_kb->config.repeat) {
            server->current_repeat.kb = matched_kb;
            server->current_repeat.keycode = keycode;
            server->current_repeat.modifiers = modifiers;

            // Start timer for the initial delay
            wl_event_source_timer_update(server->kb_repeat_timer, server->repeat_delay);
        }
    } else {
        /* Pass non-keybinding presses to client */
        wlr_seat_set_keyboard(seat, keyboard->wlr_keyboard);
        wlr_seat_keyboard_notify_key(seat, event->time_msec, event->keycode, event->state);
    }

    wlr_idle_notifier_v1_notify_activity(server->idle_notifier, server->seat);
}

static void keyboard_handle_destroy(struct wl_listener *listener, void *data) {
    UNUSED(data);

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

