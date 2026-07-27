#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <tomlc17.h>
#include <wayland-util.h>
#include <sys/inotify.h>
#include <wlr/util/log.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/backend/wayland.h>

#include "macro-utils.h"
#include "input.h"
#include "output.h"
#include "server.h"
#include "tiling.h"
#include "workspace.h"
#include "xdg.h"
#include "config.h"

#define MAX_COMMAND_ARGS 16
#define CHECK_TOML_TYPE(datum, expected_type, name_str) \
    do { \
        if ((datum).type != (expected_type) && (datum).type != TOML_UNKNOWN) { \
            fprintf(stderr, "Error: '%s' must be of type %s.\n", (name_str), toml_type_to_string(expected_type)); \
            return 1; \
        } \
    } while (0)

struct config_watcher {
    int notify_fd;
    struct wl_event_source *source;
};

struct config_watcher *watchers;
size_t watcher_count;
size_t watcher_capacity;

void cleanup_all_watchers() {
    for (size_t i = 0; i < watcher_count; i++) {
        if (watchers[i].source) {
            wl_event_source_remove(watchers[i].source);
        }
        if (watchers[i].notify_fd >= 0) {
            close(watchers[i].notify_fd);
        }
    }
    free(watchers);
    watchers = NULL;
    watcher_count = 0;
    watcher_capacity = 0;
}

static int handle_config_change(int fd, uint32_t mask, void *data) {
    UNUSED(mask);
    struct buzzay_server *server = data;

    char buffer[512];
    ssize_t len = read(fd, buffer, sizeof(buffer));
    if (len < 0) {
        perror("read inotify");
        return 0;
    }

    cleanup_all_watchers();
    handle_config(server->config_file, server);

    return 0;
}

int add_config_watcher(struct buzzay_server *server, const char *path) {
    int notify_fd = inotify_init1(IN_CLOEXEC);
    if (notify_fd < 0) {
        perror("inotify_init");
        return -1;
    }

    int wd = inotify_add_watch(notify_fd, path, IN_MODIFY | IN_DELETE_SELF);
    if (wd < 0) {
        perror("inotify_add_watch");
        close(notify_fd);
        return -1;
    }

    if (watcher_count >= watcher_capacity) {
        size_t new_cap = watcher_capacity == 0 ? 4 : watcher_capacity * 2;
        struct config_watcher *new_watchers = realloc(watchers, new_cap * sizeof(struct config_watcher));
        if (!new_watchers) {
            close(notify_fd);
            return -1;
        }
        watchers = new_watchers;
        watcher_capacity = new_cap;
    }

    struct wl_event_source *source = wl_event_loop_add_fd(
        server->wl_event_loop, 
        notify_fd, 
        WL_EVENT_READABLE, 
        handle_config_change, 
        server
    );

    if (!source) {
        close(notify_fd);
        return -1;
    }

    watchers[watcher_count++] = (struct config_watcher){
        .notify_fd = notify_fd,
        .source = source,
    };

    return 0;
}

static inline const char *toml_type_to_string(int type) {
    switch (type) {
        case TOML_STRING:  return "string";
        case TOML_INT64:   return "integer";
        case TOML_FP64:    return "float";
        case TOML_BOOLEAN: return "boolean";
        case TOML_TABLE:   return "table";
        case TOML_ARRAY:   return "array";
        default:           return "unknown";
    }
}

static bool not_unknown(toml_datum_t dat) {
    if (dat.type != TOML_UNKNOWN) return true;
    return false;
}

struct keybinding_cmd {
    const char *commands[MAX_COMMAND_ARGS];
    int command_count;
};

int handle_config_only_envs(const char *path, struct buzzay_server *server) {
    toml_result_t result = toml_parse_file_ex(path);
    if (!result.ok) {
        printf("Failed to parse toml: %s\n", result.errmsg);
        return 1;
    }

    // Apply env variables
    toml_datum_t xcursor_theme = toml_seek(result.toptab, "env.xcursor-theme");
    if (xcursor_theme.type == TOML_STRING) {
        server->xcursor_theme = strdup(xcursor_theme.u.s);
    }
    toml_datum_t xcursor_size = toml_seek(result.toptab, "env.xcursor-size");
    if (xcursor_size.type == TOML_INT64) {
        server->xcursor_size = xcursor_size.u.int64;
    }

    toml_free(result);
    return 0;
}

