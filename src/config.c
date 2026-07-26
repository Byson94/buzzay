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

    // Handle eyecandy
    toml_datum_t eyecandy = toml_seek(result.toptab, "candy");
    if (eyecandy.type != TOML_TABLE && eyecandy.type != TOML_UNKNOWN) {
        printf("'candy' must be a table.\n");
        return 1;
    }
    toml_datum_t candy_gap = toml_seek(eyecandy, "gap");
    toml_datum_t candy_opacity = toml_seek(eyecandy, "opacity");

    if (candy_gap.type != TOML_INT64 && candy_gap.type != TOML_UNKNOWN) {
        printf("'gap' of candy must be an integer.\n");
        return 1;
    }
    server->eyecandies.gap = candy_gap.u.int64;

    if (candy_opacity.type != TOML_FP64 && candy_opacity.type != TOML_UNKNOWN) {
        printf("'opacity' of candy must be a float.\n");
        return 1;
    }
    server->eyecandies.window_opacity = candy_opacity.u.fp64;

    // Handle border
    toml_datum_t eyecandy_border = toml_seek(result.toptab, "candy.border");
    if (eyecandy_border.type != TOML_TABLE && eyecandy.type != TOML_UNKNOWN) {
        printf("'candy.border' must be a table.");
        return 1;
    }
    
    toml_datum_t active_clr = toml_seek(eyecandy_border, "active");
    toml_datum_t inactive_clr = toml_seek(eyecandy_border, "inactive");
    toml_datum_t bdr_thickness = toml_seek(eyecandy_border, "thickness");

    if (active_clr.type != TOML_STRING && active_clr.type != TOML_UNKNOWN) {
        printf("'active' of candy.border must be a string.\n");
        return 1;
    }
    if (inactive_clr.type != TOML_STRING && inactive_clr.type != TOML_UNKNOWN) {
        printf("'inactive' of candy.border must be a string.\n");
        return 1;
    }

    parse_color(active_clr.u.s, server->eyecandies.active_border);
    parse_color(inactive_clr.u.s, server->eyecandies.inactive_border);

    if (bdr_thickness.type != TOML_INT64 &&  bdr_thickness.type != TOML_UNKNOWN) {
        printf("'thickness' of candy.border must be an integer.");
        return 1;
    }

    // Handle blur
    toml_datum_t eyecandy_blur = toml_seek(result.toptab, "eyecandy.blur");
    if (eyecandy_blur.type != TOML_TABLE && eyecandy_blur.type != TOML_UNKNOWN) {
        printf("'eyecandy.blur' must be a table.\n");
        return 1;
    }

    toml_datum_t blur_enabled = toml_seek(eyecandy_blur, "enabled");
    toml_datum_t blur_strength = toml_seek(eyecandy_blur, "strength");
    toml_datum_t blur_alpha = toml_seek(eyecandy_blur, "alpha");

    if (blur_enabled.type != TOML_BOOLEAN && blur_enabled.type != TOML_UNKNOWN) {
        printf("'enabled' of eyecandy.blur must be a boolean.\n");
        return 1;
    }
    server->eyecandies.blur_enabled = blur_enabled.u.boolean;

    if (blur_strength.type != TOML_FP64 && blur_strength.type != TOML_UNKNOWN) {
        printf("'strength' of eyecandy.blur must be a float.\n");
        return 1;
    }
    server->eyecandies.blur_strength = blur_strength.u.fp64;

    if (blur_alpha.type != TOML_FP64 && blur_alpha.type != TOML_UNKNOWN) {
        printf("'alpha' of eyecandy.blur must be a float.\n");
        return 1;
    }
    server->eyecandies.blur_alpha = blur_alpha.u.fp64;

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
