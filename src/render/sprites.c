#include "sprites.h"

#include "png_rgba.h"
#include "rlgl.h"

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
    if (!decoded || !sprite_alpha_usable(buf->rgba, d->canvas, d->canvas)) {
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
    if (!sprite_halo_from_alpha(buf->alpha, d->canvas, d->canvas, pad, feather, buf->halo,
                                buf->scratch)) {
        return SPRITE_LOAD_REJECTED;
    }
    blit_halo(buf->atlas, halo_cell, buf->halo);
    return SPRITE_LOAD_OK;
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
        rejected += res == SPRITE_LOAD_REJECTED ? 1 : 0;
        if (bank->present[i] && !sprite_def((SpriteId)i)->optional) {
            ++loaded_core;
        }
        bank->loaded += bank->present[i] ? 1 : 0;
    }
    free(work);
    const int32_t groups_ok = sprite_groups_resolve(bank->present, bank->group_ok);

    Texture2D tex = {0};
    if (groups_ok > 0) {
        const Image atlas_img = {.data = atlas,
                                 .width = SPRITE_ATLAS_PX,
                                 .height = SPRITE_ATLAS_PX,
                                 .mipmaps = 1,
                                 .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
        tex = LoadTextureFromImage(atlas_img);
    }
    free(atlas);
    if (tex.id == 0) {
        memset(bank, 0, sizeof *bank);
        TraceLog(LOG_INFO,
                 "INKSTORM: sprites %d/%d (%d rejected; procedural fallback for %d groups)",
                 (int)loaded_core, (int)core_total, (int)rejected, (int)SPR_GROUP_COUNT);
        return;
    }
    GenTextureMipmaps(&tex);
    SetTextureFilter(tex, TEXTURE_FILTER_TRILINEAR);
    SetTextureWrap(tex, TEXTURE_WRAP_CLAMP);
    bank->atlas = tex;
    bank->enabled = true;
    bank->groups_ok = groups_ok;
    bank->load_ms = (GetTime() - t0) * 1000.0;
    TraceLog(LOG_INFO,
             "INKSTORM: sprites %d/%d (%d rejected; procedural fallback for %d groups; %.1f ms)",
             (int)loaded_core, (int)core_total, (int)rejected, (int)(SPR_GROUP_COUNT - groups_ok),
             bank->load_ms);
}

void render_sprites_close(RenderState *r)
{
    if (r == NULL) {
        return;
    }
    if (r->sprites.atlas.id != 0) {
        UnloadTexture(r->sprites.atlas);
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
    rlSetTexture(0);
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
