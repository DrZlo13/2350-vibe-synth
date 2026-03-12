#include <math.h>
#include <assert.h>
#include <stdbool.h>
#include "render.h"
#include "font/font_render.h"
#include "font/fonts.h"
#include "display.h"

#define TAG "Render"

#define CANARY_VALUE 0xDEADBEEF
#define DATA_SIZE    (D_WIDTH * D_HEIGHT)
#define SWAP(x, y)          \
    do {                    \
        typeof(x) SWAP = x; \
        x = y;              \
        y = SWAP;           \
    } while(0)
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#ifdef RENDER_DEBUG_ENABLE
#define RENDER_DEBUG(...) FURI_LOG_I(__VA_ARGS__)
#else
#define RENDER_DEBUG(...)
#endif

const int32_t display_offset_x = 0;
const int32_t display_offset_y = 0;

struct RenderBuffer {
    uint32_t canary_pre;
    Color data[DATA_SIZE];
    uint32_t canary_post;
};

typedef struct {
    RenderBuffer* current_buffer;
    int32_t scissors_x0;
    int32_t scissors_y0;
    int32_t scissors_x1;
    int32_t scissors_y1;
} RenderData;

static RenderData render_data = {
    .current_buffer = NULL,
    .scissors_x0 = 0,
    .scissors_y0 = 0,
    .scissors_x1 = D_WIDTH,
    .scissors_y1 = D_HEIGHT,
};

static inline void memset_color(Color* dest, Color color, size_t count) {
    for(size_t i = 0; i < count; i++) {
        dest[i] = color;
    }
}

static const void* render_get_font_by_id(Font font_id) {
    switch(font_id) {
    case FontBody:
        return u8g2_font_haxrcorp4089_tr;
    case FontButton:
        return u8g2_font_helvB08_tr;
    case FontKeyboard:
        return u8g2_font_profont11_mr;
    default:
        return u8g2_font_haxrcorp4089_tr;
    }
}

static inline void render_set_pixel(int32_t x, int32_t y, Color color) {
    x += display_offset_x;
    y += display_offset_y;
    if(x < render_data.scissors_x0 || x >= render_data.scissors_x1 || y < render_data.scissors_y0 || y >= render_data.scissors_y1) {
        return;
    }
    render_data.current_buffer->data[y * D_WIDTH + x] = color;
}

static inline void render_draw_hline(int32_t x0, int32_t y, int32_t x1, Color color) {
    x0 += display_offset_x;
    x1 += display_offset_x;
    y += display_offset_y;

    if(y < render_data.scissors_y0 || y >= render_data.scissors_y1) return;
    if(x0 < render_data.scissors_x0 && x1 < render_data.scissors_x0) return;
    if(x0 >= render_data.scissors_x1 && x1 >= render_data.scissors_x1) return;

    if(x0 > x1) SWAP(x0, x1);
    if(x0 < render_data.scissors_x0) x0 = render_data.scissors_x0;
    if(x1 > render_data.scissors_x1) x1 = render_data.scissors_x1;
    if(x0 > x1) SWAP(x0, x1);

    memset_color(&render_data.current_buffer->data[y * D_WIDTH + x0], color, x1 - x0);
}

static inline void render_draw_vline(int32_t x, int32_t y0, int32_t y1, Color color) {
    x += display_offset_x;
    y0 += display_offset_y;
    y1 += display_offset_y;

    if(x < render_data.scissors_x0 || x >= render_data.scissors_x1) return;
    if(y0 < render_data.scissors_y0 && y1 < render_data.scissors_y0) return;
    if(y0 >= render_data.scissors_y1 && y1 >= render_data.scissors_y1) return;

    if(y0 > y1) SWAP(y0, y1);
    if(y0 < render_data.scissors_y0) y0 = render_data.scissors_y0;
    if(y1 > render_data.scissors_y1) y1 = render_data.scissors_y1;
    if(y0 > y1) SWAP(y0, y1);

    for(int32_t y = y0; y < y1; y++) {
        render_data.current_buffer->data[y * D_WIDTH + x] = color;
    }
}

static inline void render_draw_circle(int32_t xc, int32_t yc, int32_t r, Color color) {
    int32_t x = 0;
    int32_t y = r;
    int32_t d = 3 - 2 * r;
    while(x <= y) {
        render_set_pixel(xc + x, yc + y, color);
        render_set_pixel(xc - x, yc + y, color);
        render_set_pixel(xc + x, yc - y, color);
        render_set_pixel(xc - x, yc - y, color);
        render_set_pixel(xc + y, yc + x, color);
        render_set_pixel(xc - y, yc + x, color);
        render_set_pixel(xc + y, yc - x, color);
        render_set_pixel(xc - y, yc - x, color);
        if(d < 0) {
            d = d + 4 * x + 6;
        } else {
            d = d + 4 * (x - y) + 10;
            y--;
        }
        x++;
    }
}

