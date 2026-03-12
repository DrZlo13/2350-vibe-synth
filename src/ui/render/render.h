#pragma once
#include <stdint.h>
#include <stddef.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef enum {
    FontBody,
    FontButton,
    FontKeyboard,
} Font;

typedef struct RenderBuffer RenderBuffer;

typedef struct {
    float width;
    float height;
} Dimension;

typedef struct {
    float x;
    float y;
    float width;
    float height;
} BoundingBox;

typedef struct {
    float top_left;
    float top_right;
    float bottom_right;
    float bottom_left;
} CornerRadius;

typedef uint16_t Color;

void render_clear_buffer(Color color);

RenderBuffer* render_alloc_buffer(void);

Color* render_get_buffer_data(RenderBuffer* buffer);

RenderBuffer* render_get_current_buffer(void);

void render_set_current_buffer(RenderBuffer* buffer);

size_t render_get_buffer_width(RenderBuffer* buffer);

size_t render_get_buffer_height(RenderBuffer* buffer);

typedef enum {
    Min,
    Center,
    Max,
} Align;

void render_rectangle(
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height,
    int32_t radius_top_left,
    int32_t radius_top_right,
    int32_t radius_bottom_right,
    int32_t radius_bottom_left,
    Color color);
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
    Color color);
void render_text(int32_t x, int32_t y, const char* text, Font font_id, Color color);
void render_text_aligned(int32_t x, int32_t y, Align horizontal_align, Align vertical_align, const char* text, Font font_id, Color color);
void render_scissor_start(BoundingBox* bb);
void render_scissor_end(void);
Dimension render_measure_text(const char* str, Font font_id);

#if defined(__cplusplus)
}
#endif
