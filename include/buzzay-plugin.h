#pragma once

#ifndef BUZZAY_PLUGIN
#define BUZZAY_PLUGIN

#include <xkbcommon/xkbcommon.h>

// MUST increment once every release
// IF a change is made to the file
#define BUZZAY_API_VERSION 1
#define BZ_API __attribute__((visibility("default")))

/**
 *  The plugin wrapper structure that contains important
 *  metadata like plugin name, path, and the buzzay server.
 */
struct bz_plugin {
    const char *plugin_name; /**< The name of the plugin. */
    const char *plugin_path; /**< The version number of the plugin. */

    /**
     * Any data that you can insert into the plugin.
     */
    void *data;

    /**
     * @internal
     * Internal compositor server.
     */
    void *_internal_server;

    /**
     * @internal
     * Internal compositor server size.
     */
    size_t _internal_server_size;
};

// General

/**
 * Send a message back to the IPC client.
 */
BZ_API void write_ipc_response(int client_fd, const char* msg);

/**
 * Quit buzzay.
 */
BZ_API void bz_quit(struct bz_plugin *plugin);

/**
 * Abort the plugin.
 */
BZ_API void bz_abort_plugin(struct bz_plugin *plugin);

/** Decoration modes **/
enum bz_decoration_mode {
    /** Let the client draw their own decoration **/
    BZ_DECORATION_CLIENT_SIDE,
    /** Does not apply any decoration **/
    BZ_DECORATION_SERVER_SIDE,
};

/**
 * Set window decoration mode.
 */
BZ_API void bz_set_decoration_mode(struct bz_plugin *plugin, enum bz_decoration_mode mode);

/**
 * Enable xdg interactives like window move, window resize, etc.
 */
BZ_API void bz_enable_xdg_interactive(struct bz_plugin *plugin, bool enable);

#endif
