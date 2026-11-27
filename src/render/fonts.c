#include "render_internal.h"

#include "core.h"
#include "text_layout.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum { FONT_BODY_PX = 24, FONT_TITLE_PX = 48, MAX_CODEPOINTS = 4096 };

static int cmp_int(const void *a, const void *b)
{
    const int ia = *(const int *)a;
    const int ib = *(const int *)b;
    return (ia > ib) - (ia < ib);
}

static bool resolve_asset_font_path(const char *rel, char *out, size_t cap)
{
    char exe[PATH_MAX];
    const ssize_t len = readlink("/proc/self/exe", exe, sizeof exe - 1);
    if (len <= 0 || (size_t)len >= sizeof exe - 1) {
        return false;
    }
    exe[len] = '\0';
    char *slash = strrchr(exe, '/');
    if (slash == NULL) {
        return false;
    }
    *slash = '\0';
    const int n = snprintf(out, cap, "%s/assets/fonts/%s", exe, rel);
    return n > 0 && (size_t)n < cap && access(out, F_OK) == 0;
}

static bool add_unique_codepoint(int *out, int *count, int cap, int cp)
{
    for (int i = 0; i < *count; ++i) {
        if (out[i] == cp) {
            return true;
        }
    }
    if (*count >= cap) {
        return false;
    }
    out[(*count)++] = cp;
    return true;
}

static int collect_codepoints(int *out, int cap)
{
    int count = 0;
    for (int cp = 0x20; cp <= 0x7e && count < cap; ++cp) {
        out[count++] = cp;
    }

    for (int id = (int)TEXT_NONE + 1; id < (int)TEXT_COUNT; ++id) {
        const char *ja = text_get((TextId)id, true);
        if (ja == NULL || ja[0] == '\0') {
            continue;
        }
        int cp_count = 0;
        int *cps = LoadCodepoints(ja, &cp_count);
        if (cps != NULL) {
            for (int i = 0; i < cp_count; ++i) {
                if (!add_unique_codepoint(out, &count, cap, cps[i])) {
                    break;
                }
            }
            UnloadCodepoints(cps);
        }
    }

    (void)add_unique_codepoint(out, &count, cap, 0x2026);
    if (count > 1) {
        qsort(out, (size_t)count, sizeof out[0], cmp_int);
    }
    return count;
}

