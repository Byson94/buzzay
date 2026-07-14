#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/util/log.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/render/allocator.h>

#include "server.h"
#include "output.h"
#include "xdg.h"

const char *program_name = "buzzay";
const char *program_ver = "0.1.0";

void print_help() {
    printf("A very extensible wayland compositor.\n\n");
    printf("Usage: %s [OPTIONS]\n\n", program_name);
    printf("Options:\n");
    printf("  -l, --load     Load a plugin\n");
    printf("  -m, --msg      Send a message to a plugin\n");
    printf("  -h, --help     Show this help message and exit\n");
    printf("  -v, --version  Print the program version\n");
}

int main(int argc, char** argv) {
    int opt;
    int option_index = 0;

    static struct option long_options[] = {
        { "load", required_argument, 0, 'l' },
        { "msg", required_argument, 0, 'm' },
        { "help", no_argument, 0, 'h' },
        { "version", no_argument, 0, 'v' },
        {0, 0, 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "+hvlm", long_options, NULL)) != -1) {
        switch (opt) {
            case 'l':
                if ((argc - optind) > 1) {
                    printf("'%s' accepts only one argument.\n", argv[optind - 1]);
                    return 1;
                }

                const char* plugin = argv[optind];

                break;
            case 'm':
                for (int i = optind; i < argc; i++) {
                    printf("%s\n", argv[i]);
                }
                break;
            case 'h':
                print_help();
                break;
            case 'v': 
                printf("%s v%s\n", program_name, program_ver);
                break;
        }
    }

    for (int i = optind; i < argc; i++) {
        if (argv[i][0] == '-') {
            fprintf(stderr, "Warning: Unexpected '%s' flag. "
                            "Only one option must be used in each command.\n", argv[i]);
        }
    }

    // setup compositor
    wlr_log_init(WLR_DEBUG, NULL);

    struct buzzay_server server = {0};

    // - managed by libwayland. 
    // - manages many stuff.
    server.wl_display = wl_display_create();

    // abstraction of i/o
    server.backend = wlr_backend_autocreate(wl_display_get_event_loop(server.wl_display), NULL);

    if (server.backend == NULL) {
        wlr_log(WLR_ERROR, "failed to create wlr_backend");
        return 1;
    }

    // Create a renderer.
    // WLR_RENDERER env var can be set to specify one.
	server.renderer = wlr_renderer_autocreate(server.backend);
	if (server.renderer == NULL) {
		wlr_log(WLR_ERROR, "failed to create wlr_renderer");
		return 1;
	}

    wlr_renderer_init_wl_display(server.renderer, server.wl_display);

    // allocator is the bridge between renderer and backend,
    // it handles buffer creation allow wlr to render to screen.
    server.allocator = wlr_allocator_autocreate(server.backend, server.renderer);
    if (server.allocator == NULL) {
        wlr_log(WLR_ERROR, "failed to create wlr_allocator");
        return 1;
    }

    wlr_compositor_create(server.wl_display, 5, server.renderer);
    wlr_subcompositor_create(server.wl_display);
    wlr_data_device_manager_create(server.wl_display);

    // output layout helps in working with arrangement of screens in 
    // a physical layout.
    server.output_layout = wlr_output_layout_create(server.wl_display);

    // notify when new listeners are available on backend
    wl_list_init(&server.outputs);
    server.new_output.notify = server_new_output;
	wl_signal_add(&server.backend->events.new_output, &server.new_output);

    // fun stuff: scene graph. This handles all damage and rendering tracking.
    server.scene = wlr_scene_create();
    server.scene_layout  = wlr_scene_attach_output_layout(server.scene, server.output_layout);

	/* Set up xdg-shell version 3. The xdg-shell is a Wayland protocol which is
	 * used for application windows. For more detail on shells, refer to
	 * https://drewdevault.com/2018/07/29/Wayland-shells.html.
	 */
    wl_list_init(&server.toplevels);
    server.xdg_shell = wlr_xdg_shell_create(server.wl_display, 3);
    server.new_xdg_toplevel.notify = server_new_xdg_toplevel;
    wl_signal_add(&server.xdg_shell->events.new_toplevel, &server.new_xdg_toplevel);
	server.new_xdg_popup.notify = server_new_xdg_popup;
    wl_signal_add(&server.xdg_shell->events.new_popup, &server.new_xdg_popup);

    // create a cursor (the image)
    server.cursor = wlr_cursor_create();
    wlr_cursor_attach_output_layout(server.cursor, server.output_layout);

    server.cursor_mgr = wlr_xcursor_manager_create(NULL, 24);

    // TODO: https://drewdevault.com/2018/07/17/Input-handling-in-wlroots.html
    
    wl_list_init(&server.keyboards);
    server.new_input.notify = server_new_input;
    wl_signal_add(&server.backend->events.new_input, &server.new_input);
    server.seat = wlr_seat_create(server.wl_display, "seat0");

    // TODO: do smth

    const char *socket = wl_display_add_socket_auto(server.wl_display);
    if (!socket) {
        wlr_backend_destroy(server.backend);
        return 1;
    }

    // start the backend 
    if (!wlr_backend_start(server.backend)) {
        wlr_backend_destroy(server.backend);
        wl_display_destroy(server.wl_display);
        return 1;
    }

    // setup WAYLAND_DISPLAY env var
    setenv("WAYLAND_DISPLAY", socket, true);

    // Finally, run the wayland event loop.
    wlr_log(WLR_INFO, "Running Wayland compositor on WAYLAND_DISPLAY=%s", socket);
    wl_display_run(server.wl_display);

    // once the loop (wl_display_run) exists, we can gracefully exit
    wl_display_destroy_clients(server.wl_display);

    wl_list_remove(&server.new_xdg_toplevel.link);
    wl_list_remove(&server.new_xdg_popup.link);

    wl_list_remove(&server.new_input.link);

    wl_list_remove(&server.new_output.link);

    wlr_scene_node_destroy(&server.scene->tree.node);
    wlr_xcursor_manager_destroy(server.cursor_mgr);
    wlr_cursor_destroy(server.cursor);
    wlr_allocator_destroy(server.allocator);
    wlr_renderer_destroy(server.renderer);
    wlr_backend_destroy(server.backend);
    wl_display_destroy(server.wl_display);

    return 0;
}
