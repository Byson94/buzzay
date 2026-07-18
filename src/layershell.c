#include <stdlib.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_layer_shell_v1.h>

#include "server.h"
#include "layershell.h"

static void layershell_new_popup(struct wl_listener *listener, void *data) {
    struct buzzay_layer_surface *layer_surface = wl_container_of(listener, layer_surface, new_popup);

    printf("LayerShell TODO!");
}

static void layershell_destroy(struct wl_listener *listener, void *data) {
    struct buzzay_layer_surface *layer_surface = wl_container_of(listener, layer_surface, destroy);

    wl_list_remove(&layer_surface->new_popup.link);
    wl_list_remove(&layer_surface->destroy.link);
    
    free(layer_surface);
}

void server_new_layer_surface(struct wl_listener *listener, void *data) {
    struct buzzay_server *server = wl_container_of(listener, server, new_layer_surface);
    struct wlr_layer_surface_v1 *surface = data;

    struct buzzay_layer_surface *layer_surface = calloc(1, sizeof(*layer_surface));
    layer_surface->server = server;

    layer_surface->new_popup.notify = layershell_new_popup;
    wl_signal_add(&surface->events.new_popup, &layer_surface->new_popup);

    layer_surface->destroy.notify = layershell_destroy;
    wl_signal_add(&surface->events.destroy, &layer_surface->destroy);
}
