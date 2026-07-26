#include <stdio.h>
#include <string.h>
#include <tomlc17.h>
#include <wayland-util.h>
#include <wlr/types/wlr_output_layout.h>

#include "macro-utils.h"
#include "output.h"
#include "config.h"

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

int handle_config(const char *path, struct buzzay_server *server) {
    toml_result_t result = toml_parse_file_ex(path);
    if (!result.ok) {
        printf("Failed to parse toml: %s\n", result.errmsg);
        return 1;
    }

    // Apply monitors
    toml_datum_t monitors = toml_seek(result.toptab, "monitor");
    if (monitors.type != TOML_ARRAY && monitors.type != TOML_UNKNOWN) {
        printf("'monitor' must of an array of tables.\n");
        return 1;
    }

    for (int i = 0; i < monitors.u.arr.size; i++) {
        toml_datum_t item = monitors.u.arr.elem[i];

        if (item.type != TOML_TABLE) {
            continue;
        }

        toml_datum_t name = toml_seek(item, "id");
        toml_datum_t enabled = toml_seek(item, "enabled");
        toml_datum_t scale = toml_seek(item, "scale");
        toml_datum_t position = toml_seek(item, "position");

        if (name.type != TOML_STRING && name.type != TOML_UNKNOWN) {
            printf("'id' of monitor must be of type string.\n");
            break;
        }

        struct buzzay_output *output;
        wl_list_for_each(output, &server->outputs, link) {
            if (strcmp(output->wlr_output->name, name.u.s) == 0) {
                if (enabled.type == TOML_BOOLEAN) output->wlr_output->enabled = enabled.u.boolean;
                if (scale.type == TOML_FP64) output->wlr_output->scale = scale.u.fp64;
                if (scale.type == TOML_ARRAY) {
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

    // Handle keybindings
    toml_datum_t bindings = toml_seek(result.toptab, "bindings");
    if (bindings.type != TOML_TABLE && bindings.type != TOML_UNKNOWN) {
        printf("'bindings' must be a table.\n");
        return 1;
    }
    for (int i = 0; i < bindings.u.tab.size; i++) {
        const char *key = bindings.u.tab.key[i];
        int key_len = bindings.u.tab.len[i];
        toml_datum_t val = bindings.u.tab.value[i];

        // handle
        UNUSED(key);
        UNUSED(key_len);
        UNUSED(val);
    }

    toml_free(result);
    return 0;
}