void render_fonts_init(RenderState *r, const RenderOptions *opt)
{
    if (r == NULL) {
        return;
    }
    r->font_body = GetFontDefault();
    r->font_title = GetFontDefault();
    r->custom_fonts = false;
    r->owns_body_font = false;
    r->owns_title_font = false;
    r->title_is_brush = false;
    r->use_ja = false;

    const bool force_fault = opt != NULL && (opt->fault_mask & RENDER_FAULT_FONT) != 0;
    char resolved_body[PATH_MAX];
    const char *body_path = NULL;
    if (!force_fault) {
        if (opt != NULL && opt->font_path != NULL && opt->font_path[0] != '\0' &&
            access(opt->font_path, F_OK) == 0) {
            body_path = opt->font_path;
        } else if (resolve_asset_font_path("ui_mincho.ttf", resolved_body, sizeof resolved_body)) {
            body_path = resolved_body;
        }
    }

    if (force_fault || body_path == NULL) {
        TraceLog(LOG_WARNING, "INKSTORM: font unavailable; using built-in ASCII font");
        return;
    }

    int codepoints[MAX_CODEPOINTS];
    const int cp_count = collect_codepoints(codepoints, MAX_CODEPOINTS);
    const Font body = LoadFontEx(body_path, FONT_BODY_PX, codepoints, cp_count);
    const bool body_ok =
        IsFontValid(body) && body.texture.id != 0 && body.texture.id != GetFontDefault().texture.id;
    if (!body_ok) {
        if (body.texture.id != 0 && body.texture.id != GetFontDefault().texture.id) {
            UnloadFont(body);
        }
        TraceLog(LOG_WARNING, "INKSTORM: font unavailable; using built-in ASCII font");
        return;
    }

    SetTextureFilter(body.texture, TEXTURE_FILTER_BILINEAR);
    r->font_body = body;
    r->owns_body_font = true;
    r->custom_fonts = true;
    r->use_ja = true;

    /* Load brush title font with safe multi-tier fallback to UI Mincho */
    char resolved_title[PATH_MAX];
    const char *title_path = NULL;
    if (opt != NULL && opt->title_font_path != NULL && opt->title_font_path[0] != '\0' &&
        access(opt->title_font_path, F_OK) == 0) {
        title_path = opt->title_font_path;
    } else if (resolve_asset_font_path("title_brush.ttf", resolved_title, sizeof resolved_title)) {
        title_path = resolved_title;
    }

    Font title = {0};
    bool title_ok = false;
    if (title_path != NULL) {
        title = LoadFontEx(title_path, FONT_TITLE_PX, codepoints, cp_count);
        title_ok = IsFontValid(title) && title.texture.id != 0 &&
                   title.texture.id != GetFontDefault().texture.id;
        if (title_ok) {
            SetTextureFilter(title.texture, TEXTURE_FILTER_BILINEAR);
            r->font_title = title;
            r->owns_title_font = true;
            r->title_is_brush = true;
            TraceLog(LOG_INFO, "INKSTORM: loaded UI Mincho + Brush title fonts (%d glyphs)",
                     cp_count);
            return;
        }
        if (title.texture.id != 0 && title.texture.id != GetFontDefault().texture.id) {
            UnloadFont(title);
        }
    }

    /* Fallback tier 2: load UI Mincho at 48px for sharp titles without crashing */
    title = LoadFontEx(body_path, FONT_TITLE_PX, codepoints, cp_count);
    title_ok = IsFontValid(title) && title.texture.id != 0 &&
               title.texture.id != GetFontDefault().texture.id;
    if (title_ok) {
        SetTextureFilter(title.texture, TEXTURE_FILTER_BILINEAR);
        r->font_title = title;
        r->owns_title_font = true;
    } else {
        if (title.texture.id != 0 && title.texture.id != GetFontDefault().texture.id) {
            UnloadFont(title);
        }
        r->font_title = r->font_body;
        r->owns_title_font = false;
    }
    r->title_is_brush = false;
    TraceLog(LOG_WARNING,
             "INKSTORM: title brush font unavailable; using UI Mincho for titles (%d glyphs)",
             cp_count);
}

void render_fonts_close(RenderState *r)
{
    if (r == NULL) {
        return;
    }
    if (r->owns_title_font && r->font_title.texture.id != 0 &&
        r->font_title.texture.id != GetFontDefault().texture.id &&
        r->font_title.texture.id != r->font_body.texture.id) {
        UnloadFont(r->font_title);
    }
    if (r->owns_body_font && r->font_body.texture.id != 0 &&
        r->font_body.texture.id != GetFontDefault().texture.id) {
        UnloadFont(r->font_body);
    }
    r->font_body = GetFontDefault();
    r->font_title = GetFontDefault();
    r->custom_fonts = false;
    r->owns_body_font = false;
    r->owns_title_font = false;
    r->title_is_brush = false;
    r->use_ja = false;
    r->guide_cache.valid = false;
}

void render_set_lang_mode(RenderState *r, UiLangMode mode)
{
    if (r == NULL) {
        return;
    }
    r->use_ja = (mode == UI_LANG_EN) ? false : r->custom_fonts;
}

static Font pick_font(const RenderState *r, bool title_font)
{
    return title_font ? r->font_title : r->font_body;
}

static float spacing_for_size(float size) { return size >= 36.0f ? 2.0f : 1.0f; }