static inline void render_draw_circle_filled(int32_t xc, int32_t yc, int32_t r, Color color) {
    int32_t x = 0;
    int32_t y = r;
    int32_t d = 3 - 2 * r;
    while(x <= y) {
        render_draw_hline(xc - x, yc - y, xc + x + 1, color);
        render_draw_hline(xc - x, yc + y, xc + x + 1, color);
        render_draw_hline(xc - y, yc - x, xc + y + 1, color);
        render_draw_hline(xc - y, yc + x, xc + y + 1, color);
        if(d < 0) {
            d = d + 4 * x + 6;
        } else {
            d = d + 4 * (x - y) + 10;
            y--;
        }
        x++;
    }
}

static inline void render_draw_arc(int32_t xc, int32_t yc, int32_t r, float deg_start, float deg_stop, Color color) {
    int32_t x = 0;
    int32_t y = r;
    int32_t d = 3 - 2 * r;

    // Normalize angles to [0, 360)
    while(deg_start < 0)
        deg_start += 360.0f;
    while(deg_stop < 0)
        deg_stop += 360.0f;
    deg_start = fmodf(deg_start, 360.0f);
    deg_stop = fmodf(deg_stop, 360.0f);

    while(x <= y) {
        // 8 octant points
        int32_t points[8][2] = {
            {xc + x, yc + y},
            {xc - x, yc + y},
            {xc + x, yc - y},
            {xc - x, yc - y},
            {xc + y, yc + x},
            {xc - y, yc + x},
            {xc + y, yc - x},
            {xc - y, yc - x},
        };

        for(int i = 0; i < 8; i++) {
            float angle = atan2f((float)(points[i][1] - yc), (float)(points[i][0] - xc)) * (180.0f / 3.14159265f);
            if(angle < 0) angle += 360.0f;

            // Check if angle is within arc range
            bool in_range = false;
            if(deg_start < deg_stop) {
                in_range = (angle >= deg_start && angle <= deg_stop);
            } else {
                in_range = (angle >= deg_start || angle <= deg_stop);
            }

            if(in_range) {
                render_set_pixel(points[i][0], points[i][1], color);
            }
        }

        if(d < 0) {
            d = d + 4 * x + 6;
        } else {
            d = d + 4 * (x - y) + 10;
            y--;
        }
        x++;
    }
}

static inline void render_fill_arc(int32_t xc, int32_t yc, int32_t r, float deg_start, float deg_stop, Color color) {
    while(deg_start < 0)
        deg_start += 360.0f;
    while(deg_stop < 0)
        deg_stop += 360.0f;
    deg_start = fmodf(deg_start, 360.0f);
    deg_stop = fmodf(deg_stop, 360.0f);

    for(int32_t y = -r; y <= r; y++) {
        for(int32_t x = -r; x <= r; x++) {
            int32_t dx = x;
            int32_t dy = y;
            if(dx * dx + dy * dy <= r * r) {
                float angle = atan2f((float)dy, (float)dx) * (180.0f / 3.14159265f);
                if(angle < 0) angle += 360.0f;

                // Check if angle is within arc range
                bool in_range = false;
                if(deg_start < deg_stop) {
                    in_range = (angle >= deg_start && angle <= deg_stop);
                } else {
                    in_range = (angle >= deg_start || angle <= deg_stop);
                }

                if(in_range) {
                    render_set_pixel(xc + x, yc + y, color);
                }
            }
        }
    }
}

static inline void render_draw_rectangle(int32_t x, int32_t y, int32_t width, int32_t height, Color color) {
    render_draw_hline(x, y, x + width, color);
    render_draw_hline(x, y + height - 1, x + width, color);
    render_draw_vline(x, y, y + height, color);
    render_draw_vline(x + width - 1, y, y + height, color);
}

static inline void render_fill_rectangle(int32_t x, int32_t y, int32_t width, int32_t height, Color color) {
    for(int32_t j = y; j < y + height; j++) {
        render_draw_hline(x, j, x + width, color);
    }
}

void render_clear_buffer(Color color) {
    memset_color(render_data.current_buffer->data, color, DATA_SIZE);
}

static void render_draw_pixel_fg(int32_t x, int32_t y, void* context) {
    Color color = *(Color*)context;
    render_set_pixel(x, y, color);
}

static void render_draw_pixel_bg(int32_t x, int32_t y, void* context) {
}

static float render_clamp_corner_radius(float y_size, float radius) {
    if(radius < 1.0f) {
        return 0.0f;
    }
    if(radius > y_size / 2) {
        return y_size / 2;
    }
    // Trying to draw a 2x2 ellipse seems to result in just a dot, so if
    // there is a corner radius at minimum it must be 2
    return MAX(2, radius);
}

