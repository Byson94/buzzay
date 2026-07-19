#include <wlr/backend.h>
#include <wlr/types/wlr_gamma_control_v1.h>

#include "server.h"
#include "gamma.h"

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
}
