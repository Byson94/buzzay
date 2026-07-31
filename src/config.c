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
#include <scenefx/types/wlr_scene.h>
#include <wlr/backend/wayland.h>

#include "macro-utils.h"
#include "input.h"
#include "output.h"
#include "server.h"
#include "tiling.h"
#include "wlr/types/wlr_xdg_decoration_v1.h"
#include "workspace.h"
#include "xdg.h"
#include "config.h"
#include "error.h"

#define MAX_COMMAND_ARGS 16
#define CHECK_TOML_TYPE(datum, expected_type, name_str) \
    do { \
        if ((datum).type != (expected_type) && (datum).type != TOML_UNKNOWN) { \
            const char *type_str = toml_type_to_string(expected_type); \
            int len = snprintf(NULL, 0, "Error: '%s' must be of type %s.\n", (name_str), type_str); \
            if (len > 0) { \
                char *msg = (char *)malloc((size_t)len + 1); \
                if (msg != NULL) { \
                    snprintf(msg, (size_t)len + 1, "Error: '%s' must be of type %s.\n", (name_str), type_str); \
                    fputs(msg, stderr); \
                    create_error_bar(500, msg); \
                    free(msg); \
                } \
            } \
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

bool is_path_watched(const char *path) {
    UNUSED(path);
    // Implement check against active watchers array to prevent duplicate watchers
    return false; 
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
    clear_all_keybinding();
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
    struct keybinding_config config;
};

void kb_cmd_set_init_config(struct keybinding_cmd *kb_cmd) {
    kb_cmd->config.repeat = true;
}

int handle_config_only_envs(const char *path) {
    toml_result_t result = toml_parse_file_ex(path);
    if (!result.ok) {
        printf("Failed to parse toml: %s\n", result.errmsg);
        return 1;
    }

    toml_datum_t envs = toml_seek(result.toptab, "env");
    CHECK_TOML_TYPE(envs, TOML_TABLE, "env");

    for (int i = 0; i < envs.u.tab.size; i++) {
        const char *key = envs.u.tab.key[i];
        toml_datum_t val = envs.u.tab.value[i];

        if (val.type != TOML_STRING) continue;
        setenv(key, val.u.s, true);
    }

    toml_free(result);
    return 0;
}

int handle_config_only_cursor(const char *path, struct buzzay_server *server) {
    toml_result_t result = toml_parse_file_ex(path);
    if (!result.ok) {
        printf("Failed to parse toml: %s\n", result.errmsg);
        return 1;
    }

    // Apply cursor
    toml_datum_t xcursor_theme = toml_seek(result.toptab, "cursor.theme");
    if (xcursor_theme.type == TOML_STRING) {
        server->xcursor_theme = strdup(xcursor_theme.u.s);
    }
    toml_datum_t xcursor_size = toml_seek(result.toptab, "cursor.size");
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
    struct keybinding *kb,
    bool is_nested
) {
    if (!kb) return -1;

    kb->modifiers = 0;
    kb->is_keycode = false;
    kb->key.sym = XKB_KEY_NoSymbol;

    char *dup = strdup(key_str);
    if (!dup) return -1;

    char *token = strtok(dup, "+");
    char *last_token = NULL;

    while (token != NULL) {
        last_token = token;
        token = strtok(NULL, "+");

        if (token != NULL) {
            if (strcasecmp(last_token, "adpt") == 0) {
                if (is_nested) {
                    kb->modifiers |= WLR_MODIFIER_ALT;
                } else {
                    kb->modifiers |= WLR_MODIFIER_LOGO;
                }
            } else if (strcasecmp(last_token, "super") == 0 || strcasecmp(last_token, "mod4") == 0) {
                kb->modifiers |= WLR_MODIFIER_LOGO;
            } else if (strcasecmp(last_token, "ctrl") == 0 || strcasecmp(last_token, "control") == 0) {
                kb->modifiers |= WLR_MODIFIER_CTRL;
            } else if (strcasecmp(last_token, "shift") == 0) {
                kb->modifiers |= WLR_MODIFIER_SHIFT;
            } else if (strcasecmp(last_token, "alt") == 0 || strcasecmp(last_token, "mod1") == 0) {
                kb->modifiers |= WLR_MODIFIER_ALT;
            }
        }
    }

    if (last_token != NULL) {
        if (strncasecmp(last_token, "code:", 5) == 0) {
            char *endptr = NULL;
            long code = strtol(last_token + 5, &endptr, 10);
            
            if (endptr != (last_token + 5) && *endptr == '\0' && code >= 0) {
                kb->is_keycode = true;
                kb->key.code = (xkb_keycode_t)(code + 8); 
            }
        } else {
            kb->is_keycode = false;
            kb->key.sym = xkb_keysym_from_name(last_token, XKB_KEYSYM_CASE_INSENSITIVE);
        }
    }

    free(dup);

    if (kb->is_keycode) {
        if (kb->key.code == 0) return -1;
    } else {
        if (kb->key.sym == XKB_KEY_NoSymbol) return -1;
    }

    return 0;
}

static void spawn_command(const char *cmd) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork failed");
        return;
    }

    /* 
     * We perform double fork here so that we can orphan the
     * grandchild and leave it to be adopted by the init system.
     * Otherwise, if it were a single fork, we would have to be 
     * responsible for reaping the zombie orphan child.
     */
    if (pid == 0) {
        pid_t grandchild = fork();

        if (grandchild > 0) {
            _exit(0);
        } else if (grandchild == 0) {
            // Orphan that little bastard.
            setsid();

            if (freopen("/dev/null", "w", stdout) == NULL ||
                freopen("/dev/null", "w", stderr) == NULL) {
                _exit(1);
            }

            execl("/bin/sh", "sh", "-c", cmd, NULL);
            _exit(1);
        } else {
            perror("grandchild fork failed");
            _exit(1);
        }
    }

    // Reap the parent
    waitpid(pid, NULL, 0);
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
    } 
    // == Compositor ==
    else if (strcmp(act, "quit-compositor") == 0) {
        struct buzzay_workspace *workspace = NULL;
        wl_list_for_each(workspace, &server->workspaces, link) {
            struct buzzay_toplevel *toplevel = NULL;
            wl_list_for_each(toplevel, &workspace->toplevels, link) {
                if (toplevel) 
                    wlr_xdg_toplevel_send_close(toplevel->xdg_toplevel);
            }
        }
        wl_display_terminate(server->wl_display);
    } 
    // == Window == 
    else if (strcmp(act, "close-active-window") == 0) {
        struct buzzay_workspace *workspace = get_workspace_at_index(&server->workspaces, server->current_workspace);
        struct buzzay_toplevel *toplevel = workspace->focused_window;

        if (toplevel == NULL || toplevel->xdg_toplevel == NULL) return;
        wlr_xdg_toplevel_send_close(toplevel->xdg_toplevel);
    } else if (strcmp(act, "focus-window") == 0) {
        if (kb->command_count < 2) {
            printf("'focus-window' requires a direction to follow.");
            return;
        }

        const char *direction = kb->commands[1];

        struct buzzay_workspace *workspace = get_workspace_at_index(&server->workspaces, server->current_workspace);
        if (!workspace || !workspace->focused_window) return;

        struct buzzay_toplevel *current_toplevel = workspace->focused_window;
        struct layout_node *root_node = &workspace->layout; 
        struct layout_node *current_layout_node = find_node_for_toplevel(root_node, current_toplevel);

        if (current_layout_node) {
            struct layout_node *next_layout_node = find_node_in_direction(root_node, current_layout_node->box, direction);
            if (next_layout_node && next_layout_node->toplevel) {
                focus_toplevel(next_layout_node->toplevel);
            }
        }
    } else if (strcmp(act, "move-window") == 0) {
        if (kb->command_count < 2) {
            printf("'move-window' requires a direction to follow.");
            return;
        }

        const char *direction = kb->commands[1];

        struct buzzay_workspace *workspace = get_workspace_at_index(&server->workspaces, server->current_workspace);
        if (!workspace || !workspace->focused_window) return;

        struct buzzay_toplevel *current_toplevel = workspace->focused_window;
        struct layout_node *root_node = &workspace->layout; 
        struct layout_node *current_layout_node = find_node_for_toplevel(root_node, current_toplevel);

        if (current_layout_node) {
            struct layout_node *next_layout_node = find_node_in_direction(root_node, current_layout_node->box, direction);
            if (next_layout_node && next_layout_node->toplevel) {
                struct buzzay_toplevel *temp_toplevel = current_layout_node->toplevel;
                current_layout_node->toplevel = next_layout_node->toplevel;
                next_layout_node->toplevel = temp_toplevel;
                arrange_workspaces(server);
            }
        }
    }
    // == Workspace ==
    else if (strcmp(act, "switch-workspace") == 0) {
        if (kb->command_count < 2) {
            printf("Command count must exactly be 2 in switch-workspace.\n");
            return;
        }
        
        uint32_t new_idx = atoi(kb->commands[1]);
        if (new_idx == server->current_workspace) return;
        if (new_idx == 0) {
            // warn.
            return;
        }

        struct buzzay_workspace *this_workspace = get_workspace_at_index(&server->workspaces, server->current_workspace);
        server->current_workspace = new_idx;
        
        struct buzzay_toplevel *this_toplevel;
        wl_list_for_each(this_toplevel, &this_workspace->toplevels, link) {
            wlr_scene_node_set_enabled(&this_toplevel->scene_tree->node, false);
        }

        struct buzzay_workspace *new_workspace = get_workspace_at_index(&server->workspaces, server->current_workspace);

        /*
         * Focus the toplevel in the given workspace. First, we check if there
         * already exists a focused window in that workspace. If yes,
         * then focus it. If not, then find the first toplevel of that workspace 
         * and focus it.
         */
        wlr_seat_keyboard_notify_clear_focus(server->seat);
        if (new_workspace->focused_window) {
            focus_toplevel(new_workspace->focused_window);
        } else if (!wl_list_empty(&new_workspace->toplevels)) {
            struct buzzay_toplevel *first_toplevel = 
                wl_container_of(new_workspace->toplevels.next, first_toplevel, link);
            focus_toplevel(first_toplevel);
        }

        // Arrange the workspaces to apply result.
        arrange_workspaces(server);
    } else if (strcmp(act, "window-to-workspace") == 0) {
        if (kb->command_count < 2) {
            printf("'move-to-workspace' requires a workspace to move window to.");
            return;
        }

        const char *wsp_number_str = kb->commands[1];
        int wsp_number = atoi(wsp_number_str);

        struct buzzay_workspace *current_workspace = get_workspace_at_index(&server->workspaces, server->current_workspace);
        if (!current_workspace || !current_workspace->focused_window) return;

        struct buzzay_toplevel *current_toplevel = current_workspace->focused_window;

        struct buzzay_workspace *target_workspace = get_workspace_at_index(&server->workspaces, wsp_number);
        if (!target_workspace || target_workspace == current_workspace) return;

        wl_list_remove(&current_toplevel->link);
        workspace_remove_toplevel(current_toplevel);
        current_toplevel->in_workspace = target_workspace;
        wl_list_insert(&target_workspace->toplevels, &current_toplevel->link);

        struct layout_node *root = &target_workspace->layout;
        if (root->split_type == SPLIT_NONE && root->toplevel == NULL) {
            root->toplevel = current_toplevel;
        } else {
            struct layout_node *target = find_node_for_toplevel(root, target_workspace->focused_window);
            if (!target) {
                target = root;
            }

            struct layout_node *new_leaf = calloc(1, sizeof(struct layout_node));
            new_leaf->split_type = SPLIT_NONE;
            new_leaf->toplevel = current_toplevel;
            new_leaf->split_ratio = 0.5f;

            struct layout_node *old_child = calloc(1, sizeof(struct layout_node));
            *old_child = *target; 

            if (target->box.width >= target->box.height) {
                target->split_type = SPLIT_HORIZ;
            } else {
                target->split_type = SPLIT_VERT;
            }

            target->split_ratio = 0.5f;
            target->toplevel = NULL;
            target->first_child = old_child;
            target->second_child = new_leaf;
        }

        wlr_scene_node_set_enabled(&current_toplevel->scene_tree->node, false);

        if (!wl_list_empty(&current_workspace->toplevels)) {
            current_workspace->focused_window =
                wl_container_of(current_workspace->toplevels.next, current_toplevel, link);
        } else {
            // if it is empty reset the layout
            workspace_init(current_workspace);
        }

        target_workspace->focused_window = current_toplevel;

        focus_toplevel(current_workspace->focused_window);
        arrange_workspaces(server);
    }
    // == Layout & Monocle ==
    else if (strcmp(act, "toggle-monocle") == 0) {
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
    }
}

