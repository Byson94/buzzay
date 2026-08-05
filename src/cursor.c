#include <stdlib.h>
#include <wayland-client-core.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <wayland-util.h>
#include <wlr/backend.h>
#include <wlr/util/log.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_seat.h>
#include <scenefx/types/wlr_scene.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_cursor_shape_v1.h>
#include <wlr/types/wlr_primary_selection.h>
#include <wlr/types/wlr_idle_notify_v1.h>

#include "macro-utils.h"
#include "layershell.h"
#include "server.h"
#include "cursor.h"
#include "xdg.h"

static struct bz_underlying_surface *desktop_toplevel_at(
		struct buzzay_server *server, double lx, double ly,
		struct wlr_surface **surface, double *sx, double *sy) {
	/* This returns the topmost node in the scene at the given layout coords.
	 * We only care about surface nodes as we are specifically looking for a
	 * surface in the surface tree of a buzzay_toplevel. */
	struct wlr_scene_node *node = wlr_scene_node_at(
		&server->scene->tree.node, lx, ly, sx, sy);
	if (node == NULL || node->type != WLR_SCENE_NODE_BUFFER) {
		return NULL;
	}
	struct wlr_scene_buffer *scene_buffer = wlr_scene_buffer_from_node(node);
	struct wlr_scene_surface *scene_surface =
		wlr_scene_surface_try_from_buffer(scene_buffer);
	if (!scene_surface) {
		return NULL;
	}

	*surface = scene_surface->surface;
	/* Find the node corresponding to the buzzay_toplevel at the root of this
	 * surface tree, it is the only one for which we set the data field. */
	struct wlr_scene_tree *tree = node->parent;
	while (tree != NULL && tree->node.data == NULL) {
		tree = tree->node.parent;
	}

    if (tree == NULL) return NULL;
	return tree->node.data;
}

static void process_cursor_move(struct buzzay_server *server) {
	/* Move the grabbed toplevel to the new position. */
	struct buzzay_toplevel *toplevel = server->grabbed_toplevel;
	wlr_scene_node_set_position(&toplevel->scene_tree->node,
		server->cursor->x - server->grab_x,
		server->cursor->y - server->grab_y);
}

static void process_cursor_resize(struct buzzay_server *server) {
	/*
	 * Resizing the grabbed toplevel can be a little bit complicated, because we
	 * could be resizing from any corner or edge. This not only resizes the
	 * toplevel on one or two axes, but can also move the toplevel if you resize
	 * from the top or left edges (or top-left corner).
	 *
	 * Note that some shortcuts are taken here. In a more fleshed-out
	 * compositor, you'd wait for the client to prepare a buffer at the new
	 * size, then commit any movement that was prepared.
	 */
	struct buzzay_toplevel *toplevel = server->grabbed_toplevel;
	double border_x = server->cursor->x - server->grab_x;
	double border_y = server->cursor->y - server->grab_y;
	int new_left = server->grab_geobox.x;
	int new_right = server->grab_geobox.x + server->grab_geobox.width;
	int new_top = server->grab_geobox.y;
	int new_bottom = server->grab_geobox.y + server->grab_geobox.height;

	if (server->resize_edges & WLR_EDGE_TOP) {
		new_top = border_y;
		if (new_top >= new_bottom) {
			new_top = new_bottom - 1;
		}
	} else if (server->resize_edges & WLR_EDGE_BOTTOM) {
		new_bottom = border_y;
		if (new_bottom <= new_top) {
			new_bottom = new_top + 1;
		}
	}
	if (server->resize_edges & WLR_EDGE_LEFT) {
		new_left = border_x;
		if (new_left >= new_right) {
			new_left = new_right - 1;
		}
	} else if (server->resize_edges & WLR_EDGE_RIGHT) {
		new_right = border_x;
		if (new_right <= new_left) {
			new_right = new_left + 1;
		}
	}

	struct wlr_box *geo_box = &toplevel->xdg_toplevel->base->geometry;
	wlr_scene_node_set_position(&toplevel->scene_tree->node,
		new_left - geo_box->x, new_top - geo_box->y);

	int new_width = new_right - new_left;
	int new_height = new_bottom - new_top;
	wlr_xdg_toplevel_set_size(toplevel->xdg_toplevel, new_width, new_height);
}