static void parse_color(const char *color, float out_color[4]) {
    out_color[0] = 0.0f;
    out_color[1] = 0.0f;
    out_color[2] = 0.0f;
    out_color[3] = 1.0f;

    if (!color) {
        return;
    }

    // Skip leading '#' if found
    if (color[0] == '#') {
        color++;
    }

    size_t len = strlen(color);

    if (len == 6) {
        // handling #RRGGBB format
        unsigned int r = 0, g = 0, b = 0;
        if (sscanf(color, "%2x%2x%2x", &r, &g, &b) == 3) {
            out_color[0] = (float)r / 255.0f;
            out_color[1] = (float)g / 255.0f;
            out_color[2] = (float)b / 255.0f;
        }
    } else if (len == 3) {
        // handling #RGB format
        unsigned int r = 0, g = 0, b = 0;
        if (sscanf(color, "%1x%1x%1x", &r, &g, &b) == 3) {
            out_color[0] = (float)((r << 4) | r) / 255.0f;
            out_color[1] = (float)((g << 4) | g) / 255.0f;
            out_color[2] = (float)((b << 4) | b) / 255.0f;
        }
    }
}

static int parse_keybinding_string(
    const char *key_str, 
    xkb_keysym_t *out_sym, 
    enum wlr_keyboard_modifier *out_mods,
    bool is_nested
) {
    *out_sym = XKB_KEY_NoSymbol;

    char *dup = strdup(key_str);
    if (!dup) return -1;

    char *token = strtok(dup, "+");
    char *last_token = NULL;

    while (token != NULL) {
        last_token = token;
        token = strtok(NULL, "+");
        
        if (token != NULL) {
            if (strcmp(last_token, "ADPT") == 0) {
                if (is_nested) {
                    *out_mods |= WLR_MODIFIER_ALT;
                } else {
                    *out_mods |= WLR_MODIFIER_LOGO;
                }
            } else if (strcasecmp(last_token, "Super") == 0 || strcasecmp(last_token, "Mod4") == 0) {
                *out_mods |= WLR_MODIFIER_LOGO;
            } else if (strcasecmp(last_token, "Ctrl") == 0 || strcasecmp(last_token, "Control") == 0) {
                *out_mods |= WLR_MODIFIER_CTRL;
            } else if (strcasecmp(last_token, "Shift") == 0) {
                *out_mods |= WLR_MODIFIER_SHIFT;
            } else if (strcasecmp(last_token, "Alt") == 0 || strcasecmp(last_token, "Mod1") == 0) {
                *out_mods |= WLR_MODIFIER_ALT;
            }
        }
    }

    if (last_token != NULL) {
        *out_sym = xkb_keysym_from_name(last_token, XKB_KEYSYM_CASE_INSENSITIVE);
    }

    free(dup);

    if (*out_sym == XKB_KEY_NoSymbol) {
        return -1;
    }
    return 0;
}

static void spawn_command(const char *cmd) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork failed");
        return;
    }

    if (pid == 0) {
        setsid();

        // redirect stdout and stderr of child
        if (freopen("/dev/null", "w", stdout) == NULL ||
            freopen("/dev/null", "w", stderr) == NULL) {
            _exit(1);
        }

        execl("/bin/sh", "sh", "-c", cmd, NULL);
        _exit(1);
    }
}