void render_rectangle(
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height,
    int32_t radius_top_left,
    int32_t radius_top_right,
    int32_t radius_bottom_right,
    int32_t radius_bottom_left,
    Color color) {
    uint32_t r_top_left = render_clamp_corner_radius(height, radius_top_left);
    uint32_t r_top_right = render_clamp_corner_radius(height, radius_top_right);
    uint32_t r_bottom_right = render_clamp_corner_radius(height, radius_bottom_right);
    uint32_t r_bottom_left = render_clamp_corner_radius(height, radius_bottom_left);

    RENDER_DEBUG(TAG, "Rectangle");
    RENDER_DEBUG(TAG, "    [x: %.1f, y: %.1f, w: %.1f, h: %.1f] c%X", x, y, width, height, color);
    RENDER_DEBUG(TAG, "    [%lu, %lu, %lu, %lu]", r_top_left, r_top_right, r_bottom_right, r_bottom_left);

    if(!r_top_left && !r_top_right && !r_bottom_right && !r_bottom_left) {
        render_fill_rectangle(x, y, width, height, color);
        return;
    }

    {
        render_fill_arc(x + r_top_left, y + r_top_left, r_top_left, 180.f, 270.f, color);
        render_fill_arc(x + width - r_top_right - 1, y + r_top_right, r_top_right, 270.0f, 0.0f, color);
        render_fill_arc(x + width - r_bottom_right - 1, y + height - r_bottom_right - 1, r_bottom_right, 0.f, 90.f, color);
        render_fill_arc(x + r_bottom_left, y + height - r_bottom_left - 1, r_bottom_left, 90.f, 180.f, color);
    }

    {
        render_fill_rectangle(x + r_top_left, y, width - r_top_left - r_top_right, MAX(r_top_left, r_top_right), color);

        int32_t bottom_height = MAX(r_bottom_left, r_bottom_right);
        render_fill_rectangle(x + r_bottom_left, y + height - bottom_height, width - r_bottom_left - r_bottom_right, bottom_height, color);

        int32_t middle_height = height - MIN(r_bottom_right, r_bottom_left) - MIN(r_top_right, r_top_left);
        render_fill_rectangle(
            x + MIN(r_top_left, r_bottom_left), y + MIN(r_top_right, r_top_left), width - r_bottom_left - r_bottom_right, middle_height, color);

        int32_t left_height = height - r_top_left - r_bottom_left;
        int32_t left_width = MAX(r_top_left, r_bottom_left);
        render_fill_rectangle(x, y + r_top_left, left_width, left_height, color);

        int32_t right_height = height - r_top_right - r_bottom_right;
        int32_t right_width = MAX(r_top_right, r_bottom_right);
        render_fill_rectangle(x + width - right_width, y + r_top_right, right_width, right_height, color);
    }
}

void render_border(
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height,
    int32_t radius_top_left,
    int32_t radius_top_right,
    int32_t radius_bottom_right,
    int32_t radius_bottom_left,
    int32_t border_width_top,
    int32_t border_width_right,
    int32_t border_width_bottom,
    int32_t border_width_left,
    Color color) {
    uint32_t r_top_left = render_clamp_corner_radius(height, radius_top_left);
    uint32_t r_top_right = render_clamp_corner_radius(height, radius_top_right);
    uint32_t r_bottom_right = render_clamp_corner_radius(height, radius_bottom_right);
    uint32_t r_bottom_left = render_clamp_corner_radius(height, radius_bottom_left);

    RENDER_DEBUG(TAG, "Border");
    RENDER_DEBUG(TAG, "    [x: %.1f, y: %.1f, w: %.1f, h: %.1f] c%X", x, y, width, height, color);
    RENDER_DEBUG(TAG, "    [%lu, %lu, %lu, %lu]", r_top_left, r_top_right, r_bottom_right, r_bottom_left);
    RENDER_DEBUG(TAG, "    [%d, %d, %d, %d]", border_width_top, border_width_right, border_width_bottom, border_width_left);

    if(border_width_top > 0) {
        render_draw_arc(x + r_top_left, y + r_top_left, r_top_left, 180.f, 270.f, color);
        render_fill_rectangle(x + r_top_left, y, width - r_top_left - r_top_right, border_width_top, color);
        render_draw_arc(x + width - r_top_right - 1, y + r_top_right, r_top_right, 270.f, 0.f, color);
    }

    if(border_width_right > 0 && r_top_right + r_bottom_right <= height) {
        render_fill_rectangle(x + width - border_width_right, y + r_top_right, border_width_right, height - r_top_right - r_bottom_right, color);
    }

    if(border_width_bottom > 0) {
        render_draw_arc(x + width - r_bottom_right - 1, y + height - r_bottom_right - 1, r_bottom_right, 0.f, 90.f, color);
        render_fill_rectangle(x + r_bottom_left, y + height - border_width_bottom, width - r_bottom_left - r_bottom_right, border_width_bottom, color);
        render_draw_arc(x + r_bottom_left, y + height - r_bottom_left - 1, r_bottom_left, 90.f, 180.f, color);
    }

    if(border_width_left > 0 && r_bottom_left + r_top_left < height) {
        render_fill_rectangle(x, y + r_top_left, border_width_left, height - r_top_left - r_bottom_left, color);
    }
}