Vector2 render_measure_text(const RenderState *r, const char *text, float size, bool title_font)
{
    if (r == NULL || text == NULL || text[0] == '\0') {
        return (Vector2){0.0f, size};
    }
    return MeasureTextEx(pick_font(r, title_font), text, size, spacing_for_size(size));
}

typedef struct {
    const RenderState *r;
    float size;
    bool title_font;
} SpanMeasure;

enum { SPAN_BUF = 512 };

static size_t copy_span(char *buf, const char *text, size_t len)
{
    const size_t n = text_utf8_clip(text, len, SPAN_BUF - 1);
    memcpy(buf, text, n);
    buf[n] = '\0';
    return n;
}

static float measure_span(void *ctx, const char *text, size_t len)
{
    const SpanMeasure *m = ctx;
    char buf[SPAN_BUF];
    (void)copy_span(buf, text, len);
    return render_measure_text(m->r, buf, m->size, m->title_font).x;
}

static const char *ellipsis_for(const RenderState *r) { return r->use_ja ? "\xE2\x80\xA6" : "..."; }

static void ellipsize_into(const RenderState *r, char *out, const char *text, size_t len,
                           float max_w, float size, bool title_font)
{
    SpanMeasure m = {.r = r, .size = size, .title_font = title_font};
    const char *ell = ellipsis_for(r);
    const float ell_w = render_measure_text(r, ell, size, title_font).x;
    const size_t room = text_utf8_clip(text, len, SPAN_BUF - 1 - strlen(ell));
    const size_t keep =
        text_layout_fit_prefix(text, room, fmaxf(0.0f, max_w - ell_w), measure_span, &m);
    snprintf(out, SPAN_BUF, "%.*s%s", (int)keep, text, ell);
}

void render_draw_text_fit(const RenderState *r, const char *text, Vector2 center, float max_w,
                          float size, float min_size, PaletteId color, float alpha, bool title_font)
{
    if (r == NULL || text == NULL || text[0] == '\0') {
        return;
    }
    float s = size;
    while (s > min_size && render_measure_text(r, text, s, title_font).x > max_w) {
        s -= 1.0f;
    }
    if (render_measure_text(r, text, s, title_font).x <= max_w) {
        render_draw_text_centered(r, text, center, s, color, alpha, title_font);
        return;
    }
    char buf[SPAN_BUF];
    ellipsize_into(r, buf, text, strlen(text), max_w, s, title_font);
    render_draw_text_centered(r, buf, center, s, color, alpha, title_font);
}

float render_draw_text_wrapped(const RenderState *r, const char *text, Rectangle box, float size,
                               float min_size, PaletteId color, float alpha, bool title_font,
                               bool center)
{
    if (r == NULL || text == NULL || text[0] == '\0' || box.width <= 1.0f) {
        return 0.0f;
    }
    TextLine lines[TEXT_LAYOUT_MAX_LINES];
    SpanMeasure m = {.r = r, .size = size, .title_font = title_font};
    int32_t n = 0;
    bool truncated = false;
    float line_h = size * 1.2f;
    for (;;) {
        m.size = size;
        int32_t max_lines = (int32_t)(box.height / line_h);
        max_lines = max_lines < 1
                        ? 1
                        : (max_lines > TEXT_LAYOUT_MAX_LINES ? TEXT_LAYOUT_MAX_LINES : max_lines);
        n = text_layout_wrap(text, box.width, measure_span, &m, lines, max_lines, &truncated);
        if (!truncated || size <= min_size) {
            break;
        }
        size -= 1.0f;
        line_h = size * 1.2f;
    }
    const float total_h = (float)n * line_h;
    float y = box.y + fmaxf(0.0f, (box.height - total_h) * 0.5f);
    for (int32_t i = 0; i < n; ++i) {
        char buf[SPAN_BUF];
        if (truncated && i == n - 1) {
            ellipsize_into(r, buf, text + lines[i].start, lines[i].len, box.width, size,
                           title_font);
        } else {
            (void)copy_span(buf, text + lines[i].start, lines[i].len);
        }
        const float w = center ? render_measure_text(r, buf, size, title_font).x : 0.0f;
        const float x = center ? box.x + (box.width - w) * 0.5f : box.x;
        render_draw_text(r, buf, (Vector2){x, y}, size, color, alpha, title_font);
        y += line_h;
    }
    return total_h;
}