static void keybinding_handler(struct buzzay_server *server, void *data) {
    struct keybinding_cmd *kb = data;
    const char *act = kb->commands[0];

    if (strcmp(act, "spawn") == 0) {
        if (kb->command_count < 2) {
            printf("Command count must exactly be 2 in spawn action.\n");
            return;
        }

        spawn_command(kb->commands[1]);
    } else if (strcmp(act, "close-active-window") == 0) {
        struct buzzay_workspace *workspace = get_workspace_at_index(&server->workspaces, server->current_workspace);
        struct buzzay_toplevel *toplevel = workspace->focused_window;

        if (toplevel == NULL || toplevel->xdg_toplevel == NULL) return;
        wlr_xdg_toplevel_send_close(toplevel->xdg_toplevel);
    } else if (strcmp(act, "switch-workspace") == 0) {
        if (kb->command_count < 2) {
            printf("Command count must exactly be 2 in switch-workspace.\n");
            return;
        }
        
        struct buzzay_workspace *this_workspace = get_workspace_at_index(&server->workspaces, server->current_workspace);
        server->current_workspace = atoi(kb->commands[1]);
        
        struct buzzay_toplevel *this_toplevel;
        wl_list_for_each(this_toplevel, &this_workspace->toplevels, link) {
            wlr_scene_node_set_enabled(&this_toplevel->scene_tree->node, false);
        }

        arrange_workspaces(server);
    } else if (strcmp(act, "toggle-monocle") == 0) {
        if (server->window_layout_mode != BZ_LAYOUT_MONOCLE) {
            server->window_layout_mode = BZ_LAYOUT_MONOCLE;
        } else {
            server->window_layout_mode = BZ_LAYOUT_TILE;
        }
        arrange_workspaces(server);
    } else if (strcmp(act, "cycle-monocle") == 0) {
        focus_next_monocle(server, false);
    } else if (strcmp(act, "cycle-monocle-reverse") == 0) {
        focus_next_monocle(server, true);
    } else if (strcmp(act, "quit-compositor") == 0) {
        wl_display_terminate(server->wl_display);
    }
}