static bool is_native_overlay(struct buzzay_server *server) {
    if (!server || !server->layers.native_overlay) {
        return false;
    }

    double sx, sy;
    struct wlr_scene_node *overlay_node = wlr_scene_node_at(
        &server->layers.native_overlay->node, 
        server->cursor->x, 
        server->cursor->y, 
        &sx, &sy
    );

    if (overlay_node != NULL) {
        return true;
    }

    return false;
}

void process_cursor_motion(struct buzzay_server *server, uint32_t *time) {
    // Notify activity first.
    wlr_idle_notifier_v1_notify_activity(server->idle_notifier, server->seat);

	/* If the mode is non-passthrough, delegate to those functions. */
	if (server->cursor_mode == BUZZAY_CURSOR_MOVE) {
		process_cursor_move(server);
		return;
	} else if (server->cursor_mode == BUZZAY_CURSOR_RESIZE) {
		process_cursor_resize(server);
		return;
	}

	double sx, sy;
	struct wlr_seat *seat = server->seat;
	struct wlr_surface *surface = NULL;
	struct bz_underlying_surface *surface_under = desktop_toplevel_at(server,
			server->cursor->x, server->cursor->y, &surface, &sx, &sy);

    struct buzzay_toplevel *toplevel = NULL;
    struct buzzay_layer_surface *layershell = NULL;

    if (surface_under) {
        switch (surface_under->type) {
            case BUZZAY_SURFACE_TOPLEVEL:
                toplevel = surface_under->item;
                break;
            case BUZZAY_SURFACE_LAYERSHELL:
                layershell = surface_under->item;
                break;
        }
    }

    // Move the dragged thing to the cursor
    if (server->active_drag_icon_state && server->active_drag_icon_state->scene_tree) {
        wlr_scene_node_set_position(
            &server->active_drag_icon_state->scene_tree->node,
            server->cursor->x,
            server->cursor->y
        );
        set_cursor_shape(server, "grabbing");
    }

	if (!toplevel && !layershell) {
		/* If there's no toplevel and layersehll under the cursor, set the cursor image to a
		 * default. This is what makes the cursor image appear when you move it
		 * around the screen, not over any toplevels or layershell. */
        set_cursor_shape(server, "default");
	}

    if (server->window_active_on == WINDOW_ACTIVE_ON_HOVER && !server->focused_layersehll) {
        if (toplevel != NULL && server->hovered_toplevel != toplevel) {
            focus_toplevel(toplevel);
            server->hovered_toplevel = toplevel;
        } else if (toplevel == NULL) {
            server->hovered_toplevel = NULL;
        } 

        if (layershell != NULL) {
            focus_layershell(layershell);
            server->hovered_toplevel = NULL;
        }
    }

    if (is_native_overlay(server)) return;
	if (surface) {
		/*
		 * Send pointer enter and motion events.
		 *
		 * The enter event gives the surface "pointer focus", which is distinct
		 * from keyboard focus. You get pointer focus by moving the pointer over
		 * a window.
		 *
		 * Note that wlroots will avoid sending duplicate enter/motion events if
		 * the surface has already has pointer focus or if the client is already
		 * aware of the coordinates passed.
		 */
		wlr_seat_pointer_notify_enter(seat, surface, sx, sy);
        if (time)
            wlr_seat_pointer_notify_motion(seat, *time, sx, sy);
	} else {
		/* Clear pointer focus so future button events and such are not sent to
		 * the last client to have the cursor over it. */
        if (server->focused_layersehll) return;
        if (server->window_active_on != WINDOW_ACTIVE_ON_CLICK) {
            wlr_seat_pointer_clear_focus(seat);
            wlr_seat_keyboard_clear_focus(seat);
        }
	}
}

void server_cursor_motion(struct wl_listener *listener, void *data) {
	/* This event is forwarded by the cursor when a pointer emits a _relative_
	 * pointer motion event (i.e. a delta) */
	struct buzzay_server *server =
		wl_container_of(listener, server, cursor_motion);
	struct wlr_pointer_motion_event *event = data;
	/* The cursor doesn't move unless we tell it to. The cursor automatically
	 * handles constraining the motion to the output layout, as well as any
	 * special configuration applied for the specific input device which
	 * generated the event. You can pass NULL for the device if you want to move
	 * the cursor around without any input. */
	wlr_cursor_move(server->cursor, &event->pointer->base,
			event->delta_x, event->delta_y);
    process_cursor_motion(server, &event->time_msec);
}