static void copy_into_cached_line(char dst[RENDER_CACHED_LINE_CAP], const char *src, size_t len)
{
    const size_t n = text_utf8_clip(src, len, RENDER_CACHED_LINE_CAP - 1);
    memcpy(dst, src, n);
    dst[n] = '\0';
}

void render_layout_text_fit(const RenderState *r, const char *text, Vector2 center, float max_w,
                            float size, float min_size, bool title_font, RenderCachedText *out)
{
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof *out);
    if (r == NULL || text == NULL || text[0] == '\0') {
        return;
    }
    const size_t src_len = strlen(text);
    const size_t keep_src = text_utf8_clip(text, src_len, sizeof out->src - 1);
    memcpy(out->src, text, keep_src);
    out->src[keep_src] = '\0';
    out->box = (Rectangle){center.x, center.y, max_w, 0.0f};
    out->req_size = size;
    out->min_size = min_size;
    out->title_font = title_font;
    out->center = true;
    out->is_wrapped = false;

    float s = size;
    Vector2 m = render_measure_text(r, text, s, title_font);
    while (s > min_size && m.x > max_w) {
        s -= 1.0f;
        m = render_measure_text(r, text, s, title_font);
    }
    if (m.x <= max_w) {
        copy_into_cached_line(out->lines[0], text, src_len);
    } else {
        char buf[SPAN_BUF];
        ellipsize_into(r, buf, text, src_len, max_w, s, title_font);
        copy_into_cached_line(out->lines[0], buf, strlen(buf));
        m = render_measure_text(r, out->lines[0], s, title_font);
    }
    out->x[0] = center.x - m.x * 0.5f;
    out->y[0] = center.y - m.y * 0.5f;
    out->font_size = s;
    out->line_count = 1;
}

void render_layout_text_wrapped(const RenderState *r, const char *text, Rectangle box, float size,
                                float min_size, bool title_font, bool center, RenderCachedText *out)
{
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof *out);
    if (r == NULL || text == NULL || text[0] == '\0' || box.width <= 1.0f) {
        return;
    }
    const size_t src_len = strlen(text);
    const size_t keep_src = text_utf8_clip(text, src_len, sizeof out->src - 1);
    memcpy(out->src, text, keep_src);
    out->src[keep_src] = '\0';
    out->box = box;
    out->req_size = size;
    out->min_size = min_size;
    out->title_font = title_font;
    out->center = center;
    out->is_wrapped = true;

    TextLine lines[TEXT_LAYOUT_MAX_LINES];
    SpanMeasure m = {.r = r, .size = size, .title_font = title_font};
    int32_t n = 0;
    bool truncated = false;
    float cur_size = size;
    float line_h = cur_size * 1.2f;
    for (;;) {
        m.size = cur_size;
        int32_t max_lines = (int32_t)(box.height / line_h);
        max_lines = max_lines < 1
                        ? 1
                        : (max_lines > TEXT_LAYOUT_MAX_LINES ? TEXT_LAYOUT_MAX_LINES : max_lines);
        n = text_layout_wrap(text, box.width, measure_span, &m, lines, max_lines, &truncated);
        if (!truncated || cur_size <= min_size) {
            break;
        }
        cur_size -= 1.0f;
        line_h = cur_size * 1.2f;
    }
    const float total_h = (float)n * line_h;
    float y = box.y + fmaxf(0.0f, (box.height - total_h) * 0.5f);
    for (int32_t i = 0; i < n; ++i) {
        char buf[SPAN_BUF];
        if (truncated && i == n - 1) {
            ellipsize_into(r, buf, text + lines[i].start, lines[i].len, box.width, cur_size,
                           title_font);
        } else {
            (void)copy_span(buf, text + lines[i].start, lines[i].len);
        }
        copy_into_cached_line(out->lines[i], buf, strlen(buf));
        const float w =
            center ? render_measure_text(r, out->lines[i], cur_size, title_font).x : 0.0f;
        out->x[i] = center ? box.x + (box.width - w) * 0.5f : box.x;
        out->y[i] = y;
        y += line_h;
    }
    out->font_size = cur_size;
    out->line_count = n;
}