void handle_keybind_regsteration(struct buzzay_server *server, struct keybinding_cmd *kb_cmd, const char *key) {
    struct keybinding binding = {0};
    bool is_nested = false;

    if (!wl_list_empty(&server->outputs)) {
        struct buzzay_output *first_item = wl_container_of(
            server->outputs.next, first_item, link
        );

        is_nested = wlr_output_is_wl(first_item->wlr_output);
    }

    if (parse_keybinding_string(key, &binding, is_nested) != 0) {
        fprintf(stderr, "Error: Failed to parse keybinding string '%s'\n", key);
        free(kb_cmd);
        return;
    }
    if (!binding.is_keycode) {
        binding.key.sym = xkb_keysym_to_lower(binding.key.sym);
    }
    binding.handler = keybinding_handler;
    binding.config = kb_cmd->config;
    binding.data = kb_cmd;
    register_keybinding(binding);
}

void handle_keybinding_arr(struct buzzay_server *server, const char *key, toml_datum_t val) {
    int array_size = val.u.arr.size;
    if (array_size <= 0) {
        fprintf(stderr, "Error: Binding for '%s' has an empty command array.\n", key);
        return;
    }

    struct keybinding_cmd *kb_cmd = calloc(1, sizeof(struct keybinding_cmd));
    kb_cmd_set_init_config(kb_cmd);
    kb_cmd->command_count = 0;
    
    for (int j = 0; j < array_size && kb_cmd->command_count < 16; j++) {
        toml_datum_t elem = val.u.arr.elem[j];
        if (elem.type != TOML_STRING) {
            fprintf(stderr, "Error: Non-string element found in command array for '%s'.\n", key);
            return;
        }
        kb_cmd->commands[kb_cmd->command_count++] = strdup(elem.u.s);
    }

    if (kb_cmd->command_count < 1) {
        fprintf(stderr, "Error: commands should have at least 1 command.");
        free(kb_cmd);
        return;
    }

    handle_keybind_regsteration(server, kb_cmd, key);
}

