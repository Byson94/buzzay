#include <stdlib.h>
#include <drm_fourcc.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/interfaces/wlr_buffer.h>
#include <cairo/cairo.h>
#include <pango/pangocairo.h>
#include <time.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_cursor.h>

#include "macro-utils.h"
#include "server.h"
#include "error.h"

// memory buffer
struct error_buffer {
    struct wlr_buffer base;
    void *data;
    cairo_surface_t *surface;
};

static void error_buffer_destroy(struct wlr_buffer *wlr_buffer) {
    struct error_buffer *buffer = wl_container_of(wlr_buffer, buffer, base);
    cairo_surface_destroy(buffer->surface);
    free(buffer->data);
    wlr_buffer_finish(&buffer->base);
    free(buffer);
}

static bool error_buffer_begin_data_ptr_access(struct wlr_buffer *wlr_buffer,
                                                uint32_t flags, void **data, 
                                                uint32_t *format, size_t *stride) {
    UNUSED(flags);

    struct error_buffer *buffer = wl_container_of(wlr_buffer, buffer, base);
    *data = buffer->data;
    *stride = cairo_image_surface_get_stride(buffer->surface);
    *format = DRM_FORMAT_ARGB8888;
    return true;
}

static void error_buffer_end_data_ptr_access(struct wlr_buffer *wlr_buffer) {
    // No-op for CPU buffers
    UNUSED(wlr_buffer);
}

static const struct wlr_buffer_impl error_buffer_impl = {
    .destroy = error_buffer_destroy,
    .begin_data_ptr_access = error_buffer_begin_data_ptr_access,
    .end_data_ptr_access = error_buffer_end_data_ptr_access,
};

// Render the text using pangocairo and cairo
static struct wlr_buffer *render_text_buffer(const char *text, int *out_w, int *out_h, int *out_ascent) {
    // Calculation
    cairo_surface_t *surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
    cairo_t *cr = cairo_create(surf);
    PangoLayout *layout = pango_cairo_create_layout(cr);
    pango_layout_set_text(layout, text, -1);
    
    PangoFontDescription *font_desc = pango_font_description_from_string("Sans Bold 12");
    pango_layout_set_font_description(layout, font_desc);

    PangoContext *context = pango_layout_get_context(layout);
    PangoFontMetrics *metrics = pango_context_get_metrics(context, font_desc, pango_language_get_default());

    if (out_ascent) {
        *out_ascent = pango_font_metrics_get_ascent(metrics) / PANGO_SCALE;
    }

    pango_font_metrics_unref(metrics);
    pango_font_description_free(font_desc);

    pango_layout_get_pixel_size(layout, out_w, out_h);
    cairo_destroy(cr);
    cairo_surface_destroy(surf);

    if (*out_w <= 0 || *out_h <= 0) {
        g_object_unref(layout);
        return NULL;
    }

    // Allocation
    int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, *out_w);
    void *data = calloc(1, stride * (*out_h));
    surf = cairo_image_surface_create_for_data(data, CAIRO_FORMAT_ARGB32, *out_w, *out_h, stride);
    
    cr = cairo_create(surf);
    pango_cairo_update_layout(cr, layout);
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0); 
    pango_cairo_show_layout(cr, layout);

    g_object_unref(layout);
    cairo_destroy(cr);

    struct error_buffer *buf = calloc(1, sizeof(*buf));
    wlr_buffer_init(&buf->base, &error_buffer_impl, *out_w, *out_h);
    buf->data = data;
    buf->surface = surf;

    return &buf->base;
}

static uint32_t get_current_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

static int error_bar_timer_handler(void *data) {
    struct custom_error_bar *bar = data;
    uint32_t now = get_current_time_ms();
    uint32_t elapsed = now - bar->start_time_ms;
    uint32_t duration_ms = 5000;

    if (elapsed >= duration_ms) {
        destroy_error_bar(bar);
        return 0; 
    }

    int current_w = (int)((float)elapsed / (float)duration_ms * bar->display_width);
    wlr_scene_rect_set_size(bar->progress_rect, current_w, bar->progress_height);

    wl_event_source_timer_update(bar->timer, 16);
    return 0;
}

struct wlr_output *get_active_output_by_cursor(struct buzzay_server *server) {
    double cx = server->cursor->x;
    double cy = server->cursor->y;

    struct wlr_output *output = wlr_output_layout_output_at(
        server->output_layout, cx, cy
    );

    return output; 
}

struct buzzay_server *buzzay_server_ptr = NULL;
void save_buzzay_server(struct buzzay_server *sptr) {
    buzzay_server_ptr = sptr;
}

struct custom_error_bar *create_error_bar(int display_width, const char *error_msg) {
    struct custom_error_bar *bar = calloc(1, sizeof(struct custom_error_bar));
    if (!bar || !buzzay_server_ptr) return NULL;
    
    struct wlr_scene_tree *drawing_area = buzzay_server_ptr->layers.native_overlay;
    struct wl_event_loop *wl_loop = buzzay_server_ptr->wl_event_loop;
    struct wlr_output *output = get_active_output_by_cursor(buzzay_server_ptr);

    if (!output) return NULL;

    bar->display_width = display_width;
    bar->progress_height = 3;
    
    bar->tree = wlr_scene_tree_create(drawing_area);

    float dark_bg_slate[4] = {0.12f, 0.13f, 0.15f, 1.00f};
    float fill_color[4] = {0.85f, 0.45f, 0.35f, 1.00f};
    float *border_clr = buzzay_server_ptr->eyecandies.active_border;

    int bar_height = 40;
    int x_pos = (output->width - display_width) / 2;
    int y_pos = 10;

    bar->border_rect = wlr_scene_rect_create(bar->tree, display_width + 2, bar_height + 2, border_clr);
    bar->bg_rect = wlr_scene_rect_create(bar->tree, display_width, bar_height, dark_bg_slate);
    bar->progress_rect = wlr_scene_rect_create(bar->tree, 0, bar->progress_height, fill_color);

    wlr_scene_node_set_position(&bar->border_rect->node, -1, -1);
    wlr_scene_node_set_position(&bar->bg_rect->node, 0, 0);

    int progress_y = bar_height - bar->progress_height;
    wlr_scene_node_set_position(&bar->progress_rect->node, 0, progress_y);

    int text_w = 0, text_h = 0, font_ascent = 0;
    struct wlr_buffer *text_buf = render_text_buffer(error_msg, &text_w, &text_h, &font_ascent);

    if (text_buf) {
        bar->text_node = wlr_scene_buffer_create(bar->tree, text_buf);
        wlr_buffer_drop(text_buf);

        int text_x = (display_width - text_w) / 2;
        int text_y = (bar_height - font_ascent) / 2 - (text_h / 8);
        wlr_scene_node_set_position(&bar->text_node->node, text_x, text_y);
    }

    wlr_scene_node_set_position(&bar->tree->node, x_pos, y_pos);

    bar->start_time_ms = get_current_time_ms();
    bar->timer = wl_event_loop_add_timer(wl_loop, error_bar_timer_handler, bar);
    
    wl_event_source_timer_update(bar->timer, 16);

    return bar;
}

void destroy_error_bar(struct custom_error_bar *bar) {
    if (!bar) return;
    
    if (bar->timer) {
        wl_event_source_remove(bar->timer);
        bar->timer = NULL;
    }

    wlr_scene_node_destroy(&bar->tree->node);
    free(bar);
}
