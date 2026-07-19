#include <stdlib.h>
#include <wlr/types/wlr_idle_inhibit_v1.h>
#include <wlr/types/wlr_idle_notify_v1.h>

#include "server.h"
#include "idle.h"

static void handle_inhibitor_destroy(struct wl_listener *listener, void *data) {
    struct buzzay_inhibitor *inhib = wl_container_of(listener, inhib, destroy);
    struct buzzay_server *server = inhib->server;

    server->idle_inhibit_count--;

    wlr_idle_notifier_v1_set_inhibited(server->idle_notifier, server->idle_inhibit_count > 0);

    wl_list_remove(&inhib->destroy.link);
    free(inhib);
}

void server_new_idle_new_inhibitor(struct wl_listener *listener, void *data) {
    struct buzzay_server *server = wl_container_of(listener, server, idle_new_inhibitor);
    struct wlr_idle_inhibitor_v1 *inhibitor = data;

    server->idle_inhibit_count++;

    wlr_idle_notifier_v1_set_inhibited(server->idle_notifier, server->idle_inhibit_count > 0);

    struct buzzay_inhibitor *idata = calloc(1, sizeof(struct buzzay_inhibitor));
    idata->server = server;
    idata->inhibitor = inhibitor;

    inhibitor->data = idata;
    idata->destroy.notify = handle_inhibitor_destroy;
    wl_signal_add(&inhibitor->events.destroy, &idata->destroy);
}
