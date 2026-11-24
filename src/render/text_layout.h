#ifndef INK_RENDER_TEXT_LAYOUT_H
#define INK_RENDER_TEXT_LAYOUT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TEXT_LAYOUT_MAX_LINES 8

typedef float (*TextMeasureFn)(void *ctx, const char *text, size_t len);

typedef struct {
    size_t start;
    size_t len;
} TextLine;

size_t text_utf8_next(const char *s, size_t i, size_t n);
size_t text_layout_fit_prefix(const char *text, size_t len, float max_w, TextMeasureFn measure,
                              void *ctx);
int32_t text_layout_wrap(const char *text, float max_w, TextMeasureFn measure, void *ctx,
                         TextLine *lines, int32_t max_lines, bool *truncated);

#endif
