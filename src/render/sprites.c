#include "sprites.h"

#include "png_rgba.h"
#include "rlgl.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { SPRITE_HALO_MAX = SPRITE_CANVAS_MAX / 2, SPRITE_PATH_MAX = 4096 };

/* 512² の RGBA でも 1 MiB 余りに収まる。これを超える入力は展開前に弾く */
#define SPRITE_FILE_MAX_BYTES (8L * 1024L * 1024L)

typedef enum { SPRITE_LOAD_ABSENT = 0, SPRITE_LOAD_REJECTED, SPRITE_LOAD_OK } SpriteLoadResult;

typedef struct {
    unsigned char *atlas;
    uint8_t *rgba;
    size_t rgba_cap;
    uint8_t *raw;
    size_t raw_cap;
    uint8_t *alpha;
    uint8_t *halo;
    uint8_t *scratch;
} SpriteLoadBuffers;

static unsigned char *read_whole_file(const char *path, int *size_out)
{
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        return NULL;
    }
    unsigned char *buf = NULL;
    long len = -1;
    if (fseek(fp, 0, SEEK_END) == 0) {
        len = ftell(fp);
    }
    if (len > 0 && len <= SPRITE_FILE_MAX_BYTES && fseek(fp, 0, SEEK_SET) == 0) {
        buf = malloc((size_t)len);
        if (buf != NULL && fread(buf, 1, (size_t)len, fp) != (size_t)len) {
            free(buf);
            buf = NULL;
        }
    }
    fclose(fp);
    if (buf != NULL) {
        *size_out = (int)len;
    }
    return buf;
}

static void blit_sprite(unsigned char *atlas, SpriteRect cell, const unsigned char *rgba)
{
    /* ImageDraw は合成するため、透明画素へ滲ませた RGB が失われてミップが黒ずむ */
    const size_t row_bytes = (size_t)cell.w * 4u;
    for (int32_t y = 0; y < cell.h; ++y) {
        const size_t dst = ((size_t)(cell.y + y) * SPRITE_ATLAS_PX + (size_t)cell.x) * 4u;
        memcpy(atlas + dst, rgba + (size_t)y * row_bytes, row_bytes);
    }
}

static void blit_halo(unsigned char *atlas, SpriteRect cell, const uint8_t *mask)
{
    for (int32_t y = 0; y < cell.h; ++y) {
        unsigned char *row = atlas + ((size_t)(cell.y + y) * SPRITE_ATLAS_PX + (size_t)cell.x) * 4u;
        for (int32_t x = 0; x < cell.w; ++x) {
            row[(size_t)x * 4u + 0u] = 255u;
            row[(size_t)x * 4u + 1u] = 255u;
            row[(size_t)x * 4u + 2u] = 255u;
            row[(size_t)x * 4u + 3u] = mask[(size_t)y * (size_t)cell.w + (size_t)x];
        }
    }
}

static SpriteLoadResult load_sprite(const SpriteLoadBuffers *buf, SpriteId id, const char *dir)
{
    const SpriteDef *d = sprite_def(id);
    char path[SPRITE_PATH_MAX];
    if (d == NULL || !sprite_join_path(dir, d->file, path, sizeof path) || !FileExists(path)) {
        return SPRITE_LOAD_ABSENT;
    }
    SpriteRect cell;
    SpriteRect halo_cell;
    if (d->canvas > SPRITE_CANVAS_MAX || !sprite_atlas_slot(id, false, &cell) ||
        !sprite_atlas_slot(id, true, &halo_cell)) {
        return SPRITE_LOAD_REJECTED;
    }

    /* 自前で読んで固定上限つき RGBA8 デコーダで展開し、外部デコーダの非有界確保を避ける */
    int size = 0;
    unsigned char *data = read_whole_file(path, &size);
    const bool decoded =
        data != NULL && png_rgba_decode(data, (size_t)size, d->canvas, d->canvas, buf->rgba,
                                        buf->rgba_cap, buf->raw, buf->raw_cap);
    free(data);
    if (!decoded || !sprite_alpha_usable_ex(buf->rgba, d->canvas, d->canvas, d->tile_x)) {
        return SPRITE_LOAD_REJECTED;
    }

    const unsigned char *rgba = buf->rgba;
    blit_sprite(buf->atlas, cell, rgba);
    const size_t texels = (size_t)d->canvas * (size_t)d->canvas;
    for (size_t i = 0; i < texels; ++i) {
        buf->alpha[i] = rgba[i * 4u + 3u];
    }

    int32_t pad = 0;
    int32_t feather = 0;
    sprite_halo_params(d, &pad, &feather);
    if (!sprite_halo_from_alpha_ex(buf->alpha, d->canvas, d->canvas, pad, feather, d->tile_x,
                                   buf->halo, buf->scratch)) {
        return SPRITE_LOAD_REJECTED;
    }
    blit_halo(buf->atlas, halo_cell, buf->halo);
    return SPRITE_LOAD_OK;
}

