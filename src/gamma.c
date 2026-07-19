#include <stdlib.h>
#include <wayland-util.h>
#include <wlr/backend.h>
#include <wlr/backend/session.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_gamma_control_v1.h>
#include <wlr/render/color.h>

#include "server.h"
#include "gamma.h"

static void handle_gamma_control_destroy(struct wl_listener *listener, void *data) {
    struct buzzay_gamma *bz_gamma = wl_container_of(listener, bz_gamma, destroy);
    
    // check if the user changed TTY
    if (!bz_gamma->output->enabled || !bz_gamma->session->active) {
        goto cleanup;
    }

    struct wlr_output_state state;
    wlr_output_state_init(&state);

    wlr_output_state_set_color_transform(&state, NULL);
    wlr_output_commit_state(bz_gamma->output, &state);
    wlr_output_state_finish(&state);

cleanup:
    wl_list_remove(&bz_gamma->destroy.link);
    free(bz_gamma);
}

void server_new_set_gamma(struct wl_listener *listener, void *data) {
    struct buzzay_server *server = wl_container_of(listener, server, set_gamma);
    struct wlr_gamma_control_manager_v1_set_gamma_event *gamma_event = data;
    struct wlr_output *output = gamma_event->output;
    struct wlr_gamma_control_v1 *gamma_ctrl = gamma_event->control;

    struct wlr_output_state state;
    wlr_output_state_init(&state);

    // return if gamma is null for whatever reason
    if (gamma_ctrl == NULL) {
        wlr_gamma_control_v1_send_failed_and_destroy(gamma_ctrl);
        return;
    }

    if (!wlr_gamma_control_v1_apply(gamma_ctrl, &state)) {
        wlr_gamma_control_v1_send_failed_and_destroy(gamma_ctrl);
        wlr_output_state_finish(&state);
        return;
    }

    if (!wlr_output_commit_state(output, &state)) {
        wlr_gamma_control_v1_send_failed_and_destroy(gamma_ctrl);
    }

    wlr_output_state_finish(&state);

    struct buzzay_gamma *bz_gamma = calloc(1, sizeof(struct buzzay_gamma));
    bz_gamma->session = server->session;
    bz_gamma->output = output;
    bz_gamma->destroy.notify = handle_gamma_control_destroy;
    wl_resource_add_destroy_listener(gamma_ctrl->resource, &bz_gamma->destroy);
}