int handle_config(const char *path, struct buzzay_server *server) {
    toml_result_t result = toml_parse_file_ex(path);
    if (!result.ok) {
        printf("Failed to parse toml: %s\n", result.errmsg);
        return 1;
    }

    // First setup watcher
    add_config_watcher(server, path);

    // Hanlde core
    toml_datum_t core_focuson = toml_seek(result.toptab, "core.focus-on");
    toml_datum_t core_xdg_interactive = toml_seek(result.toptab, "core.xdg-interactive");
    toml_datum_t core_layout_mode = toml_seek(result.toptab, "core.layout-mode");
    toml_datum_t core_prefer_csd = toml_seek(result.toptab, "core.prever-csd");
    toml_datum_t core_spawn = toml_seek(result.toptab, "core.spawn");
    toml_datum_t core_include = toml_seek(result.toptab, "core.include");

    CHECK_TOML_TYPE(core_focuson, TOML_STRING, "core.focus-on");
    CHECK_TOML_TYPE(core_xdg_interactive, TOML_BOOLEAN, "core.xdg-interactive");
    CHECK_TOML_TYPE(core_layout_mode, TOML_STRING, "core.layout-mode");
    CHECK_TOML_TYPE(core_prefer_csd, TOML_BOOLEAN, "core.prefer-csd");
    CHECK_TOML_TYPE(core_spawn, TOML_ARRAY, "core.spawn");
    CHECK_TOML_TYPE(core_include, TOML_ARRAY, "core.include");

    if (not_unknown(core_xdg_interactive)) 
        server->enable_xdg_interactive = core_xdg_interactive.u.boolean;

    if (not_unknown(core_focuson)) {
        if (strcmp(core_focuson.u.s, "click") == 0) {
            server->window_active_on = WINDOW_ACTIVE_ON_CLICK;
        } else if (strcmp(core_focuson.u.s, "hover") == 0) {
            server->window_active_on = WINDOW_ACTIVE_ON_HOVER;
        } else {
            printf("Unknown mode found in 'focus-on'.\n");
            return 1;
        }
    }

    if (not_unknown(core_layout_mode)) {
        if (strcmp(core_layout_mode.u.s, "tiling") == 0) {
            server->window_layout_mode = BZ_LAYOUT_TILE;
        } else if (strcmp(core_layout_mode.u.s, "monocle") == 0) {
            server->window_layout_mode = BZ_LAYOUT_MONOCLE;
        } else {
            printf("Unknown mode found in 'layout-mode'.\n");
            return 1;
        }
    }

    if (not_unknown(core_prefer_csd)) {
        if (core_prefer_csd.u.boolean) {
            server->decoration_mode = WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE;
        } else {
            server->decoration_mode = WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE;
        }
    }

    if (not_unknown(core_spawn) && server->server_first_load) {
        for (int i = 0; i < core_spawn.u.arr.size; i++) {
            toml_datum_t item = core_spawn.u.arr.elem[i];
            if (item.type != TOML_STRING) continue;
            spawn_command(item.u.s);
        }
    }

    if (not_unknown(core_include)) {
        for (int i = 0; i < core_include.u.arr.size; i++) {
            toml_datum_t item = core_include.u.arr.elem[i];
            if (item.type != TOML_STRING) continue;
            handle_config(item.u.s, server);
        }
    }

    // Apply monitors
    toml_datum_t monitors = toml_seek(result.toptab, "monitor");
    CHECK_TOML_TYPE(monitors, TOML_ARRAY, "monitor");

    for (int i = 0; i < monitors.u.arr.size; i++) {
        toml_datum_t item = monitors.u.arr.elem[i];

        if (item.type != TOML_TABLE) {
            continue;
        }

        toml_datum_t name = toml_seek(item, "id");
        toml_datum_t enabled = toml_seek(item, "enabled");
        toml_datum_t scale = toml_seek(item, "scale");
        toml_datum_t position = toml_seek(item, "position");

        CHECK_TOML_TYPE(name, TOML_STRING, "id");
        CHECK_TOML_TYPE(enabled, TOML_BOOLEAN, "enabled");
        CHECK_TOML_TYPE(scale, TOML_FP64, "scale");
        CHECK_TOML_TYPE(position, TOML_ARRAY, "position");

        struct buzzay_output *output;
        wl_list_for_each(output, &server->outputs, link) {
            if (strcmp(output->wlr_output->name, name.u.s) == 0) {
                if (not_unknown(enabled)) output->wlr_output->enabled = enabled.u.boolean;
                if (not_unknown(scale)) output->wlr_output->scale = scale.u.fp64;

                if (not_unknown(position)) {
                    if (position.u.arr.size <= 2) {
                        printf("Monitor position must receive two elements: '[x, y]'\n");
                        break;
                    }
                    toml_datum_t x = position.u.arr.elem[0];
                    toml_datum_t y = position.u.arr.elem[1];

                    if (x.type != TOML_INT64 || y.type != TOML_INT64) {
                        printf("Both x and y coords of monitor position must be an integer.\n");
                        break;
                    }

                    wlr_output_layout_add(server->output_layout, output->wlr_output, x.u.int64, y.u.int64);
                }
                
                break;
            }
        }
    }

    // Handle eyecandy
    toml_datum_t candy_gap = toml_seek(result.toptab, "candy.gap");
    toml_datum_t candy_opacity = toml_seek(result.toptab, "candy.opacity");
    CHECK_TOML_TYPE(candy_gap, TOML_INT64, "gap");
    CHECK_TOML_TYPE(candy_opacity, TOML_FP64, "opacity");

    if (not_unknown(candy_gap)) {
        server->eyecandies.gap = candy_gap.u.int64;
    }
    if (not_unknown(candy_opacity)) server->eyecandies.window_opacity = candy_opacity.u.fp64;

    // Handle border
    toml_datum_t active_clr = toml_seek(result.toptab, "candy.border.active");
    toml_datum_t inactive_clr = toml_seek(result.toptab, "candy.border.inactive");
    toml_datum_t bdr_thickness = toml_seek(result.toptab, "candy.border.thickness");
    CHECK_TOML_TYPE(active_clr, TOML_STRING, "active");
    CHECK_TOML_TYPE(inactive_clr, TOML_STRING, "inactive");
    CHECK_TOML_TYPE(bdr_thickness, TOML_INT64, "thickness");

    if (not_unknown(active_clr)) {
        parse_color(active_clr.u.s, server->eyecandies.active_border);
        update_border_colors(server);
    }
    if (not_unknown(inactive_clr)) {
        parse_color(inactive_clr.u.s, server->eyecandies.inactive_border);
        update_border_colors(server);
    }
    if (not_unknown(bdr_thickness)) {
        server->eyecandies.border_thickness = bdr_thickness.u.int64;
    }

    // Handle blur
    toml_datum_t blur_enabled = toml_seek(result.toptab, "candy.blur.enabled");
    toml_datum_t blur_strength = toml_seek(result.toptab, "candy.blur.strength");
    toml_datum_t blur_alpha = toml_seek(result.toptab, "candy.blur.alpha");
    CHECK_TOML_TYPE(blur_enabled, TOML_BOOLEAN, "enabled");
    CHECK_TOML_TYPE(blur_strength, TOML_FP64, "strength");
    CHECK_TOML_TYPE(blur_alpha, TOML_FP64, "alpha");

    if (not_unknown(blur_enabled)) server->eyecandies.blur_enabled = blur_enabled.u.boolean;
    if (not_unknown(blur_strength)) server->eyecandies.blur_strength = blur_strength.u.fp64;
    if (not_unknown(blur_alpha)) server->eyecandies.blur_alpha = blur_alpha.u.fp64;

    // Handle inputs
    toml_datum_t input_keyboard_layout = toml_seek(result.toptab, "input.keyboard.layout");
    toml_datum_t input_keyboard_variant = toml_seek(result.toptab, "input.keyboard.variant");
    toml_datum_t input_keyboard_options = toml_seek(result.toptab, "input.keyboard.options");
    CHECK_TOML_TYPE(input_keyboard_layout, TOML_STRING, "input.keyboard.layout");
    CHECK_TOML_TYPE(input_keyboard_variant, TOML_STRING, "input.keyboard.variant");
    CHECK_TOML_TYPE(input_keyboard_options, TOML_STRING, "input.keyboard.options");

    if (not_unknown(input_keyboard_layout)) {
        const char *layout = input_keyboard_layout.u.s;
        const char *variant = not_unknown(input_keyboard_variant) ? input_keyboard_variant.u.s : "";
        const char *options = not_unknown(input_keyboard_options) ? input_keyboard_options.u.s : "";

        struct buzzay_keyboard *device;
        wl_list_for_each(device, &server->keyboards, link) {
            apply_keyboard_config_to_device(device->wlr_keyboard, layout, variant, options);
        }
    }

    // Handle keybindings
    toml_datum_t bindings = toml_seek(result.toptab, "bindings");
    CHECK_TOML_TYPE(bindings, TOML_TABLE, "bindings");

    for (int i = 0; i < bindings.u.tab.size; i++) {
        const char *key = bindings.u.tab.key[i];
        toml_datum_t val = bindings.u.tab.value[i];

        if (val.type != TOML_ARRAY) {
            fprintf(stderr, "Error: Binding for '%s' must be an array of commands.\n", key);
            continue;
        }

        int array_size = val.u.arr.size;
        if (array_size <= 0) {
            fprintf(stderr, "Error: Binding for '%s' has an empty command array.\n", key);
            continue;
        }

        struct keybinding_cmd *kb_cmd = calloc(1, sizeof(struct keybinding_cmd));
        kb_cmd->command_count = 0;
        
        for (int j = 0; j < array_size && kb_cmd->command_count < 16; j++) {
            toml_datum_t elem = val.u.arr.elem[j];
            if (elem.type != TOML_STRING) {
                fprintf(stderr, "Error: Non-string element found in command array for '%s'.\n", key);
                break;
            }
            kb_cmd->commands[kb_cmd->command_count++] = strdup(elem.u.s);
        }

        if (kb_cmd->command_count < 1) {
            fprintf(stderr, "Error: commands should have at least 1 command.");
            free(kb_cmd);
            continue;
        }

        struct keybinding binding = {0};
        bool is_nested = false;

        if (!wl_list_empty(&server->outputs)) {
            struct buzzay_output *first_item = wl_container_of(
                server->outputs.next, first_item, link
            );

            is_nested = wlr_output_is_wl(first_item->wlr_output);
        }

        if (parse_keybinding_string(
                    key, &binding.sym, 
                    &binding.modifiers, 
                    is_nested) != 0) {
            fprintf(stderr, "Error: Failed to parse keybinding string '%s'\n", key);
            free(kb_cmd);
            continue;
        }
        binding.handler = keybinding_handler;
        binding.data = kb_cmd;
        register_keybinding(binding);
    }

    arrange_workspaces(server);

    toml_free(result);
    return 0;
}
