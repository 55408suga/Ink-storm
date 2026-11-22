#include "render_internal.h"

#include "core.h"

#include <limits.h>
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
    memset(r->text_size_valid, 0, sizeof r->text_size_valid);

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

Vector2 render_measure_text_id(RenderState *r, TextId id, float size, bool title_font)
{
    if (r == NULL || (unsigned)id >= (unsigned)TEXT_COUNT) {
        return (Vector2){0.0f, size};
    }
    const int slot = title_font ? 1 : 0;
    const float base_px = title_font ? (float)FONT_TITLE_PX : (float)FONT_BODY_PX;
    if (!r->text_size_valid[id][slot]) {
        const char *txt = text_get(id, r->use_ja);
        r->text_size_cache[id][slot] =
            MeasureTextEx(pick_font(r, title_font), txt, base_px, spacing_for_size(base_px));
        r->text_size_valid[id][slot] = true;
    }
    const float ratio = size / base_px;
    return (Vector2){
        r->text_size_cache[id][slot].x * ratio,
        r->text_size_cache[id][slot].y * ratio,
    };
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

void render_draw_number_fixed(const RenderState *r, const char *digits, Vector2 pos, float size,
                              PaletteId color, float alpha)
{
    if (r == NULL || digits == NULL || digits[0] == '\0') {
        return;
    }
    const Font font = r->font_body;
    const float cell_w = MeasureTextEx(font, "0", size, 0.0f).x + 1.0f;
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