void server_cursor_motion_absolute(
		struct wl_listener *listener, void *data) {
	/* This event is forwarded by the cursor when a pointer emits an _absolute_
	 * motion event, from 0..1 on each axis. This happens, for example, when
	 * wlroots is running under a Wayland window rather than KMS+DRM, and you
	 * move the mouse over the window. You could enter the window from any edge,
	 * so we have to warp the mouse there. There is also some hardware which
	 * emits these events. */
	struct buzzay_server *server =
		wl_container_of(listener, server, cursor_motion_absolute);
	struct wlr_pointer_motion_absolute_event *event = data;
	wlr_cursor_warp_absolute(server->cursor, &event->pointer->base, event->x,
		event->y);

    process_cursor_motion(server, &event->time_msec);
}

void server_cursor_button(struct wl_listener *listener, void *data) {
	/* This event is forwarded by the cursor when a pointer emits a button
	 * event. */
	struct buzzay_server *server =
		wl_container_of(listener, server, cursor_button);
	struct wlr_pointer_button_event *event = data;

	/* Notify the client with pointer focus that a button press has occurred */
	uint32_t serial = wlr_seat_pointer_notify_button(server->seat,
			event->time_msec, event->button, event->state);

    server->last_serial = serial;
    server->cursor_recently_reset = false;

	if (event->state == WL_POINTER_BUTTON_STATE_RELEASED) {
        if (server->cursor_mode != BUZZAY_CURSOR_PASSTHROUGH) {
            reset_cursor_mode(server);
            server->cursor_recently_reset = true;
        }
	} else if (server->window_active_on == WINDOW_ACTIVE_ON_CLICK) {
		/* Focus that client if the button was _pressed_ */
        double sx, sy;
        struct wlr_surface *surface = NULL;
        struct bz_underlying_surface *surface_under = desktop_toplevel_at(server,
                server->cursor->x, server->cursor->y, &surface, &sx, &sy);

        if (!surface_under) return;

        switch(surface_under->type) {
            case BUZZAY_SURFACE_TOPLEVEL: {
                struct buzzay_toplevel *toplevel = surface_under->item;
                focus_toplevel(toplevel);
                break;
            }
            case BUZZAY_SURFACE_LAYERSHELL: {
                struct buzzay_layer_surface *layershell = surface_under->item;
                focus_layershell(layershell);
                break;
            }
        }
	}
}

void server_cursor_axis(struct wl_listener *listener, void *data) {
	/* This event is forwarded by the cursor when a pointer emits an axis event,
	 * for example when you move the scroll wheel. */
	struct buzzay_server *server =
		wl_container_of(listener, server, cursor_axis);
	struct wlr_pointer_axis_event *event = data;
	/* Notify the client with pointer focus of the axis event. */
	wlr_seat_pointer_notify_axis(server->seat,
			event->time_msec, event->orientation, event->delta,
			event->delta_discrete, event->source, event->relative_direction);
}

void server_cursor_frame(struct wl_listener *listener, void *data) {
    UNUSED(data);

	/* This event is forwarded by the cursor when a pointer emits an frame
	 * event. Frame events are sent after regular pointer events to group
	 * multiple events together. For instance, two axis events may happen at the
	 * same time, in which case a frame event won't be sent in between. */
	struct buzzay_server *server =
		wl_container_of(listener, server, cursor_frame);
	/* Notify the client with pointer focus of the frame event. */
	wlr_seat_pointer_notify_frame(server->seat);
}

void seat_pointer_focus_change(struct wl_listener *listener, void *data) {
	struct buzzay_server *server = wl_container_of(
			listener, server, pointer_focus_change);
	/* This event is raised when the pointer focus is changed, including when the
	 * client is closed. We set the cursor image to its default if target surface
	 * is NULL */
	struct wlr_seat_pointer_focus_change_event *event = data;
	if (event->new_surface == NULL) {
        set_cursor_shape(server, "default");
	}
}

