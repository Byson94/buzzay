#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <tomlc17.h>
#include <wayland-util.h>
#include <wlr/util/log.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>

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
    toml_datum_t xcursor_theme = toml_seek(result.toptab, "env.xcursor_theme");
    if (xcursor_theme.type == TOML_STRING) {
        server->xcursor_theme = xcursor_theme.u.s;
    }
    toml_datum_t xcursor_size = toml_seek(result.toptab, "env.xcursor_size");
    if (xcursor_size.type == TOML_INT64) {
        server->xcursor_size = xcursor_size.u.int64;
    }

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

static int parse_keybinding_string(const char *key_str, xkb_keysym_t *out_sym, enum wlr_keyboard_modifier *out_mods) {
    *out_sym = XKB_KEY_NoSymbol;

    char *dup = strdup(key_str);
    if (!dup) return -1;

    char *token = strtok(dup, "+");
    char *last_token = NULL;

    while (token != NULL) {
        last_token = token;
        token = strtok(NULL, "+");
        
        if (token != NULL) {
            if (strcasecmp(last_token, "Super") == 0 || strcasecmp(last_token, "Mod4") == 0) {
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
        focus_next_monocle(server);
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

    // Hanlde core
    toml_datum_t core_conf = toml_seek(result.toptab, "core");
    CHECK_TOML_TYPE(core_conf, TOML_TABLE, "core");

    toml_datum_t core_focuson = toml_seek(core_conf, "focus-on");
    toml_datum_t core_xdg_interactive = toml_seek(core_conf, "xdg-interactive");
    toml_datum_t core_layout_mode = toml_seek(core_conf, "layout-mode");
    toml_datum_t core_spawn = toml_seek(core_conf, "spawn");

    CHECK_TOML_TYPE(core_focuson, TOML_STRING, "focus-on");
    CHECK_TOML_TYPE(core_xdg_interactive, TOML_BOOLEAN, "xdg-interactive");
    CHECK_TOML_TYPE(core_layout_mode, TOML_STRING, "layout-mode");
    CHECK_TOML_TYPE(core_spawn, TOML_ARRAY, "spawn");

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

    for (int i = 0; i < core_spawn.u.arr.size; i++) {
        toml_datum_t item = core_spawn.u.arr.elem[i];
        if (item.type != TOML_STRING) continue;
        spawn_command(item.u.s);
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
    toml_datum_t eyecandy = toml_seek(result.toptab, "candy");
    toml_datum_t candy_gap = toml_seek(eyecandy, "gap");
    toml_datum_t candy_opacity = toml_seek(eyecandy, "opacity");
    CHECK_TOML_TYPE(eyecandy, TOML_TABLE, "candy");
    CHECK_TOML_TYPE(candy_gap, TOML_INT64, "gap");
    CHECK_TOML_TYPE(candy_opacity, TOML_FP64, "opacity");

    if (not_unknown(candy_gap)) server->eyecandies.gap = candy_gap.u.int64;
    if (not_unknown(candy_opacity)) server->eyecandies.window_opacity = candy_opacity.u.fp64;

    // Handle border
    toml_datum_t eyecandy_border = toml_seek(result.toptab, "candy.border");
    CHECK_TOML_TYPE(eyecandy_border, TOML_TABLE, "candy.border");
    
    toml_datum_t active_clr = toml_seek(eyecandy_border, "active");
    toml_datum_t inactive_clr = toml_seek(eyecandy_border, "inactive");
    toml_datum_t bdr_thickness = toml_seek(eyecandy_border, "thickness");
    CHECK_TOML_TYPE(active_clr, TOML_STRING, "active");
    CHECK_TOML_TYPE(inactive_clr, TOML_STRING, "inactive");
    CHECK_TOML_TYPE(bdr_thickness, TOML_INT64, "thickness");

    if (not_unknown(active_clr)) parse_color(active_clr.u.s, server->eyecandies.active_border);
    if (not_unknown(inactive_clr)) parse_color(inactive_clr.u.s, server->eyecandies.inactive_border);
    if (not_unknown(eyecandy_border)) server->eyecandies.border_thickness = bdr_thickness.u.int64;

    // Handle blur
    toml_datum_t eyecandy_blur = toml_seek(result.toptab, "eyecandy.blur");
    toml_datum_t blur_enabled = toml_seek(eyecandy_blur, "enabled");
    toml_datum_t blur_strength = toml_seek(eyecandy_blur, "strength");
    toml_datum_t blur_alpha = toml_seek(eyecandy_blur, "alpha");
    CHECK_TOML_TYPE(eyecandy_blur, TOML_TABLE, "eyecandy.blur");
    CHECK_TOML_TYPE(blur_enabled, TOML_BOOLEAN, "enabled");
    CHECK_TOML_TYPE(blur_strength, TOML_FP64, "strength");
    CHECK_TOML_TYPE(blur_alpha, TOML_FP64, "alpha");

    if (not_unknown(blur_enabled)) server->eyecandies.blur_enabled = blur_enabled.u.boolean;
    if (not_unknown(blur_strength)) server->eyecandies.blur_strength = blur_strength.u.fp64;
    if (not_unknown(blur_alpha)) server->eyecandies.blur_alpha = blur_alpha.u.fp64;

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
        if (parse_keybinding_string(key, &binding.sym, &binding.modifiers) != 0) {
            fprintf(stderr, "Error: Failed to parse keybinding string '%s'\n", key);
            free(kb_cmd);
            continue;
        }
        binding.handler = keybinding_handler;
        binding.data = kb_cmd;
        register_keybinding(binding);
    }

    toml_free(result);
    return 0;
}
