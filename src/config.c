#include <stdio.h>
#include <tomlc17.h>

#include "config.h"

int handle_config(const char *path, struct buzzay_server *server) {
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