void render_text(int32_t x, int32_t y, const char* text, Font font_id, Color color) {
    const void* font = render_get_font_by_id(font_id);

    U8G2FontRender_t render = U8G2FontRender(font, render_draw_pixel_fg, render_draw_pixel_bg, &color);
    u8g2_render_print_multiline(&render, x, y, text, strlen(text));
}

void render_text_aligned(int32_t x, int32_t y, Align horizontal_align, Align vertical_align, const char* text, Font font_id, Color color) {
    const void* font = render_get_font_by_id(font_id);

    U8G2FontRender_t render = U8G2FontRender(font, render_draw_pixel_fg, render_draw_pixel_bg, &color);
    Dimension text_dim = {
        .width = u8g2_font_get_string_width_multiline(&render, text, strlen(text)),
        .height = u8g2_font_get_height(&render),
    };

    if(horizontal_align == Center) {
        x -= text_dim.width / 2;
    } else if(horizontal_align == Max) {
        x -= text_dim.width;
    }

    if(vertical_align == Center) {
        y -= text_dim.height / 2;
    } else if(vertical_align == Max) {
        y -= text_dim.height;
    }

    u8g2_render_print_multiline(&render, x, y, text, strlen(text));
}

void render_scissor_start(BoundingBox* bb) {
    RENDER_DEBUG(TAG, "Scissor start");
    RENDER_DEBUG(TAG, "    [x: %.1f, y: %.1f, w: %.1f, h: %.1f]", bb->x, bb->y, bb->width, bb->height);

    render_data.scissors_x0 = bb->x;
    render_data.scissors_y0 = bb->y;
    render_data.scissors_x1 = bb->x + bb->width;
    render_data.scissors_y1 = bb->y + bb->height;

    render_data.scissors_x0 += display_offset_x;
    render_data.scissors_y0 += display_offset_y;
    render_data.scissors_x1 += display_offset_x;
    render_data.scissors_y1 += display_offset_y;

    // Clamp scissors to buffer dimensions
    if(render_data.scissors_x0 < 0) render_data.scissors_x0 = 0;
    if(render_data.scissors_y0 < 0) render_data.scissors_y0 = 0;
    if(render_data.scissors_x1 > (int32_t)D_WIDTH) render_data.scissors_x1 = D_WIDTH;
    if(render_data.scissors_y1 > (int32_t)D_HEIGHT) render_data.scissors_y1 = D_HEIGHT;
}

void render_scissor_end(void) {
    RENDER_DEBUG(TAG, "Scissor end");
    render_data.scissors_x0 = 0;
    render_data.scissors_y0 = 0;
    render_data.scissors_x1 = D_WIDTH;
    render_data.scissors_y1 = D_HEIGHT;
}

Dimension render_measure_text(const char* str, Font font_id) {
    const void* font = render_get_font_by_id(font_id);
    U8G2FontRender_t font_render = U8G2FontRender(font, render_draw_pixel_fg, render_draw_pixel_bg, NULL);

    Dimension dimensions = {
        .width = u8g2_font_get_string_width_multiline(&font_render, str, strlen(str)),
        .height = u8g2_font_get_height(&font_render),
    };

    return dimensions;
}

RenderBuffer* render_alloc_buffer(void) {
    RenderBuffer* buffer = malloc(sizeof(RenderBuffer));
    memset(buffer, 0, sizeof(RenderBuffer));
    buffer->canary_pre = CANARY_VALUE;
    buffer->canary_post = CANARY_VALUE;
    return buffer;
}

Color* render_get_buffer_data(RenderBuffer* buffer) {
    if(buffer->canary_pre != CANARY_VALUE || buffer->canary_post != CANARY_VALUE) {
        assert(false && "RenderBuffer canary corrupted");
    }
    return buffer->data;
}

RenderBuffer* render_get_current_buffer(void) {
    return render_data.current_buffer;
}

void render_set_current_buffer(RenderBuffer* buffer) {
    render_data.current_buffer = buffer;
}

size_t render_get_buffer_width(RenderBuffer* buffer) {
    return D_WIDTH;
}

size_t render_get_buffer_height(RenderBuffer* buffer) {
    return D_HEIGHT;
}