static Texture2D upload_atlas_cell_repeat(const unsigned char *atlas, SpriteRect cell,
                                          unsigned char *scratch)
{
    const size_t row_bytes = (size_t)cell.w * 4u;
    for (int32_t y = 0; y < cell.h; ++y) {
        const size_t src = ((size_t)(cell.y + y) * SPRITE_ATLAS_PX + (size_t)cell.x) * 4u;
        memcpy(scratch + (size_t)y * row_bytes, atlas + src, row_bytes);
    }
    const Image img = {.data = scratch,
                       .width = cell.w,
                       .height = cell.h,
                       .mipmaps = 1,
                       .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    Texture2D tex = LoadTextureFromImage(img);
    if (tex.id != 0) {
        GenTextureMipmaps(&tex);
        SetTextureFilter(tex, TEXTURE_FILTER_TRILINEAR);
        SetTextureWrap(tex, TEXTURE_WRAP_REPEAT);
    }
    return tex;
}

static void load_backgrounds(SpriteBank *bank, const char *dir, int32_t *rejected)
{
    const size_t bg_rgba_bytes = (size_t)SPRITE_BG_W * (size_t)SPRITE_BG_H * 4u;
    const size_t bg_raw_bytes = ((size_t)SPRITE_BG_W * 4u + 1u) * (size_t)SPRITE_BG_H;
    uint8_t *bg_work = malloc(bg_rgba_bytes + bg_raw_bytes);
    if (bg_work == NULL) {
        TraceLog(LOG_WARNING,
                 "INKSTORM: background buffer allocation failed; drawing procedural scenery");
        return;
    }
    uint8_t *bg_rgba = bg_work;
    uint8_t *bg_raw = bg_work + bg_rgba_bytes;
    for (int32_t k = 0; k < SPR_BG_COUNT; ++k) {
        const char *file = sprite_bg_file((SpriteBgId)k);
        char path[SPRITE_PATH_MAX];
        if (file == NULL || !sprite_join_path(dir, file, path, sizeof path) || !FileExists(path)) {
            continue;
        }
        int size = 0;
        unsigned char *data = read_whole_file(path, &size);
        const bool decoded =
            data != NULL && png_rgba_decode(data, (size_t)size, SPRITE_BG_W, SPRITE_BG_H, bg_rgba,
                                            bg_rgba_bytes, bg_raw, bg_raw_bytes);
        free(data);
        if (!decoded || !sprite_bg_alpha_usable(bg_rgba, SPRITE_BG_W, SPRITE_BG_H)) {
            ++(*rejected);
            TraceLog(LOG_WARNING, "INKSTORM: background rejected (%s)", file);
            continue;
        }
        const Image img = {.data = bg_rgba,
                           .width = SPRITE_BG_W,
                           .height = SPRITE_BG_H,
                           .mipmaps = 1,
                           .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
        Texture2D bg_tex = LoadTextureFromImage(img);
        if (bg_tex.id != 0) {
            SetTextureFilter(bg_tex, TEXTURE_FILTER_BILINEAR);
            SetTextureWrap(bg_tex, TEXTURE_WRAP_CLAMP);
            bank->bg_tex[k] = bg_tex;
            ++bank->bgs_loaded;
        }
    }
    free(bg_work);
}

static void finish_sprites_init(SpriteBank *bank, const char *dir, double t0, int32_t loaded_core,
                                int32_t core_total, int32_t rejected, int32_t fallback_groups)
{
    load_backgrounds(bank, dir, &rejected);
    bank->load_ms = (GetTime() - t0) * 1000.0;
    TraceLog(LOG_INFO,
             "INKSTORM: sprites %d/%d (%d rejected; procedural fallback for %d groups; "
             "bg %d/%d; %.1f ms)",
             (int)loaded_core, (int)core_total, (int)rejected, (int)fallback_groups,
             (int)bank->bgs_loaded, (int)SPR_BG_COUNT, bank->load_ms);
}

void render_sprites_init(RenderState *r, const RenderOptions *opt)
{
    if (r == NULL) {
        return;
    }
    SpriteBank *bank = &r->sprites;
    memset(bank, 0, sizeof *bank);
    const double t0 = GetTime();
    const int32_t core_total = sprite_core_count();
    if (opt != NULL && (opt->fault_mask & RENDER_FAULT_SPRITES) != 0u) {
        TraceLog(LOG_WARNING, "INKSTORM: sprites disabled by debug fault; drawing procedural ink");
        return;
    }
    if (opt == NULL || opt->sprite_dir == NULL || opt->sprite_dir[0] == '\0') {
        TraceLog(LOG_INFO, "INKSTORM: sprites 0/%d (assets/sprites not found; procedural ink)",
                 (int)core_total);
        return;
    }

    const size_t atlas_bytes = (size_t)SPRITE_ATLAS_PX * SPRITE_ATLAS_PX * 4u;
    const size_t canvas_texels = (size_t)SPRITE_CANVAS_MAX * SPRITE_CANVAS_MAX;
    const size_t rgba_bytes = canvas_texels * 4u;
    const size_t raw_bytes = ((size_t)SPRITE_CANVAS_MAX * 4u + 1u) * (size_t)SPRITE_CANVAS_MAX;
    const size_t halo_texels = (size_t)SPRITE_HALO_MAX * SPRITE_HALO_MAX;
    unsigned char *atlas = calloc(1, atlas_bytes);
    uint8_t *work = malloc(rgba_bytes + raw_bytes + canvas_texels + 3u * halo_texels);
    if (atlas == NULL || work == NULL) {
        free(atlas);
        free(work);
        TraceLog(LOG_WARNING, "INKSTORM: sprite atlas allocation failed; drawing procedural ink");
        finish_sprites_init(bank, opt->sprite_dir, t0, 0, core_total, 0, SPR_GROUP_COUNT);
        return;
    }
    const SpriteLoadBuffers buf = {
        .atlas = atlas,
        .rgba = work,
        .rgba_cap = rgba_bytes,
        .raw = work + rgba_bytes,
        .raw_cap = raw_bytes,
        .alpha = work + rgba_bytes + raw_bytes,
        .halo = work + rgba_bytes + raw_bytes + canvas_texels,
        .scratch = work + rgba_bytes + raw_bytes + canvas_texels + halo_texels,
    };

    int32_t loaded_core = 0;
    int32_t rejected = 0;
    for (int32_t i = 0; i < SPR_COUNT; ++i) {
        const SpriteLoadResult res = load_sprite(&buf, (SpriteId)i, opt->sprite_dir);
        bank->present[i] = res == SPRITE_LOAD_OK;
        if (res == SPRITE_LOAD_REJECTED) {
            ++rejected;
            TraceLog(LOG_WARNING, "INKSTORM: sprite rejected (%s)", sprite_def((SpriteId)i)->file);
        }
        if (bank->present[i] && !sprite_def((SpriteId)i)->optional) {
            ++loaded_core;
        }
        bank->loaded += bank->present[i] ? 1 : 0;
    }
    int32_t groups_ok = sprite_groups_resolve(bank->present, bank->group_ok);

    Texture2D tex = {0};
    Texture2D band_tex = {0};
    Texture2D band_halo_tex = {0};
    if (groups_ok > 0) {
        const Image atlas_img = {.data = atlas,
                                 .width = SPRITE_ATLAS_PX,
                                 .height = SPRITE_ATLAS_PX,
                                 .mipmaps = 1,
                                 .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
        tex = LoadTextureFromImage(atlas_img);
    }
    if (tex.id != 0 && bank->present[SPR_RYU_BODY]) {
        SpriteRect body_cell;
        SpriteRect halo_cell;
        if (sprite_atlas_slot(SPR_RYU_BODY, false, &body_cell) &&
            sprite_atlas_slot(SPR_RYU_BODY, true, &halo_cell)) {
            band_tex = upload_atlas_cell_repeat(atlas, body_cell, work);
            band_halo_tex = upload_atlas_cell_repeat(atlas, halo_cell, work);
        }
        if (band_tex.id == 0 || band_halo_tex.id == 0) {
            if (band_tex.id != 0) {
                UnloadTexture(band_tex);
                band_tex = (Texture2D){0};
            }
            if (band_halo_tex.id != 0) {
                UnloadTexture(band_halo_tex);
                band_halo_tex = (Texture2D){0};
            }
            bank->present[SPR_RYU_BODY] = false;
            --loaded_core;
            --bank->loaded;
            groups_ok = sprite_groups_resolve(bank->present, bank->group_ok);
            if (groups_ok == 0) {
                UnloadTexture(tex);
                tex = (Texture2D){0};
            }
        }
    }
    free(work);
    free(atlas);
    if (tex.id == 0) {
        memset(bank, 0, sizeof *bank);
        finish_sprites_init(bank, opt->sprite_dir, t0, loaded_core, core_total, rejected,
                            SPR_GROUP_COUNT);
        return;
    }
    GenTextureMipmaps(&tex);
    SetTextureFilter(tex, TEXTURE_FILTER_TRILINEAR);
    SetTextureWrap(tex, TEXTURE_WRAP_CLAMP);
    bank->atlas = tex;
    bank->ryu_band_tex = band_tex;
    bank->ryu_band_halo_tex = band_halo_tex;
    bank->enabled = true;
    bank->groups_ok = groups_ok;
    finish_sprites_init(bank, opt->sprite_dir, t0, loaded_core, core_total, rejected,
                        SPR_GROUP_COUNT - groups_ok);
}

void render_sprites_close(RenderState *r)
{
    if (r == NULL) {
        return;
    }
    if (r->sprites.atlas.id != 0) {
        UnloadTexture(r->sprites.atlas);
    }
    if (r->sprites.ryu_band_tex.id != 0) {
        UnloadTexture(r->sprites.ryu_band_tex);
    }
    if (r->sprites.ryu_band_halo_tex.id != 0) {
        UnloadTexture(r->sprites.ryu_band_halo_tex);
    }
    for (int32_t k = 0; k < SPR_BG_COUNT; ++k) {
        if (r->sprites.bg_tex[k].id != 0) {
            UnloadTexture(r->sprites.bg_tex[k]);
        }
    }
    memset(&r->sprites, 0, sizeof r->sprites);
}

SpriteId render_sprite_resolve(const RenderState *r, SpriteId id)
{
    if (r == NULL || !r->sprites.enabled) {
        return SPR_NONE;
    }
    return sprite_resolve(id, r->sprites.present, r->sprites.group_ok);
}

static void draw_cell(RenderState *r, SpriteId id, bool halo, const SpriteXform *xf, Color tint)
{
    if (r == NULL || xf == NULL || tint.a == 0 || !r->sprites.enabled || r->sprites.atlas.id == 0 ||
        id < 0 || id >= SPR_COUNT || !r->sprites.present[id]) {
        return;
    }
    const SpriteDef *d = sprite_def(id);
    SpriteRect cell;
    if (!sprite_atlas_slot(id, halo, &cell)) {
        return;
    }
    const float inv = 1.0f / (float)SPRITE_ATLAS_PX;
    SpriteQuad q;
    sprite_quad(d, xf, (float)cell.x * inv, (float)cell.y * inv, (float)(cell.x + cell.w) * inv,
                (float)(cell.y + cell.h) * inv, &q);
    rlCheckRenderBatchLimit(4);
    rlSetTexture(r->sprites.atlas.id);
    rlBegin(RL_QUADS);
    rlColor4ub(tint.r, tint.g, tint.b, tint.a);
    rlNormal3f(0.0f, 0.0f, 1.0f);
    for (int k = 0; k < 4; ++k) {
        rlTexCoord2f(q.u[k], q.v[k]);
        rlVertex2f(q.x[k], q.y[k]);
    }
    rlEnd();
    rlSetTexture(rlGetTextureIdDefault());
    r->metrics.prim_verts += 4;
}

void render_sprite_draw(RenderState *r, SpriteId id, const SpriteXform *xf, Color tint)
{
    draw_cell(r, id, false, xf, tint);
}

void render_sprite_draw_halo(RenderState *r, SpriteId id, const SpriteXform *xf, Color tint)
{
    draw_cell(r, id, true, xf, tint);
}

bool render_sprite_draw_bg(RenderState *r, SpriteBgId id)
{
    if (r == NULL || id < 0 || id >= SPR_BG_COUNT || r->sprites.bg_tex[id].id == 0) {
        return false;
    }
    const Texture2D tex = r->sprites.bg_tex[id];
    const Rectangle src = {0.0f, 0.0f, (float)tex.width, (float)tex.height};
    const Rectangle dst = {-(float)RENDER_BG_PAD, -(float)RENDER_BG_PAD,
                           (float)(INK_SCREEN_W + 2 * RENDER_BG_PAD),
                           (float)(INK_SCREEN_H + 2 * RENDER_BG_PAD)};
    DrawTexturePro(tex, src, dst, (Vector2){0.0f, 0.0f}, 0.0f, pal_white());
    return true;
}

static void emit_band_tri(RenderState *r, unsigned int tex_id, Color tint, GameVec2 p0, GameVec2 p1,
                          GameVec2 p2, float u0, float v0, float u1, float v1, float u2, float v2)
{
    const float cross = (p1.x - p0.x) * (p2.y - p0.y) - (p1.y - p0.y) * (p2.x - p0.x);
    if (!isfinite(cross) || fabsf(cross) <= 1e-4f) {
        return;
    }
    if (cross > 0.0f) {
        const GameVec2 tp = p1;
        const float tu = u1;
        const float tv = v1;
        p1 = p2;
        u1 = u2;
        v1 = v2;
        p2 = tp;
        u2 = tu;
        v2 = tv;
    }
    rlCheckRenderBatchLimit(3);
    rlSetTexture(tex_id);
    rlBegin(RL_TRIANGLES);
    rlColor4ub(tint.r, tint.g, tint.b, tint.a);
    rlNormal3f(0.0f, 0.0f, 1.0f);
    rlTexCoord2f(u0, v0);
    rlVertex2f(p0.x, p0.y);
    rlTexCoord2f(u1, v1);
    rlVertex2f(p1.x, p1.y);
    rlTexCoord2f(u2, v2);
    rlVertex2f(p2.x, p2.y);
    rlEnd();
    r->metrics.prim_verts += 3;
}

static void emit_band_quad_tris(RenderState *r, unsigned int tex_id, Color tint,
                                const GameVec2 pts[4], const float qu[4], const float qv[4])
{
    emit_band_tri(r, tex_id, tint, pts[0], pts[1], pts[2], qu[0], qv[0], qu[1], qv[1], qu[2],
                  qv[2]);
    emit_band_tri(r, tex_id, tint, pts[0], pts[2], pts[3], qu[0], qv[0], qu[2], qv[2], qu[3],
                  qv[3]);
}

static GameVec2 vec2_lerp(GameVec2 a, GameVec2 b, float t)
{
    return (GameVec2){a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
}

enum { RYU_BAND_CAP = RENDER_RYU_SPINE_PTS + 1 };

void render_sprite_draw_ryu_band(RenderState *r, SpriteId id, bool halo, const GameVec2 *spine,
                                 int32_t spine_n, bool awakened, float time_s, bool flip_v,
                                 float reveal, Color tint)
{
    if (r == NULL || spine == NULL || spine_n < 2 || tint.a == 0 || !r->sprites.enabled ||
        id != SPR_RYU_BODY || !r->sprites.present[id] || !isfinite(reveal) || reveal <= 0.01f) {
        return;
    }
    const Texture2D band_tex = halo ? r->sprites.ryu_band_halo_tex : r->sprites.ryu_band_tex;
    if (band_tex.id == 0) {
        return;
    }
    const int32_t n = spine_n > RENDER_RYU_SPINE_PTS ? RENDER_RYU_SPINE_PTS : spine_n;
    float arc[RENDER_RYU_SPINE_PTS];
    arc[0] = 0.0f;
    for (int32_t i = 1; i < n; ++i) {
        arc[i] = arc[i - 1] + hypotf(spine[i].x - spine[i - 1].x, spine[i].y - spine[i - 1].y);
    }
    const float total_len = arc[n - 1];
    if (!(total_len > 1e-3f) || !isfinite(total_len)) {
        return;
    }

    const float clamped_reveal = reveal > 1.0f ? 1.0f : reveal;
    GameVec2 vis[RYU_BAND_CAP];
    float s_val[RYU_BAND_CAP];
    int32_t vis_n = 0;
    if (clamped_reveal >= 0.9999f) {
        vis_n = n;
        for (int32_t i = 0; i < n; ++i) {
            vis[i] = spine[i];
            s_val[i] = arc[i] / total_len;
        }
    } else {
        const float vis_len = total_len * clamped_reveal;
        vis[0] = spine[0];
        s_val[0] = 0.0f;
        vis_n = 1;
        for (int32_t i = 1; i < n; ++i) {
            if (arc[i] < vis_len) {
                vis[vis_n] = spine[i];
                s_val[vis_n] = arc[i] / total_len;
                ++vis_n;
            } else {
                const float seg_l = arc[i] - arc[i - 1];
                const float frac = seg_l > 1e-6f ? (vis_len - arc[i - 1]) / seg_l : 0.0f;
                if (frac > 1e-5f || vis_n < 2) {
                    vis[vis_n] = vec2_lerp(spine[i - 1], spine[i], frac);
                    s_val[vis_n] = clamped_reveal;
                    ++vis_n;
                }
                break;
            }
        }
    }
    if (vis_n < 2) {
        return;
    }

    /* 首元の切り口が頭の鬣の下へ自然に潜るよう、先頭を頭側へわずかに差し込む */
    bool has_tuck = false;
    const float ndx = vis[0].x - vis[1].x;
    const float ndy = vis[0].y - vis[1].y;
    const float ndl = hypotf(ndx, ndy);
    if (ndl > 1e-4f && clamped_reveal > 0.10f) {
        const float tuck_px = 22.0f * (clamped_reveal < 0.25f ? clamped_reveal / 0.25f : 1.0f);
        for (int32_t i = vis_n; i > 0; --i) {
            vis[i] = vis[i - 1];
            s_val[i] = s_val[i - 1];
        }
        vis[0] = (GameVec2){vis[1].x + (ndx / ndl) * tuck_px, vis[1].y + (ndy / ndl) * tuck_px};
        s_val[0] = 0.0f;
        ++vis_n;
        has_tuck = true;
    }

    const float dsign = flip_v ? -1.0f : 1.0f;
    GameVec2 last_fwd = {flip_v ? -1.0f : 1.0f, 0.0f};
    for (int32_t i = 0; i < vis_n - 1; ++i) {
        const float dx = vis[i].x - vis[i + 1].x;
        const float dy = vis[i].y - vis[i + 1].y;
        const float len = hypotf(dx, dy);
        if (len > 1e-5f) {
            last_fwd = (GameVec2){dx / len, dy / len};
            break;
        }
    }

    float seg_len[RYU_BAND_CAP];
    GameVec2 seg_fwd[RYU_BAND_CAP];
    GameVec2 seg_nrm[RYU_BAND_CAP];
    for (int32_t i = 0; i < vis_n - 1; ++i) {
        const float dx = vis[i].x - vis[i + 1].x;
        const float dy = vis[i].y - vis[i + 1].y;
        const float len = hypotf(dx, dy);
        if (len > 1e-5f) {
            last_fwd = (GameVec2){dx / len, dy / len};
        }
        seg_len[i] = len;
        seg_fwd[i] = last_fwd;
        seg_nrm[i] = (GameVec2){dsign * last_fwd.y, -dsign * last_fwd.x};
    }

    /* 75° 超の急屈曲（ヘアピン折り返し含む）を検出し、突き出しを防ぎつつ外側の扇で継ぐ */
    static const float k_fold_cos = 0.258819045f;
    bool is_fold[RYU_BAND_CAP] = {false};
    float fold_scale[RYU_BAND_CAP];
    GameVec2 vert_fwd[RYU_BAND_CAP];
    vert_fwd[0] = seg_fwd[0];
    vert_fwd[vis_n - 1] = seg_fwd[vis_n - 2];
    fold_scale[0] = 1.0f;
    fold_scale[vis_n - 1] = 1.0f;
    for (int32_t i = 1; i < vis_n - 1; ++i) {
        const float turn_cos = seg_fwd[i - 1].x * seg_fwd[i].x + seg_fwd[i - 1].y * seg_fwd[i].y;
        const int32_t ia = i >= 2 ? i - 2 : 0;
        const int32_t ib = i + 1 < vis_n - 1 ? i + 1 : vis_n - 2;
        const float span_cos = seg_fwd[ia].x * seg_fwd[ib].x + seg_fwd[ia].y * seg_fwd[ib].y;
        const float min_cos = turn_cos < span_cos ? turn_cos : span_cos;
        if (min_cos < 0.45f) {
            const float kt = (min_cos + 1.0f) / 1.45f;
            const float kc = kt < 0.0f ? 0.0f : (kt > 1.0f ? 1.0f : kt);
            const float ks = kc * kc * (3.0f - 2.0f * kc);
            fold_scale[i] = 0.38f + 0.62f * ks;
        } else {
            fold_scale[i] = 1.0f;
        }
        if (turn_cos < k_fold_cos) {
            is_fold[i] = true;
            vert_fwd[i] = seg_fwd[i];
        } else {
            const float fx = seg_fwd[i - 1].x + seg_fwd[i].x;
            const float fy = seg_fwd[i - 1].y + seg_fwd[i].y;
            const float fl = hypotf(fx, fy);
            vert_fwd[i] = fl > 1e-4f ? (GameVec2){fx / fl, fy / fl} : seg_fwd[i];
        }
    }
    for (int pass = 0; pass < 2; ++pass) {
        GameVec2 smooth_fwd[RYU_BAND_CAP];
        float smooth_fs[RYU_BAND_CAP];
        smooth_fwd[0] = vert_fwd[0];
        smooth_fwd[vis_n - 1] = vert_fwd[vis_n - 1];
        smooth_fs[0] = fold_scale[0];
        smooth_fs[vis_n - 1] = fold_scale[vis_n - 1];
        for (int32_t i = 1; i < vis_n - 1; ++i) {
            smooth_fs[i] =
                0.25f * fold_scale[i - 1] + 0.50f * fold_scale[i] + 0.25f * fold_scale[i + 1];
            if (is_fold[i] || is_fold[i - 1] || is_fold[i + 1]) {
                smooth_fwd[i] = vert_fwd[i];
                continue;
            }
            const float fx =
                0.25f * vert_fwd[i - 1].x + 0.50f * vert_fwd[i].x + 0.25f * vert_fwd[i + 1].x;
            const float fy =
                0.25f * vert_fwd[i - 1].y + 0.50f * vert_fwd[i].y + 0.25f * vert_fwd[i + 1].y;
            const float fl = hypotf(fx, fy);
            smooth_fwd[i] = fl > 1e-4f ? (GameVec2){fx / fl, fy / fl} : vert_fwd[i];
        }
        memcpy(vert_fwd, smooth_fwd, (size_t)vis_n * sizeof(GameVec2));
        memcpy(fold_scale, smooth_fs, (size_t)vis_n * sizeof(float));
    }

    /* 縁パスは本体中心 v = 0.53716 を軸に V 範囲と帯幅を同率で広げ、背腹のぼかしを切らない */
    static const float k_body_v_min = 0.265f;
    static const float k_body_v_max = 0.745f;
    static const float k_body_v_span = 0.480f;
    static const float k_v_center = 0.265f + 0.567f * 0.480f;
    const float v_min = halo ? 0.185f : k_body_v_min;
    const float v_max = halo ? 0.825f : k_body_v_max;
    const float dorsal_ratio = (k_v_center - v_min) / k_body_v_span;
    const float ventral_ratio = (v_max - k_v_center) / k_body_v_span;

    float band_w[RYU_BAND_CAP];
    GameVec2 top_pt[RYU_BAND_CAP];
    GameVec2 bot_pt[RYU_BAND_CAP];
    float tile_u[RYU_BAND_CAP];
    for (int32_t i = 0; i < vis_n; ++i) {
        const GameVec2 dorsal = {dsign * vert_fwd[i].y, -dsign * vert_fwd[i].x};
        float miter_scale = 1.0f;
        if (i > 0 && i < vis_n - 1 && !is_fold[i] && fold_scale[i] > 0.90f) {
            const float dot = dorsal.x * seg_nrm[i].x + dorsal.y * seg_nrm[i].y;
            const float denom = dot > 0.72f ? dot : 0.72f;
            miter_scale = 1.0f / denom;
            if (miter_scale > 1.38f) {
                miter_scale = 1.38f;
            }
        }
        band_w[i] = sprite_ryu_body_width(awakened, time_s, s_val[i]) * fold_scale[i];
        if (has_tuck && i == 0) {
            band_w[i] *= 0.85f;
        }
        const float eff_w = band_w[i] * miter_scale;
        top_pt[i] = (GameVec2){vis[i].x + dorsal.x * eff_w * dorsal_ratio,
                               vis[i].y + dorsal.y * eff_w * dorsal_ratio};
        bot_pt[i] = (GameVec2){vis[i].x - dorsal.x * eff_w * ventral_ratio,
                               vis[i].y - dorsal.y * eff_w * ventral_ratio};
        if (i == 0) {
            tile_u[0] = 0.0f;
        } else {
            const float base_w = 0.5f * (sprite_ryu_body_width(awakened, time_s, s_val[i - 1]) +
                                         sprite_ryu_body_width(awakened, time_s, s_val[i]));
            tile_u[i] =
                tile_u[i - 1] + (base_w > 1e-3f ? seg_len[i - 1] / (base_w * 2.083f) : 0.0f);
        }
    }

    /* 内側の曲がりで縁の頂点が追い越した区間は中点へ寄せ、面の裏返り・段差を防ぐ */
    for (int32_t i = 0; i < vis_n - 1; ++i) {
        if (is_fold[i] || is_fold[i + 1]) {
            continue;
        }
        if ((top_pt[i].x - top_pt[i + 1].x) * seg_fwd[i].x +
                (top_pt[i].y - top_pt[i + 1].y) * seg_fwd[i].y <
            0.0f) {
            const GameVec2 mid = vec2_lerp(top_pt[i], top_pt[i + 1], 0.5f);
            top_pt[i] = mid;
            top_pt[i + 1] = mid;
        }
        if ((bot_pt[i].x - bot_pt[i + 1].x) * seg_fwd[i].x +
                (bot_pt[i].y - bot_pt[i + 1].y) * seg_fwd[i].y <
            0.0f) {
            const GameVec2 mid = vec2_lerp(bot_pt[i], bot_pt[i + 1], 0.5f);
            bot_pt[i] = mid;
            bot_pt[i + 1] = mid;
        }
    }

    const unsigned int tex_id = band_tex.id;
    for (int32_t i = vis_n - 2; i >= 0; --i) {
        const float du = tile_u[i + 1] - tile_u[i];
        if (seg_len[i] > 1e-5f && du > 1e-6f) {
            const GameVec2 seg_top_a =
                is_fold[i] ? (GameVec2){vis[i].x + seg_nrm[i].x * band_w[i] * dorsal_ratio,
                                        vis[i].y + seg_nrm[i].y * band_w[i] * dorsal_ratio}
                           : top_pt[i];
            const GameVec2 seg_bot_a =
                is_fold[i] ? (GameVec2){vis[i].x - seg_nrm[i].x * band_w[i] * ventral_ratio,
                                        vis[i].y - seg_nrm[i].y * band_w[i] * ventral_ratio}
                           : bot_pt[i];
            const GameVec2 seg_top_b =
                is_fold[i + 1]
                    ? (GameVec2){vis[i + 1].x + seg_nrm[i].x * band_w[i + 1] * dorsal_ratio,
                                 vis[i + 1].y + seg_nrm[i].y * band_w[i + 1] * dorsal_ratio}
                    : top_pt[i + 1];
            const GameVec2 seg_bot_b =
                is_fold[i + 1]
                    ? (GameVec2){vis[i + 1].x - seg_nrm[i].x * band_w[i + 1] * ventral_ratio,
                                 vis[i + 1].y - seg_nrm[i].y * band_w[i + 1] * ventral_ratio}
                    : bot_pt[i + 1];

            const float ua = 1.0f - tile_u[i];
            const float ub = 1.0f - tile_u[i + 1];
            const GameVec2 qpts[4] = {seg_top_a, seg_bot_a, seg_bot_b, seg_top_b};
            const float qu[4] = {ua, ua, ub, ub};
            const float qv[4] = {v_min, v_max, v_max, v_min};
            emit_band_quad_tris(r, tex_id, tint, qpts, qu, qv);
        }

        if (i > 0 && is_fold[i] && seg_len[i - 1] > 1e-5f && seg_len[i] > 1e-5f) {
            const float bend_dot =
                dsign * (seg_fwd[i - 1].x * seg_fwd[i].y - seg_fwd[i - 1].y * seg_fwd[i].x);
            const bool dorsal_outer = bend_dot <= 0.0f;
            const float sign_out = dorsal_outer ? 1.0f : -1.0f;
            const float outer_r = band_w[i] * (dorsal_outer ? dorsal_ratio : ventral_ratio);
            const float v_out = dorsal_outer ? v_min : v_max;
            const GameVec2 out_a = {vis[i].x + sign_out * seg_nrm[i - 1].x * outer_r,
                                    vis[i].y + sign_out * seg_nrm[i - 1].y * outer_r};
            const GameVec2 out_b = {vis[i].x + sign_out * seg_nrm[i].x * outer_r,
                                    vis[i].y + sign_out * seg_nrm[i].y * outer_r};
            const float bx = sign_out * (seg_nrm[i - 1].x + seg_nrm[i].x);
            const float by = sign_out * (seg_nrm[i - 1].y + seg_nrm[i].y);
            const float bl = hypotf(bx, by);
            const GameVec2 mid_dir = bl > 1e-3f ? (GameVec2){bx / bl, by / bl}
                                                : (GameVec2){-seg_fwd[i - 1].x, -seg_fwd[i - 1].y};
            const GameVec2 out_mid = {vis[i].x + mid_dir.x * outer_r,
                                      vis[i].y + mid_dir.y * outer_r};
            const float ui = 1.0f - tile_u[i];
            emit_band_tri(r, tex_id, tint, vis[i], out_a, out_mid, ui, k_v_center, ui, v_out, ui,
                          v_out);
            emit_band_tri(r, tex_id, tint, vis[i], out_mid, out_b, ui, k_v_center, ui, v_out, ui,
                          v_out);
        }
    }
    rlSetTexture(rlGetTextureIdDefault());
}