void seat_request_set_selection(struct wl_listener *listener, void *data) {
	/* This event is raised by the seat when a client wants to set the selection,
	 * usually when the user copies something. wlroots allows compositors to
	 * ignore such requests if they so choose, but in buzzay we always honor
	 */
	struct buzzay_server *server = wl_container_of(
			listener, server, request_set_selection);
	struct wlr_seat_request_set_selection_event *event = data;
	wlr_seat_set_selection(server->seat, event->source, event->serial);
}

void seat_request_cursor(struct wl_listener *listener, void *data) {
	struct buzzay_server *server = wl_container_of(
			listener, server, request_cursor);
	/* This event is raised by the seat when a client provides a cursor image */
	struct wlr_seat_pointer_request_set_cursor_event *event = data;
	struct wlr_seat_client *focused_client =
		server->seat->pointer_state.focused_client;
	/* This can be sent by any client, so we check to make sure this one is
	 * actually has pointer focus first. */
	if (focused_client == event->seat_client) {
		/* Once we've vetted the client, we can tell the cursor to use the
		 * provided surface as the cursor image. It will set the hardware cursor
		 * on the output that it's currently on and continue to do so as the
		 * cursor moves between outputs. */
		wlr_cursor_set_surface(server->cursor, event->surface,
				event->hotspot_x, event->hotspot_y);
	}
}

void server_handle_request_start_drag(struct wl_listener *listener, void *data) {
    struct buzzay_server *server = wl_container_of(listener, server, request_start_drag);
    struct wlr_seat_request_start_drag_event *event = data;

    if (wlr_seat_validate_pointer_grab_serial(server->seat, event->origin, event->serial)) {
        wlr_seat_start_pointer_drag(server->seat, event->drag, event->serial);
    } else {
        wlr_data_source_destroy(event->drag->source);
    }
}


static void drag_icon_handle_destroy(struct wl_listener *listener, void *data) {
    UNUSED(data);
    struct drag_icon_state *state = wl_container_of(listener, state, destroy);
    state->server->active_drag_icon_state = NULL;

    process_cursor_motion(state->server, NULL);
    wl_list_remove(&state->destroy.link);
    free(state);
}

void server_handle_start_drag(struct wl_listener *listener, void *data) {
    struct buzzay_server *server = wl_container_of(listener, server, seat_start_drag);
    struct wlr_drag *drag = data;

    if (drag->icon) {
        struct drag_icon_state *state = calloc(1, sizeof(*state));
        state->icon = drag->icon;
        state->server = server;
        state->scene_tree = wlr_scene_drag_icon_create(&server->scene->tree, drag->icon);

        state->destroy.notify = drag_icon_handle_destroy;
        wl_signal_add(&drag->icon->events.destroy, &state->destroy);

        server->active_drag_icon_state = state;
    }
}

void seat_set_primary_selection(struct wl_listener *listener, void *data) {
    struct buzzay_server *server = wl_container_of(listener, server, set_primary_selection);
    struct wlr_seat_request_set_primary_selection_event *event = data;
    wlr_seat_set_primary_selection(server->seat, event->source, event->serial);
}

// Curosr shape protocol 
void set_cursor_shape_forced(struct buzzay_server *server, const char *shape) {
    wlr_cursor_set_xcursor(server->cursor, server->cursor_mgr, shape);
    server->current_cursor_shape = shape;
}

void set_cursor_shape(struct buzzay_server *server, const char *shape) {
    /*
     * Unlike the 'forced' variant, this function only
     * sets the cursor if the current cursor does not
     * match the to-be-set cursor.
     */
    if (server->current_cursor_shape && strcmp(server->current_cursor_shape, shape) == 0) {
        return;
    }
    set_cursor_shape_forced(server, shape);
}

void server_new_request_cursor_set_shape(struct wl_listener *listener, void *data) {
    struct buzzay_server *server = wl_container_of(listener, server, cursor_request_set_shape);
    struct wlr_cursor_shape_manager_v1_request_set_shape_event *shape_event = data;
    const char *shape_name = wlr_cursor_shape_v1_name(shape_event->shape);

    struct wlr_seat_client *focused_client  = server->seat->pointer_state.focused_client;
    if (focused_client == shape_event->seat_client) {
        /*
         * Valid place to forcefully set cursor shape 
         * as clients wont request it that often in terms
         * of every cursor motion. Moreover, doing this forcefully
         * is necessary to break the cursor out of hiding, which 
         * it can get into by being idle.
         */
        set_cursor_shape_forced(server, shape_name);
    }
}