void render_draw_cached_text(const RenderState *r, const RenderCachedText *ct, PaletteId color,
                             float alpha)
{
    if (r == NULL || ct == NULL || ct->line_count <= 0) {
        return;
    }
    for (int32_t i = 0; i < ct->line_count; ++i) {
        render_draw_text(r, ct->lines[i], (Vector2){ct->x[i], ct->y[i]}, ct->font_size, color,
                         alpha, ct->title_font);
    }
}

void render_draw_text(const RenderState *r, const char *text, Vector2 pos, float size,
                      PaletteId color, float alpha, bool title_font)
{
    if (r == NULL || text == NULL || text[0] == '\0' || alpha <= 0.001f) {
        return;
    }
    DrawTextEx(pick_font(r, title_font), text, pos, size, spacing_for_size(size),
               pal_color_alpha(color, alpha));
}

void render_draw_text_id(const RenderState *r, TextId id, Vector2 pos, float size, PaletteId color,
                         float alpha, bool title_font)
{
    if (r == NULL || (unsigned)id >= (unsigned)TEXT_COUNT) {
        return;
    }
    render_draw_text(r, text_get(id, r->use_ja), pos, size, color, alpha, title_font);
}

void render_draw_text_centered(const RenderState *r, const char *text, Vector2 center, float size,
                               PaletteId color, float alpha, bool title_font)
{
    if (r == NULL || text == NULL || text[0] == '\0') {
        return;
    }
    const Vector2 m = render_measure_text(r, text, size, title_font);
    render_draw_text(r, text, (Vector2){center.x - m.x * 0.5f, center.y - m.y * 0.5f}, size, color,
                     alpha, title_font);
}

static float fixed_digit_cell(Font font, float size)
{
    return MeasureTextEx(font, "0", size, 0.0f).x + 1.0f;
}

float render_measure_number_fixed(const RenderState *r, const char *digits, float size)
{
    if (r == NULL || digits == NULL) {
        return 0.0f;
    }
    const float cell_w = fixed_digit_cell(r->font_body, size);
    float w = 0.0f;
    for (const char *p = digits; *p != '\0'; ++p) {
        if (*p >= '0' && *p <= '9') {
            w += cell_w;
        } else {
            const char ch[2] = {*p, '\0'};
            w += MeasureTextEx(r->font_body, ch, size, 0.0f).x + 1.0f;
        }
    }
    return w;
}

void render_draw_number_fixed(const RenderState *r, const char *digits, Vector2 pos, float size,
                              PaletteId color, float alpha)
{
    if (r == NULL || digits == NULL || digits[0] == '\0') {
        return;
    }
    const Font font = r->font_body;
    const float cell_w = fixed_digit_cell(font, size);
    const Color c = pal_color_alpha(color, alpha);
    float x = pos.x;
    for (const char *p = digits; *p != '\0'; ++p) {
        char ch[2] = {*p, '\0'};
        const float cw = MeasureTextEx(font, ch, size, 0.0f).x;
        const float advance = (*p >= '0' && *p <= '9') ? cell_w : (cw + 1.0f);
        const float dx = (*p >= '0' && *p <= '9') ? (cell_w - cw) * 0.5f : 0.0f;
        DrawTextEx(font, ch, (Vector2){x + dx, pos.y}, size, 0.0f, c);
        x += advance;
    }
}
