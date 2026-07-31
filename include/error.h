#pragma once

#include <wlr/backend.h>
#include <wlr/types/wlr_scene.h>

struct custom_error_bar {
    struct wlr_scene_tree *tree;
    struct wlr_scene_rect *bg_rect;
    struct wlr_scene_rect *border_rect;
    struct wlr_scene_rect *progress_rect;
    struct wlr_scene_buffer *text_node;

    struct wl_event_source *timer;
    uint32_t start_time_ms;
    int display_width;
    int progress_height;
};

extern struct buzzay_server *buzzay_server_ptr;

struct custom_error_bar *create_error_bar(int display_width, const char *error_msg);
void destroy_error_bar(struct custom_error_bar *bar);
void save_buzzay_server(struct buzzay_server *sptr);