int handle_keybinding_table(struct buzzay_server *server, const char *key, toml_datum_t val) {
    toml_datum_t action = toml_seek(val, "action");
    toml_datum_t arg = toml_seek(val, "arg");
    toml_datum_t repeat = toml_seek(val, "repeat");

    CHECK_TOML_TYPE(action, TOML_STRING, "action");
    CHECK_TOML_TYPE(arg, TOML_STRING, "arg");
    CHECK_TOML_TYPE(repeat, TOML_BOOLEAN, "repeat");

    struct keybinding_cmd *kb_cmd = calloc(1, sizeof(struct keybinding_cmd));
    kb_cmd_set_init_config(kb_cmd);
    kb_cmd->command_count = 0;

    if (not_unknown(action)) {
        kb_cmd->commands[kb_cmd->command_count++] = strdup(action.u.s);
    } else {
        fprintf(stderr, "Error: action must be provided.\n");
        return 1;
    }

    if (not_unknown(arg)) 
        kb_cmd->commands[kb_cmd->command_count++] = strdup(arg.u.s);

    if (not_unknown(repeat)) {
        kb_cmd->config.repeat = repeat.u.boolean;
    }

    handle_keybind_regsteration(server, kb_cmd, key);
    return 0;
}

int handle_config(const char *path, struct buzzay_server *server) {
    toml_result_t result = toml_parse_file_ex(path);
    if (!result.ok) {
        printf("Failed to parse toml: %s\n", result.errmsg);
        return 1;
    }

    // First setup watcher
    if (!is_path_watched(path)) {
        add_config_watcher(server, path);
    }

    // Handle keybindings (must be first as its very important)
    toml_datum_t bindings = toml_seek(result.toptab, "bindings");
    CHECK_TOML_TYPE(bindings, TOML_TABLE, "bindings");

    for (int i = 0; i < bindings.u.tab.size; i++) {
        const char *key = bindings.u.tab.key[i];
        toml_datum_t val = bindings.u.tab.value[i];

        if (val.type != TOML_ARRAY && val.type != TOML_TABLE) {
        }

        switch (val.type) {
            case TOML_ARRAY:
                handle_keybinding_arr(server, key, val);
                break;
            case TOML_TABLE:
                handle_keybinding_table(server, key, val);
                break;
            default:
                fprintf(stderr, "Error: Binding for '%s' must be an array of commands or a table.\n", key);
                break;
        }
    }

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
        toml_datum_t vrr = toml_seek(item, "vrr");
        toml_datum_t scale = toml_seek(item, "scale");
        toml_datum_t position = toml_seek(item, "position");
        toml_datum_t transform = toml_seek(item, "transform");
        toml_datum_t mode = toml_seek(item, "mode");

        CHECK_TOML_TYPE(name, TOML_STRING, "id");
        CHECK_TOML_TYPE(enabled, TOML_BOOLEAN, "enabled");
        CHECK_TOML_TYPE(vrr, TOML_BOOLEAN, "vrr");
        CHECK_TOML_TYPE(scale, TOML_FP64, "scale");
        CHECK_TOML_TYPE(position, TOML_ARRAY, "position");
        CHECK_TOML_TYPE(transform, TOML_STRING, "transform");
        CHECK_TOML_TYPE(mode, TOML_STRING, "mode");

        struct buzzay_output *output;
        wl_list_for_each(output, &server->outputs, link) {
            if (strcmp(output->wlr_output->name, name.u.s) == 0) {
                struct wlr_output_state state;
                wlr_output_state_init(&state);

                if (not_unknown(enabled)) wlr_output_state_set_enabled(&state, enabled.u.boolean);
                if (not_unknown(scale)) wlr_output_state_set_scale(&state, scale.u.fp64);

                if (not_unknown(vrr)) {
                    wlr_output_state_set_adaptive_sync_enabled(&state, vrr.u.boolean);
                }

                if (not_unknown(transform)) output_apply_transform(&state, transform.u.s);
                if (not_unknown(mode)) output_apply_mode(output->wlr_output, &state, mode.u.s);

                if (wlr_output_test_state(output->wlr_output, &state))
                    wlr_output_commit_state(output->wlr_output, &state);
                wlr_output_state_finish(&state);

                if (not_unknown(position)) {
                    if (position.u.arr.size < 2) {
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
    toml_datum_t blur_strength = toml_seek(result.toptab, "candy.blur.strength");
    toml_datum_t blur_alpha = toml_seek(result.toptab, "candy.blur.alpha");
    toml_datum_t blur_passes = toml_seek(result.toptab, "candy.blur.passes");
    toml_datum_t blur_noise = toml_seek(result.toptab, "candy.blur.noise");
    CHECK_TOML_TYPE(blur_strength, TOML_FP64, "strength");
    CHECK_TOML_TYPE(blur_alpha, TOML_FP64, "alpha");
    CHECK_TOML_TYPE(blur_passes, TOML_INT64, "passes");
    CHECK_TOML_TYPE(blur_noise, TOML_FP64, "noise");

    if (not_unknown(blur_strength)) server->eyecandies.blur_strength = blur_strength.u.fp64;
    if (not_unknown(blur_alpha)) server->eyecandies.blur_alpha = blur_alpha.u.fp64;
    if (not_unknown(blur_passes)) server->eyecandies.blur_passes = blur_passes.u.int64;
    if (not_unknown(blur_noise)) server->eyecandies.blur_noise = blur_noise.u.fp64;
    scene_set_blur_confs(server);

    struct buzzay_workspace *workspace = get_workspace_at_index(&server->workspaces, server->current_workspace);
    struct buzzay_toplevel *this_toplevel;
    wl_list_for_each(this_toplevel, &workspace->toplevels, link) {
        toplevel_apply_blur_confs(this_toplevel);
    }

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

    arrange_workspaces(server);

    toml_free(result);
    return 0;
}
