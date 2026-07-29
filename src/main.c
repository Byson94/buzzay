#include <pwd.h>
#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/inotify.h>
#include <wayland-server-core.h>
#include <wayland-util.h>
#include <wlr/backend.h>
#include <wlr/util/log.h>
#include <wlr/backend/session.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_gamma_control_v1.h>
#include <wlr/types/wlr_cursor_shape_v1.h>
#include <wlr/types/wlr_idle_inhibit_v1.h>
#include <wlr/types/wlr_idle_notify_v1.h>
#include <wlr/types/wlr_viewporter.h>
#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/types/wlr_xdg_output_v1.h>
#include <wlr/types/wlr_ext_data_control_v1.h>
#include <wlr/types/wlr_single_pixel_buffer_v1.h>
#include <wlr/types/wlr_fractional_scale_v1.h>
#include <wlr/types/wlr_export_dmabuf_v1.h>
#include <wlr/render/allocator.h>
#include <scenefx/render/fx_renderer/fx_renderer.h>
#include <scenefx/types/wlr_scene.h>

#include "handle-plugin.h"
#include "layershell.h"
#include "workspace.h"
#include "tiling.h"
#include "input.h"
#include "output.h"
#include "cursor.h"
#include "server.h"
#include "config.h"
#include "gamma.h"
#include "idle.h"
#include "xdg.h"
#include "ipc.h"

const char *program_name = "buzzay";
const char *program_ver = "0.1.0";

void print_help() {
    printf("%s - An extensible wayland compositor.\n\n", program_name);
    printf("Usage:\n");
    printf("  %s\n", program_name);
    printf("  %s plugin <subcommand>\n", program_name);
    printf("  %s ipc <subcommand>\n", program_name);
    printf("  %s -h, --help\n", program_name);
    printf("  %s -v, --version\n\n", program_name);
    printf("Commands:\n");
    printf("  plugin   Plugin related commands\n");
    printf("  ipc      Ipc related commands\n");
    printf("Options:\n");
    printf("  -h, --help     Show this help message and exit\n");
    printf("  -v, --version  Print the program version\n\n");
    printf("Tip: Run '<command> --help' to get help for each command.\n");
}

void print_help_plugin() {
    printf("Usage:\n");
    printf("  %s plugin load <plugin_name>\n", program_name);
    printf("  %s plugin msg <plugin_name> <messages...>\n\n", program_name);
    printf("Commands:\n");
    printf("  plugin load    Load a plugin\n");
    printf("  plugin msg     Send a message to a plugin\n\n");
    printf("Additional Info: Messages can be send to a plugin in the '<plugin_name> <arg1> <arg2> ...' format.\n");
    printf("Example: '%s plugin msg myplugin perform greeting'\n", program_name);
}

