#include <stdlib.h>
#include <wayland-client-core.h>
#include <wlr/backend.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <scenefx/types/wlr_scene.h>

#include "macro-utils.h"
#include "server.h"
#include "output.h"

static void output_configure_scene(struct wlr_scene_node *node,
		struct buzzay_toplevel *toplevel) {
	if (!node->enabled) {
		return;
	}

	struct buzzay_toplevel *_toplevel = node->data;
	if (_toplevel) {
		toplevel = _toplevel;
	}

	if (node->type == WLR_SCENE_NODE_BUFFER) {
		struct wlr_scene_buffer *buffer = wlr_scene_buffer_from_node(node);

		struct wlr_scene_surface * scene_surface =
			wlr_scene_surface_try_from_buffer(buffer);
		if (!scene_surface) {
			return;
		}

		struct wlr_xdg_surface *xdg_surface =
			wlr_xdg_surface_try_from_wlr_surface(scene_surface->surface);

		if (toplevel &&
				xdg_surface &&
				xdg_surface->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL) {
			wlr_scene_buffer_set_opacity(buffer, toplevel->server->eyecandies.window_opacity);
            wlr_scene_buffer_set_corner_radii(buffer, corner_radii_all(toplevel->server->eyecandies.corner_radius));
		}
	} else if (node->type == WLR_SCENE_NODE_TREE) {
		struct wlr_scene_tree *tree = wl_container_of(node, tree, node);
		struct wlr_scene_node *node;
		wl_list_for_each(node, &tree->children, link) {
			output_configure_scene(node, toplevel);
		}
	}
}

static void output_frame(struct wl_listener *listener, void *data) {
    UNUSED(data);

	/* This function is called every time an output is ready to display a frame,
	 * generally at the output's refresh rate (e.g. 60Hz). */
	struct buzzay_output *output = wl_container_of(listener, output, frame);
	struct wlr_scene *scene = output->server->scene;

	struct wlr_scene_output *scene_output = wlr_scene_get_scene_output(scene, output->wlr_output);
    output_configure_scene(&scene_output->scene->tree.node, NULL);

	/* Render the scene if needed and commit the output */
	wlr_scene_output_commit(scene_output, NULL);

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	wlr_scene_output_send_frame_done(scene_output, &now);
}

static void output_request_state(struct wl_listener *listener, void *data) {
	/* This function is called when the backend requests a new state for
	 * the output. For example, Wayland and X11 backends request a new mode
	 * when the output window is resized. */
	struct buzzay_output *output = wl_container_of(listener, output, request_state);
	const struct wlr_output_event_request_state *event = data;
	wlr_output_commit_state(output->wlr_output, event->state);

	wlr_scene_optimized_blur_set_size(output->server->layers.blur,
			output->wlr_output->width, output->wlr_output->height);
}

static void output_destroy(struct wl_listener *listener, void *data) {
    UNUSED(data);

	struct buzzay_output *output = wl_container_of(listener, output, destroy);

	wl_list_remove(&output->frame.link);
	wl_list_remove(&output->request_state.link);
	wl_list_remove(&output->destroy.link);
	wl_list_remove(&output->link);
	free(output);
}


void server_new_output(struct wl_listener *listener, void *data) {
    // ran when a new output (i.e display) is available.
    struct buzzay_server *server = wl_container_of(listener, server, new_output);
    struct wlr_output *wlr_output = data;

    // tell it to use our allocator and renderer
    wlr_output_init_render(wlr_output, server->allocator, server->renderer);

    // enable output
    struct wlr_output_state state;
    wlr_output_state_init(&state);
    wlr_output_state_set_enabled(&state, true);

    struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
    if (mode != NULL) {
        wlr_output_state_set_mode(&state, mode);
    }

    wlr_output_commit_state(wlr_output, &state);
    wlr_output_state_finish(&state);

    struct buzzay_output *output = calloc(1, sizeof(*output));
    output->wlr_output = wlr_output;
    output->server = server;

    output->frame.notify = output_frame;
    wl_signal_add(&wlr_output->events.frame, &output->frame);

    output->request_state.notify = output_request_state;
    wl_signal_add(&wlr_output->events.request_state, &output->request_state);

    output->destroy.notify = output_destroy;
    wl_signal_add(&wlr_output->events.destroy, &output->destroy);
    wl_list_insert(&server->outputs, &output->link);

    struct wlr_output_layout_output *l_output = wlr_output_layout_add_auto(server->output_layout, wlr_output);
    struct wlr_scene_output *scene_output = wlr_scene_output_create(server->scene, wlr_output);

    wlr_scene_output_layout_add_output(server->scene_layout, l_output, scene_output);

	wlr_scene_optimized_blur_set_size(output->server->layers.blur,
			output->wlr_output->width, output->wlr_output->height);
}