static int start_compositor() {
    // setup compositor
    wlr_log_init(WLR_INFO, NULL);

    struct buzzay_server server = {0};

    // Resolve configuration path
    char conf_file_path[PATH_MAX];
    const char *conf_home = getenv("XDG_CONFIG_HOME");
    const char *conf_file = getenv("BUZZAY_CONF");

    if (conf_file != NULL) {
        strcpy(conf_file_path, conf_file);
    } else if (conf_home != NULL) {
        snprintf(conf_file_path, sizeof(conf_file_path), "%s/buzzay/config.toml", conf_home);
    } else {
        char *homedir = getpwuid(getuid())->pw_dir;
        snprintf(conf_file_path, sizeof(conf_file_path), "%s/.config/buzzay/config.toml", homedir);
    }

    // Setup Configs
    server.config_file = conf_file_path;
    server.server_first_load = true;
    server.enable_xdg_interactive = true;
    server.window_active_on = WINDOW_ACTIVE_ON_CLICK;
    server.window_layout_mode = BZ_LAYOUT_TILE;

    server.xcursor_theme = getenv("XCURSOR_THEME");
    server.xcursor_size = 24;

    const char* xcursor_size_str = getenv("XCURSOR_SIZE");
    if (xcursor_size_str != NULL) {
        server.xcursor_size = atoi(xcursor_size_str);
    }

    struct buzzay_eyecandies default_eyecandy = {
        .gap = 5,
        .active_border = { 0.8f, 0.5f, 0.2f, 1.0f },
        .inactive_border = { 0.2f, 0.2f, 0.25f, 1.0f },
        .border_thickness = 2,
        .corner_radius = 10,
        .window_opacity = 1,
        .blur_strength = 1,
        .blur_alpha = 1,
        .blur_passes = 3,
        .blur_noise = 0
    };
    server.eyecandies = default_eyecandy;

    // setup the cursor, envs & server plugin
    handle_config_only_cursor(conf_file_path, &server);
    handle_config_only_envs(conf_file_path);

    // - managed by libwayland. 
    // - manages many stuff.
    server.wl_display = wl_display_create();
    server.wl_event_loop = wl_display_get_event_loop(server.wl_display);

    // abstraction of i/o
    server.backend = wlr_backend_autocreate(server.wl_event_loop, &server.session);
    if (server.backend == NULL) {
        wlr_log(WLR_ERROR, "failed to create wlr_backend");
        return 1;
    }

    // Create a renderer.
    // WLR_RENDERER env var can be set to specify one.
	server.renderer = fx_renderer_create(server.backend);
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
    server.output_layout_change.notify = server_output_layout_changed;
    wl_signal_add(&server.output_layout->events.change, &server.output_layout_change);

    // notify when new listeners are available on backend
    wl_list_init(&server.outputs);
    server.new_output.notify = server_new_output;
	wl_signal_add(&server.backend->events.new_output, &server.new_output);

    // fun stuff: scene graph. This handles all damage and rendering tracking.
    server.scene = wlr_scene_create();
    server.layers.background = wlr_scene_tree_create(&server.scene->tree);
    server.layers.bottom = wlr_scene_tree_create(&server.scene->tree);
    server.layers.blur = wlr_scene_optimized_blur_create(&server.scene->tree, 0, 0);
    server.layers.workspace = wlr_scene_tree_create(&server.scene->tree);
    server.layers.top = wlr_scene_tree_create(&server.scene->tree);
    server.layers.overlay = wlr_scene_tree_create(&server.scene->tree);
    server.scene_layout  = wlr_scene_attach_output_layout(server.scene, server.output_layout);

	/* Set up xdg-shell version 7. The xdg-shell is a Wayland protocol which is
	 * used for application windows. For more detail on shells, refer to
	 * https://drewdevault.com/2018/07/29/Wayland-shells.html.
	 */
    wl_list_init(&server.workspaces);
    for (int i = 0; i < 10; i++) {
        struct buzzay_workspace *ws = calloc(1, sizeof(*ws));
        ws->id = i + 1;
        wl_list_init(&ws->toplevels);
        workspace_init(ws);
        wl_list_insert(server.workspaces.prev, &ws->link);
    }
    server.current_workspace = 1;
    server.xdg_shell = wlr_xdg_shell_create(server.wl_display, 7);
    server.new_xdg_toplevel.notify = server_new_xdg_toplevel;
    wl_signal_add(&server.xdg_shell->events.new_toplevel, &server.new_xdg_toplevel);
	server.new_xdg_popup.notify = server_new_xdg_popup;
    wl_signal_add(&server.xdg_shell->events.new_popup, &server.new_xdg_popup);

    // Setup the xdg-decorations for decoration support
    server.xdg_decoration = wlr_xdg_decoration_manager_v1_create(server.wl_display);
    server.decoration_mode = WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE;
    server.new_toplevel_decoration.notify = server_new_toplevel_decoration;
    wl_signal_add(&server.xdg_decoration->events.new_toplevel_decoration, &server.new_toplevel_decoration);

    // Setup viewporter
    wlr_viewporter_create(server.wl_display);
    
    // Setup layer shell
    server.layer_shell = wlr_layer_shell_v1_create(server.wl_display, 5);
    server.new_layer_surface.notify = server_new_layer_surface;
    wl_signal_add(&server.layer_shell->events.new_surface, &server.new_layer_surface);

    // Setup gamma protocol
    server.gamma_mgr = wlr_gamma_control_manager_v1_create(server.wl_display);
    server.set_gamma.notify = server_new_set_gamma;
    wl_signal_add(&server.gamma_mgr->events.set_gamma, &server.set_gamma);

    // setup idle inhibit and notifier
    server.idle_inhibit_count = 0;
    server.idle_inhibit_mgr = wlr_idle_inhibit_v1_create(server.wl_display);
    server.idle_new_inhibitor.notify = server_new_idle_new_inhibitor;
    wl_signal_add(&server.idle_inhibit_mgr->events.new_inhibitor, &server.idle_new_inhibitor);

    server.idle_notifier = wlr_idle_notifier_v1_create(server.wl_display);

    // setup screencopy and xdg output manager protocol
    wlr_screencopy_manager_v1_create(server.wl_display);
    wlr_xdg_output_manager_v1_create(server.wl_display, server.output_layout);

    // setup other protocols
    wlr_ext_data_control_manager_v1_create(server.wl_display, 1);
    wlr_single_pixel_buffer_manager_v1_create(server.wl_display);
    wlr_fractional_scale_manager_v1_create(server.wl_display, 1);
    wlr_export_dmabuf_manager_v1_create(server.wl_display);

    // create a cursor (the image)
    server.cursor = wlr_cursor_create();
    wlr_cursor_attach_output_layout(server.cursor, server.output_layout);

    server.cursor_mgr = wlr_xcursor_manager_create(server.xcursor_theme, server.xcursor_size);
    if (!wlr_xcursor_manager_load(server.cursor_mgr, 1.0f)) {
        wlr_log(WLR_ERROR, "Failed to load XCursor theme");
    }
    wlr_log(WLR_INFO, "Theme: %s, Size: %d", server.xcursor_theme ? server.xcursor_theme : "default", server.xcursor_size);
    if (server.cursor_mgr) {
        bool loaded = wlr_xcursor_manager_load(server.cursor_mgr, 1.0f);
        wlr_log(WLR_INFO, "Manager loaded: %s", loaded ? "YES" : "NO");
    }
    wlr_cursor_set_xcursor(server.cursor, server.cursor_mgr, "default");

    // setup cursor shape protocol 
    server.cursor_shape_mgr = wlr_cursor_shape_manager_v1_create(server.wl_display, 2);
    server.cursor_request_set_shape.notify = server_new_request_cursor_set_shape;
    wl_signal_add(&server.cursor_shape_mgr->events.request_set_shape, &server.cursor_request_set_shape);

    // track cursor movement
    server.cursor_mode = BUZZAY_CURSOR_PASSTHROUGH;
    server.cursor_motion.notify = server_cursor_motion;
    wl_signal_add(&server.cursor->events.motion, &server.cursor_motion);
    server.cursor_motion_absolute.notify = server_cursor_motion_absolute;
    wl_signal_add(&server.cursor->events.motion_absolute, &server.cursor_motion_absolute);
	server.cursor_button.notify = server_cursor_button;
	wl_signal_add(&server.cursor->events.button, &server.cursor_button);
	server.cursor_axis.notify = server_cursor_axis;
	wl_signal_add(&server.cursor->events.axis, &server.cursor_axis);
	server.cursor_frame.notify = server_cursor_frame;
	wl_signal_add(&server.cursor->events.frame, &server.cursor_frame);

    // setup seats
    wl_list_init(&server.keyboards);
    server.new_input.notify = server_new_input;
    wl_signal_add(&server.backend->events.new_input, &server.new_input);
    server.seat = wlr_seat_create(server.wl_display, "seat0");
	server.request_cursor.notify = seat_request_cursor;
	wl_signal_add(&server.seat->events.request_set_cursor,
			&server.request_cursor);
	server.pointer_focus_change.notify = seat_pointer_focus_change;
	wl_signal_add(&server.seat->pointer_state.events.focus_change,
			&server.pointer_focus_change);
	server.request_set_selection.notify = seat_request_set_selection;
	wl_signal_add(&server.seat->events.request_set_selection,
			&server.request_set_selection);

    const char *wayland_socket = wl_display_add_socket_auto(server.wl_display);
    if (!wayland_socket) {
        wlr_backend_destroy(server.backend);
        return 1;
    }

    // start the backend 
    if (!wlr_backend_start(server.backend)) {
        wlr_backend_destroy(server.backend);
        wl_display_destroy(server.wl_display);
        return 1;
    }

    // startup IPC
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    strncpy(addr.sun_path, ipc_socket_file, sizeof(addr.sun_path) - 1);

    unlink(addr.sun_path);
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        wlr_log(WLR_ERROR, "Failed to bind IPC socket: is another instance running?");
        close(fd);
        return 1;
    }
    listen(fd, 5);

    wl_event_loop_add_fd(server.wl_event_loop, fd, WL_EVENT_READABLE, handle_ipc_connection, (void *)&server);

    // setup env vars
    setenv("WAYLAND_DISPLAY", wayland_socket, true);
    setenv("XCURSOR_THEME", server.xcursor_theme, true);

    int s = (int)((ceil(log10(server.xcursor_size))+1)*sizeof(char));
    char cur_size[s];
    sprintf(cur_size, "%d", server.xcursor_size);

    setenv("XCURSOR_SIZE", cur_size, true);

    // Parse config and setup watcher
    handle_config(conf_file_path, &server);
    server.server_first_load = false;

    // Finally, run the wayland event loop.
    wlr_log(WLR_INFO, "Running Wayland compositor on WAYLAND_DISPLAY=%s", wayland_socket);
    wl_display_run(server.wl_display);

    // once the loop (wl_display_run) exists, we can gracefully exit
    close(fd);
    unlink("/tmp/buzzay.sock");

    wl_display_destroy_clients(server.wl_display);

    wl_list_remove(&server.new_xdg_toplevel.link);
    wl_list_remove(&server.new_xdg_popup.link);
    wl_list_remove(&server.new_toplevel_decoration.link);
    wl_list_remove(&server.output_layout_change.link);
    wl_list_remove(&server.pointer_focus_change.link);
    wl_list_remove(&server.request_set_selection.link);

	wl_list_remove(&server.cursor_motion.link);
	wl_list_remove(&server.cursor_motion_absolute.link);
	wl_list_remove(&server.cursor_button.link);
	wl_list_remove(&server.cursor_axis.link);
	wl_list_remove(&server.cursor_frame.link);
    wl_list_remove(&server.cursor_request_set_shape.link);
    wl_list_remove(&server.request_cursor.link);

    wl_list_remove(&server.new_input.link);
    wl_list_remove(&server.new_output.link);

    wl_list_remove(&server.new_layer_surface.link);
    wl_list_remove(&server.set_gamma.link);
    wl_list_remove(&server.idle_new_inhibitor.link);

    wlr_scene_node_destroy(&server.scene->tree.node);
    wlr_xcursor_manager_destroy(server.cursor_mgr);
    wlr_cursor_destroy(server.cursor);
    wlr_allocator_destroy(server.allocator);
    wlr_renderer_destroy(server.renderer);
    wlr_backend_destroy(server.backend);
    wl_display_destroy(server.wl_display);

    free(plugin_array);
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        return start_compositor();
    }

    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        print_help();
        return 0;
    }

    if (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0) {
        printf("%s v%s\n", program_name, program_ver);
        return 0;
    }

    // Handle plugins
    if (strcmp(argv[1], "plugin") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: 'plugin' subcommand requires an action (load, msg).\n");
            return 1;
        }

        // Action: print help
        if (strcmp(argv[2], "--help") == 0) {
            print_help_plugin();
            return 0;
        }

        // Action: plugin load <name>
        if (strcmp(argv[2], "load") == 0) {
            if (argc != 4) {
                printf("'plugin load' accepts only one argument.\n");
                return 1;
            }

            const char *plugin = argv[3];
            if (strlen(plugin) > 100) {
                printf("Plugin name must not exceed 100 characters.\n");
                return 1;
            }

            char msg[100 + 5] = "load ";
            strcat(msg, plugin);
            return ipc_send_msg(msg);
        }
        
        // Action: plugin msg <messages...>
        else if (strcmp(argv[2], "msg") == 0) {
            int num_args = argc - 3;
            if (num_args <= 0) {
                printf("Error: 'plugin msg' requires at least one message argument.\n");
                return 1;
            }
            if (num_args >= 100) {
                printf("Only a maximum of 100 arguments can be passed.\n");
                return 1;
            }

            char msg[4096] = "msg ";
            for (int i = 3; i < argc; i++) {
                if (strlen(msg) + strlen(argv[i]) + 1 >= sizeof(msg)) {
                    printf("Error: Combined message string too long.\n");
                    return 1;
                }
                strcat(msg, argv[i]);
                strcat(msg, " ");
            }

            return ipc_send_msg(msg);
        } else {
            fprintf(stderr, "Error: Unknown plugin action '%s'.\n", argv[2]);
            return 1;
        }
    }

    fprintf(stderr, "Error: Unknown command '%s'. Use -h for help.\n", argv[1]);
    return 0;
}
