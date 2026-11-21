#include "ink_prims.h"
#include "render_internal.h"

#include "core.h"

#include <math.h>
#include <stddef.h>

static const float k_ent_pi = 3.14159265359f;
static const float k_ent_two_pi = 6.28318530718f;
static const Vector2 k_shade_dir = {0.7071f, 0.7071f};

static uint32_t entity_seed(EntityId id, uint32_t part, uint64_t boil_step)
{
    uint64_t h = ink_hash_u32(INK_HASH_INIT, id.index);
    h = ink_hash_u32(h, id.gen);
    h = ink_hash_u32(h, part);
    h = ink_hash_u32(h, (uint32_t)(boil_step & 0xffffffffu));
    return (uint32_t)(h ^ (h >> 32));
}

static void draw_eye_brush(RenderState *r, Vector2 at, float size, PaletteId pal, float alpha)
{
    /* Slanted calligraphic almond eye (never a geometric circle) */
    const Vector2 pts[4] = {
        {at.x - size * 1.15f, at.y + size * 0.12f},
        {at.x - size * 0.10f, at.y - size * 0.58f},
        {at.x + size * 1.15f, at.y - size * 0.08f},
        {at.x + size * 0.12f, at.y + size * 0.52f},
    };
    ink_fill_poly(r, pts, 4, pal_color_alpha(pal, alpha));
}

static void draw_shrine(RenderState *r, const ShrineState *s, uint64_t ticks, int act_index)
{
    const uint64_t boil = ticks / 6u;
    const uint32_t seed =
        (uint32_t)(ink_hash_u32(INK_HASH_INIT, (uint32_t)(boil + 77u)) & 0xffffffffu);
    const float cx = s->x;
    const float cy = s->y;
    const float hp_ratio =
        s->max_hp > 0 ? ink_clampf((float)s->hp / (float)s->max_hp, 0.0f, 1.0f) : 0.0f;

    /* Asymmetric organic silhouette for knockout */
    const Vector2 ko_poly[8] = {
        {cx - 40.0f, cy - 10.0f}, {cx - 24.0f, cy - 30.0f}, {cx + 2.0f, cy - 34.0f},
        {cx + 27.0f, cy - 28.0f}, {cx + 42.0f, cy - 8.0f},  {cx + 28.0f, cy + 30.0f},
        {cx - 2.0f, cy + 34.0f},  {cx - 30.0f, cy + 28.0f},
    };
    ink_knockout(r, ko_poly, 8, 4.0f, act_index);

    /* Base stone shadow wash */
    ink_wash(r, (Vector2){cx, cy + 22.0f}, 34.0f, 13.0f, 0.04f,
             (InkTone){.color = PAL_SUMI_CLEAR, .alpha = 0.55f}, 2, seed + 1u);

    /* Gofun sanctuary walls (asymmetric hand-brushed trapezoid) */
    const Vector2 wall[4] = {
        {cx - 21.0f, cy - 8.0f},
        {cx + 23.0f, cy - 9.0f},
        {cx + 25.0f, cy + 24.0f},
        {cx - 24.0f, cy + 23.0f},
    };
    ink_fill_poly(r, wall, 4, pal_color_alpha(PAL_GOFUN, 0.94f));
    ink_wash(r, (Vector2){cx + 4.0f, cy + 8.0f}, 18.0f, 14.0f, 0.1f,
             (InkTone){.color = PAL_SUMI_LIGHT, .alpha = 0.22f}, 2, seed + 2u);
    ink_outline(r, wall, 4, true, 3.5f, k_shade_dir, seed + 3u);

    /* Curved sweeping roof stroke */
    const Vector2 roof[5] = {
        {cx - 42.0f, cy - 8.0f},  {cx - 20.0f, cy - 22.0f}, {cx + 1.0f, cy - 31.0f},
        {cx + 22.0f, cy - 21.0f}, {cx + 44.0f, cy - 6.0f},
    };
    const BrushProfile roof_prof = {.w_max = 8.5f,
                                    .w_min_ratio = 0.25f,
                                    .entry = 0.14f,
                                    .exit = 0.18f,
                                    .dry = 0.25f,
                                    .seed = seed + 4u};
    ink_stroke(r, roof, 5, roof_prof, (InkTone){.color = PAL_SUMI_DARK, .alpha = 0.96f}, 1.0f);

    /* Secondary eaves ridge */
    const Vector2 eave[3] = {{cx - 32.0f, cy - 7.0f}, {cx, cy - 11.0f}, {cx + 33.0f, cy - 6.0f}};
    const BrushProfile eave_prof = {.w_max = 3.6f,
                                    .w_min_ratio = 0.32f,
                                    .entry = 0.15f,
                                    .exit = 0.20f,
                                    .dry = 0.15f,
                                    .seed = seed + 5u};
    ink_stroke(r, eave, 3, eave_prof, (InkTone){.color = PAL_SUMI_SCORCH, .alpha = 0.90f}, 1.0f);

    /* Shimenawa rope line under the eaves */
    const Vector2 rope[4] = {
        {cx - 19.0f, cy - 4.0f},
        {cx - 6.0f, cy + 1.0f},
        {cx + 7.0f, cy + 1.5f},
        {cx + 20.0f, cy - 4.5f},
    };
    const BrushProfile rope_prof = {.w_max = 2.6f,
                                    .w_min_ratio = 0.35f,
                                    .entry = 0.15f,
                                    .exit = 0.20f,
                                    .dry = 0.10f,
                                    .seed = seed + 6u};
    ink_stroke(r, rope, 4, rope_prof, (InkTone){.color = PAL_SUMI_HEAVY, .alpha = 0.85f}, 1.0f);

    /* Sacred vermilion flame under the roof (distinct from round lantern yokai) */
    if (s->hp > 0) {
        const float flicker =
            sinf((float)ticks * 0.22f) * (r->shrine_flicker_t > 0.0f ? 4.5f : 1.2f);
        const float flame_h = 9.0f + hp_ratio * 11.0f;
        const float flame_w = 5.5f + hp_ratio * 5.0f;
        const Vector2 flame_pos = {cx + flicker * 0.3f, cy + 11.0f};
        ink_teardrop(r, flame_pos, (Vector2){flicker * 0.08f, -1.0f}, flame_h, flame_w,
                     pal_color_alpha(PAL_SHU, 0.90f));
        ink_teardrop(r, (Vector2){flame_pos.x, flame_pos.y + 1.5f}, (Vector2){0.0f, -1.0f},
                     flame_h * 0.58f, flame_w * 0.54f, pal_color_alpha(PAL_SHU_HOT, 0.94f));
    }

    /* Invulnerability / hit flicker flash */
    if (s->invuln_t > 0.0f && ((ticks / 3u) & 1u) == 0u) {
        ink_outline_tone(r, ko_poly, 8, true, 3.0f, k_shade_dir,
                         (InkTone){.color = PAL_GOFUN, .alpha = 0.85f}, seed + 9u);
    }
}

static void draw_status_overlays(RenderState *r, const TransformComp *tr, const StatusComp *st,
                                 const HealthComp *hp, uint32_t seed)
{
    const Vector2 c = {tr->x, tr->y};
    const float rad = tr->radius > 6.0f ? tr->radius : 16.0f;

    if (st != NULL) {
        if (st->ink_mark_t > 0.0f) {
            const BrushProfile mp = {.w_max = 3.4f,
                                     .w_min_ratio = 0.3f,
                                     .entry = 0.2f,
                                     .exit = 0.3f,
                                     .dry = 0.3f,
                                     .seed = seed + 40u};
            ink_arc_stroke(r, c, rad + 6.0f, 0.4f, 4.9f, mp,
                           (InkTone){.color = PAL_SUMI_HEAVY, .alpha = 0.80f});
        }
        if (st->burn_acc > 0.01f) {
            for (int k = 0; k < 2; ++k) {
                const float ang = -k_ent_pi * 0.5f + ((k == 0) ? -0.45f : 0.45f);
                const Vector2 p = {c.x + cosf(ang) * rad * 0.7f, c.y + sinf(ang) * rad * 0.85f};
                ink_teardrop(r, p, (Vector2){0.15f * (float)(k * 2 - 1), -1.0f}, 9.0f, 4.5f,
                             pal_color_alpha(PAL_SHU, 0.85f));
                ink_teardrop(r, p, (Vector2){0.0f, -1.0f}, 5.0f, 2.5f,
                             pal_color_alpha(PAL_SHU_HOT, 0.92f));
            }
        }
        if (st->charge_t > 0.0f) {
            const Vector2 bolt[4] = {
                {c.x - rad * 0.8f, c.y - rad * 0.4f},
                {c.x - rad * 0.2f, c.y - rad * 0.9f},
                {c.x + rad * 0.3f, c.y - rad * 0.3f},
                {c.x + rad * 0.9f, c.y - rad * 0.75f},
            };
            const BrushProfile zp = {.w_max = 2.8f,
                                     .w_min_ratio = 0.35f,
                                     .entry = 0.15f,
                                     .exit = 0.25f,
                                     .dry = 0.0f,
                                     .seed = seed + 41u};
            ink_stroke(r, bolt, 4, zp, (InkTone){.color = PAL_AO, .alpha = 0.90f}, 1.0f);
            const BrushProfile zc = {.w_max = 1.4f,
                                     .w_min_ratio = 0.40f,
                                     .entry = 0.1f,
                                     .exit = 0.1f,
                                     .dry = 0.0f,
                                     .seed = seed + 42u};
            ink_stroke(r, bolt, 4, zc, (InkTone){.color = PAL_AO_HOT, .alpha = 0.95f}, 1.0f);
        }
    }

    if (hp != NULL && hp->flash_t > 0.0f) {
        const Vector2 slash[2] = {{c.x - rad * 0.85f, c.y - rad * 0.55f},
                                  {c.x + rad * 0.85f, c.y + rad * 0.55f}};
        const BrushProfile fp = {.w_max = 4.5f,
                                 .w_min_ratio = 0.25f,
                                 .entry = 0.15f,
                                 .exit = 0.35f,
                                 .dry = 0.2f,
                                 .seed = seed + 45u};
        ink_stroke(r, slash, 2, fp, (InkTone){.color = PAL_GOFUN, .alpha = 0.92f}, 1.0f);
        ink_stroke(r, slash, 2,
                   (BrushProfile){.w_max = 2.0f,
                                  .w_min_ratio = 0.3f,
                                  .entry = 0.15f,
                                  .exit = 0.3f,
                                  .dry = 0.0f,
                                  .seed = seed + 46u},
                   (InkTone){.color = PAL_SHU, .alpha = 0.85f}, 1.0f);
    }
}

static void draw_enemy_archer(RenderState *r, const TransformComp *tr, const EnemyComp *en,
                              uint32_t seed, int act_index, float alpha)
{
    const float cx = tr->x;
    const float cy = tr->y;
    const float rad = tr->radius > 6.0f ? tr->radius : 16.0f;
    const float ang = tr->facing;
    const float ca = cosf(ang);
    const float sa = sinf(ang);

    /* Slender asymmetric hooded archer silhouette */
    const Vector2 body[7] = {
        {cx - rad * 0.55f, cy - rad * 0.95f}, {cx + rad * 0.32f, cy - rad * 1.05f},
        {cx + rad * 0.72f, cy - rad * 0.25f}, {cx + rad * 0.55f, cy + rad * 0.92f},
        {cx - rad * 0.10f, cy + rad * 1.05f}, {cx - rad * 0.76f, cy + rad * 0.78f},
        {cx - rad * 0.80f, cy - rad * 0.18f},
    };
    ink_knockout(r, body, 7, 3.0f, act_index);
    ink_wash(r, (Vector2){cx, cy - rad * 0.15f}, rad * 0.72f, rad * 0.92f, 0.12f,
             (InkTone){.color = PAL_SUMI_LIGHT, .alpha = 0.52f * alpha}, 2, seed + 1u);
    ink_wash(r, (Vector2){cx - rad * 0.1f, cy - rad * 0.52f}, rad * 0.52f, rad * 0.42f, -0.15f,
             (InkTone){.color = PAL_SUMI_HEAVY, .alpha = 0.46f * alpha}, 2, seed + 2u);
    ink_outline_tone(r, body, 7, true, 3.0f, k_shade_dir,
                     (InkTone){.color = PAL_SUMI_DARK, .alpha = 0.96f * alpha}, seed + 3u);

    /* Slender bow bent more during windup */
    const float pull = en != NULL ? ink_clampf(en->windup, 0.0f, 1.0f) : 0.0f;
    const float bow_r = rad * 1.15f;
    const Vector2 bow[4] = {
        {cx + ca * (bow_r * 0.45f) - sa * bow_r, cy + sa * (bow_r * 0.45f) + ca * bow_r},
        {cx + ca * (bow_r * (1.05f - 0.15f * pull)) - sa * (bow_r * 0.35f),
         cy + sa * (bow_r * (1.05f - 0.15f * pull)) + ca * (bow_r * 0.35f)},
        {cx + ca * (bow_r * (1.05f - 0.15f * pull)) + sa * (bow_r * 0.35f),
         cy + sa * (bow_r * (1.05f - 0.15f * pull)) - ca * (bow_r * 0.35f)},
        {cx + ca * (bow_r * 0.45f) + sa * bow_r, cy + sa * (bow_r * 0.45f) - ca * bow_r},
    };
    const BrushProfile bp = {.w_max = 3.2f,
                             .w_min_ratio = 0.25f,
                             .entry = 0.15f,
                             .exit = 0.30f,
                             .dry = 0.2f,
                             .seed = seed + 4u};
    ink_stroke(r, bow, 4, bp, (InkTone){.color = PAL_SUMI_SCORCH, .alpha = 0.95f * alpha}, 1.0f);

    if (pull > 0.05f) {
        /* Drawn arrow telegraph */
        const Vector2 arr[2] = {
            {cx - ca * (rad * 0.3f * pull), cy - sa * (rad * 0.3f * pull)},
            {cx + ca * (rad * 1.35f), cy + sa * (rad * 1.35f)},
        };
        ink_stroke(r, arr, 2,
                   (BrushProfile){.w_max = 2.6f,
                                  .w_min_ratio = 0.3f,
                                  .entry = 0.1f,
                                  .exit = 0.35f,
                                  .dry = 0.0f,
                                  .seed = seed + 5u},
                   (InkTone){.color = PAL_SHU, .alpha = 0.85f * alpha}, 1.0f);
    }

    draw_eye_brush(r, (Vector2){cx + ca * 4.0f, cy - rad * 0.35f + sa * 3.0f}, 3.4f, PAL_GOFUN,
                   alpha);
}

static void draw_enemy_lantern(RenderState *r, const TransformComp *tr, const EnemyComp *en,
                               uint32_t seed, uint64_t ticks, int act_index, float alpha)
{
    const float cx = tr->x;
    const float cy = tr->y;
    const float rad = tr->radius > 6.0f ? tr->radius : 18.0f;
    const float pull = en != NULL ? ink_clampf(en->windup, 0.0f, 1.0f) : 0.0f;
    const float pulse =
        0.85f + 0.15f * sinf((float)ticks * 0.14f + (float)(seed & 15u)) + 0.28f * pull;

    /* Asymmetric torn chochin paper lantern silhouette */
    const Vector2 body[8] = {
        {cx - rad * 0.42f, cy - rad * 1.08f}, {cx + rad * 0.48f, cy - rad * 1.02f},
        {cx + rad * 0.96f, cy - rad * 0.32f}, {cx + rad * 0.88f, cy + rad * 0.56f},
        {cx + rad * 0.38f, cy + rad * 1.04f}, {cx - rad * 0.46f, cy + rad * 0.96f},
        {cx - rad * 0.92f, cy + rad * 0.40f}, {cx - rad * 0.84f, cy - rad * 0.42f},
    };
    ink_knockout(r, body, 8, 3.0f, act_index);
    ink_fill_poly(r, body, 8, pal_color_alpha(PAL_GOFUN, 0.88f * alpha));

    /* Inner vermilion flame glow (two-layer fire tone) */
    ink_wash(r, (Vector2){cx, cy + 1.0f}, rad * 0.56f * pulse, rad * 0.62f * pulse, 0.08f,
             (InkTone){.color = PAL_SHU, .alpha = 0.68f * alpha}, 2, seed + 1u);
    ink_wash(r, (Vector2){cx, cy + 1.0f}, rad * 0.30f * pulse, rad * 0.34f * pulse, -0.1f,
             (InkTone){.color = PAL_SHU_HOT, .alpha = 0.88f * alpha}, 1, seed + 2u);

    /* Curved lantern ribs */
    for (int rib = -1; rib <= 1; ++rib) {
        const float ry = cy + (float)rib * rad * 0.46f;
        const Vector2 pts[3] = {
            {cx - rad * 0.80f, ry},
            {cx, ry + (float)rib * 3.5f},
            {cx + rad * 0.82f, ry - 1.0f},
        };
        ink_stroke(r, pts, 3,
                   (BrushProfile){.w_max = 2.4f,
                                  .w_min_ratio = 0.35f,
                                  .entry = 0.15f,
                                  .exit = 0.25f,
                                  .dry = 0.2f,
                                  .seed = seed + 10u + (uint32_t)(rib + 1)},
                   (InkTone){.color = PAL_SUMI_HEAVY, .alpha = 0.72f * alpha}, 1.0f);
    }

    /* Torn mouth and tongue flick */
    const Vector2 tongue[3] = {
        {cx - 2.0f, cy + rad * 0.35f},
        {cx + 6.0f, cy + rad * 0.78f},
        {cx + 12.0f + pull * 5.0f, cy + rad * 1.18f},
    };
    ink_stroke(r, tongue, 3,
               (BrushProfile){.w_max = 3.8f,
                              .w_min_ratio = 0.22f,
                              .entry = 0.1f,
                              .exit = 0.45f,
                              .dry = 0.1f,
                              .seed = seed + 15u},
               (InkTone){.color = PAL_SHU, .alpha = 0.92f * alpha}, 1.0f);

    ink_outline_tone(r, body, 8, true, 3.0f, k_shade_dir,
                     (InkTone){.color = PAL_SUMI_DARK, .alpha = 0.96f * alpha}, seed + 3u);
    draw_eye_brush(r, (Vector2){cx, cy - rad * 0.34f}, 5.0f, PAL_SUMI_SCORCH, alpha);
    draw_eye_brush(r, (Vector2){cx + 0.5f, cy - rad * 0.34f}, 2.2f, PAL_GOFUN, alpha);
}

static void draw_enemy_umbrella(RenderState *r, const TransformComp *tr, const EnemyComp *en,
                                uint32_t seed, int act_index, float alpha)
{
    const float cx = tr->x;
    const float cy = tr->y;
    const float rad = tr->radius > 6.0f ? tr->radius : 20.0f;
    const float pull = en != NULL ? ink_clampf(en->windup, 0.0f, 1.0f) : 0.0f;
    const float squash = 1.0f - 0.16f * pull;

    /* Karakasa umbrella canopy + single geta leg */
    const Vector2 canopy[7] = {
        {cx - rad * 1.12f, cy + rad * 0.10f * squash},
        {cx - rad * 0.65f, cy - rad * 0.72f * squash},
        {cx + rad * 0.06f, cy - rad * 1.08f * squash},
        {cx + rad * 0.78f, cy - rad * 0.62f * squash},
        {cx + rad * 1.06f, cy + rad * 0.18f * squash},
        {cx + rad * 0.28f, cy + rad * 0.32f * squash},
        {cx - rad * 0.36f, cy + rad * 0.36f * squash},
    };
    ink_knockout(r, canopy, 7, 3.2f, act_index);
    ink_wash(r, (Vector2){cx, cy - rad * 0.22f}, rad * 0.98f, rad * 0.62f * squash, -0.08f,
             (InkTone){.color = PAL_SUMI_LIGHT, .alpha = 0.58f * alpha}, 2, seed + 1u);

    /* Radial ribs */
    const Vector2 apex = {cx + rad * 0.04f, cy - rad * 1.04f * squash};
    for (int i = -2; i <= 2; ++i) {
        const Vector2 rib[2] = {apex, {cx + (float)i * rad * 0.44f, cy + rad * 0.22f * squash}};
        ink_stroke(r, rib, 2,
                   (BrushProfile){.w_max = 2.8f,
                                  .w_min_ratio = 0.30f,
                                  .entry = 0.12f,
                                  .exit = 0.35f,
                                  .dry = 0.22f,
                                  .seed = seed + 20u + (uint32_t)(i + 2)},
                   (InkTone){.color = PAL_SUMI_DARK, .alpha = 0.85f * alpha}, 1.0f);
    }
    ink_outline_tone(r, canopy, 7, true, 3.5f, k_shade_dir,
                     (InkTone){.color = PAL_SUMI_DARK, .alpha = 0.96f * alpha}, seed + 3u);

    /* Single hopping leg */
    const Vector2 leg[3] = {
        {cx - 1.0f, cy + rad * 0.24f},
        {cx - 4.0f, cy + rad * 0.78f},
        {cx + 5.0f, cy + rad * 1.15f},
    };
    ink_stroke(r, leg, 3,
               (BrushProfile){.w_max = 4.2f,
                              .w_min_ratio = 0.32f,
                              .entry = 0.1f,
                              .exit = 0.25f,
                              .dry = 0.1f,
                              .seed = seed + 28u},
               (InkTone){.color = PAL_SUMI_SCORCH, .alpha = 0.94f * alpha}, 1.0f);

    /* One bold calligraphic eye */
    draw_eye_brush(r, (Vector2){cx + rad * 0.08f, cy - rad * 0.24f}, 7.0f, PAL_GOFUN, alpha);
    draw_eye_brush(r, (Vector2){cx + rad * 0.10f, cy - rad * 0.24f}, 3.6f, PAL_SUMI_SCORCH, alpha);
}

static void draw_boss_tengu(RenderState *r, const TransformComp *tr, const BossComp *boss,
                            const ShrineState *shrine, uint32_t seed, int act_index, float alpha)
{
    const float cx = tr->x;
    const float cy = tr->y;
    const float rad = tr->radius > 10.0f ? tr->radius : 44.0f;

    /* Telegraph crimson dashed warning lines before boss attack */
    if (boss != NULL && boss->telegraph > 0.01f) {
        const float tx = boss->gale_dx != 0.0f || boss->gale_dy != 0.0f ? boss->gale_dx : 0.0f;
        const float ty = boss->gale_dx != 0.0f || boss->gale_dy != 0.0f ? boss->gale_dy : 1.0f;
        const float len = 180.0f + boss->telegraph * 140.0f;
        const InkTone warn = {.color = PAL_SHU, .alpha = 0.55f + 0.35f * boss->telegraph};
        if (boss->telegraph_pattern == 3 && shrine != NULL) {
            const float vr = 14.0f * FLUID_CELL_PX;
            const int segs = 12;
            for (int k = 0; k < segs; ++k) {
                const float a0 = k_ent_two_pi * (float)k / (float)segs;
                const float a1 = k_ent_two_pi * ((float)k + 0.55f) / (float)segs;
                const Vector2 p0 = {shrine->x + cosf(a0) * vr, shrine->y + sinf(a0) * vr};
                const Vector2 p1 = {shrine->x + cosf(a1) * vr, shrine->y + sinf(a1) * vr};
                ink_dashed_stroke(r, p0, p1, 18.0f, 6.0f, 3.4f, warn, seed + 90u + (uint32_t)k);
            }
        }
        for (int k = -1; k <= 1; ++k) {
            const float a = atan2f(ty, tx) + (float)k * 0.24f;
            const Vector2 dst = {cx + cosf(a) * len, cy + sinf(a) * len};
            ink_dashed_stroke(r, (Vector2){cx, cy}, dst, 16.0f, 10.0f, 3.4f, warn,
                              seed + 80u + (uint32_t)(k + 1));
        }
    }

    /* Sweeping knockout covering mask & wing roots */
    const Vector2 ko[8] = {
        {cx - rad * 1.65f, cy - rad * 0.35f}, {cx - rad * 0.80f, cy - rad * 1.10f},
        {cx + rad * 0.08f, cy - rad * 1.05f}, {cx + rad * 0.90f, cy - rad * 1.08f},
        {cx + rad * 1.72f, cy - rad * 0.25f}, {cx + rad * 0.95f, cy + rad * 0.92f},
        {cx - rad * 0.04f, cy + rad * 1.15f}, {cx - rad * 0.98f, cy + rad * 0.88f},
    };
    ink_knockout(r, ko, 8, 5.0f, act_index);

    /* Layered dry-brush wings (6 sweeping flicks on each side) */
    for (int side = -1; side <= 1; side += 2) {
        for (int f = 0; f < 6; ++f) {
            const float u = (float)f / 5.0f;
            const float span = rad * (1.75f - 0.32f * u);
            const float lift = rad * (-0.75f + 1.15f * u);
            const Vector2 wpts[3] = {
                {cx + (float)side * rad * 0.28f, cy - rad * 0.15f + (float)f * 4.5f},
                {cx + (float)side * span * 0.62f, cy + lift * 0.45f - rad * 0.25f},
                {cx + (float)side * span, cy + lift},
            };
            const BrushProfile wp = {
                .w_max = 10.5f - (float)f * 0.9f,
                .w_min_ratio = 0.20f,
                .entry = 0.12f,
                .exit = 0.48f,
                .dry = 0.55f,
                .seed = seed + 30u + (uint32_t)(side > 0 ? 10 + f : f),
            };
            ink_stroke(r, wpts, 3, wp,
                       (InkTone){.color = (f % 2 == 0) ? PAL_SUMI_DARK : PAL_SUMI_HEAVY,
                                 .alpha = 0.90f * alpha},
                       1.0f);
        }
    }

    /* Bengara (muted iron-oxide crimson, distinct from fire PAL_SHU) tengu mask */
    const Vector2 mask[7] = {
        {cx - rad * 0.56f, cy - rad * 0.68f}, {cx + rad * 0.04f, cy - rad * 0.82f},
        {cx + rad * 0.60f, cy - rad * 0.62f}, {cx + rad * 0.66f, cy + rad * 0.18f},
        {cx + rad * 0.22f, cy + rad * 0.84f}, {cx - rad * 0.26f, cy + rad * 0.80f},
        {cx - rad * 0.64f, cy + rad * 0.12f},
    };
    ink_wash(r, (Vector2){cx, cy}, rad * 0.66f, rad * 0.78f, 0.06f,
             (InkTone){.color = PAL_BENGARA, .alpha = 0.88f * alpha}, 2, seed + 2u);
    ink_outline_tone(r, mask, 7, true, 6.5f, k_shade_dir,
                     (InkTone){.color = PAL_SUMI_SCORCH, .alpha = 0.98f * alpha}, seed + 3u);

    /* Prominent tengu nose ridge stroke */
    const Vector2 nose[3] = {
        {cx, cy - rad * 0.10f}, {cx + 2.0f, cy + rad * 0.24f}, {cx - 1.0f, cy + rad * 0.52f}};
    ink_stroke(r, nose, 3,
               (BrushProfile){.w_max = 6.2f,
                              .w_min_ratio = 0.28f,
                              .entry = 0.18f,
                              .exit = 0.30f,
                              .dry = 0.15f,
                              .seed = seed + 4u},
               (InkTone){.color = PAL_SUMI_DARK, .alpha = 0.94f * alpha}, 1.0f);

    /* Fierce slanted eyes */
    draw_eye_brush(r, (Vector2){cx - rad * 0.26f, cy - rad * 0.24f}, 7.2f, PAL_GOFUN, alpha);
    draw_eye_brush(r, (Vector2){cx + rad * 0.26f, cy - rad * 0.24f}, 7.2f, PAL_GOFUN, alpha);
    draw_eye_brush(r, (Vector2){cx - rad * 0.24f, cy - rad * 0.24f}, 3.6f, PAL_SUMI_SCORCH, alpha);
    draw_eye_brush(r, (Vector2){cx + rad * 0.24f, cy - rad * 0.24f}, 3.6f, PAL_SUMI_SCORCH, alpha);

    /* Feather fan (hauchiwa) held at lower right */
    const Vector2 fan_base = {cx + rad * 0.55f, cy + rad * 0.52f};
    for (int k = -2; k <= 2; ++k) {
        const float fa = 0.35f + (float)k * 0.22f;
        const Vector2 fpts[2] = {
            fan_base, {fan_base.x + cosf(fa) * rad * 0.85f, fan_base.y + sinf(fa) * rad * 0.85f}};
        ink_stroke(r, fpts, 2,
                   (BrushProfile){.w_max = 4.6f,
                                  .w_min_ratio = 0.25f,
                                  .entry = 0.1f,
                                  .exit = 0.45f,
                                  .dry = 0.35f,
                                  .seed = seed + 60u + (uint32_t)(k + 2)},
                   (InkTone){.color = PAL_SUMI_HEAVY, .alpha = 0.88f * alpha}, 1.0f);
    }
}

static void draw_projectile(RenderState *r, const TransformComp *tr, const VelocityComp *vel,
                            const ProjComp *pr, uint32_t seed, int act_index)
{
    const float vx = vel != NULL ? vel->vx : 1.0f;
    const float vy = vel != NULL ? vel->vy : 0.0f;
    const float speed = sqrtf(vx * vx + vy * vy);
    const Vector2 dir = speed > 1e-3f ? (Vector2){vx / speed, vy / speed} : (Vector2){1.0f, 0.0f};
    const Vector2 pos = {tr->x, tr->y};

    /* Deflected / burning / electrified dragon-vein trail */
    if (pr->trail_n >= 2) {
        Vector2 tpts[PROJ_TRAIL_LEN];
        const int tn = pr->trail_n < PROJ_TRAIL_LEN ? pr->trail_n : PROJ_TRAIL_LEN;
        for (int i = 0; i < tn; ++i) {
            tpts[i] = (Vector2){pr->trail[i].x, pr->trail[i].y};
        }
        PaletteId trail_pal = PAL_SUMI_LIGHT;
        float trail_w = 3.0f;
        if (pr->burning) {
            trail_pal = PAL_SHU;
            trail_w = 5.2f;
        } else if (pr->electrified) {
            trail_pal = PAL_AO;
            trail_w = 4.8f;
        } else if (pr->deflected) {
            trail_pal = PAL_KIN;
            trail_w = 4.6f;
        }
        ink_stroke(r, tpts, tn,
                   (BrushProfile){.w_max = trail_w,
                                  .w_min_ratio = 0.25f,
                                  .entry = 0.55f,
                                  .exit = 0.12f,
                                  .dry = 0.35f,
                                  .seed = seed + 1u},
                   (InkTone){.color = trail_pal, .alpha = 0.80f}, 1.0f);
    }

    /* Paper knockout around projectile */
    const float rad = tr->radius > 3.0f ? tr->radius : 7.0f;
    Vector2 ko[6];
    for (int i = 0; i < 6; ++i) {
        const float a = k_ent_two_pi * (float)i / 6.0f;
        ko[i] = (Vector2){pos.x + cosf(a) * (rad + 2.0f), pos.y + sinf(a) * (rad + 2.0f)};
    }
    ink_knockout(r, ko, 6, 2.5f, act_index);

    if (pr->deflected) {
        ink_outline_tone(r, ko, 6, true, 3.0f, k_shade_dir,
                         (InkTone){.color = PAL_KIN, .alpha = 0.94f}, seed + 2u);
    }

    if (pr->kind == PROJ_ARROW) {
        const Vector2 shaft[2] = {
            {pos.x - dir.x * 16.0f, pos.y - dir.y * 16.0f},
            {pos.x + dir.x * 12.0f, pos.y + dir.y * 12.0f},
        };
        const PaletteId col = pr->burning ? PAL_SHU : (pr->electrified ? PAL_AO : PAL_SUMI_SCORCH);
        ink_stroke(r, shaft, 2,
                   (BrushProfile){.w_max = 3.2f,
                                  .w_min_ratio = 0.3f,
                                  .entry = 0.12f,
                                  .exit = 0.35f,
                                  .dry = 0.0f,
                                  .seed = seed + 3u},
                   (InkTone){.color = col, .alpha = 0.96f}, 1.0f);
        /* 3 fletching ticks */
        const Vector2 side = {-dir.y, dir.x};
        for (int f = -1; f <= 1; ++f) {
            const Vector2 base = {pos.x - dir.x * (12.0f + (float)f * 2.5f),
                                  pos.y - dir.y * (12.0f + (float)f * 2.5f)};
            const Vector2 tip = {base.x - dir.x * 5.0f + side.x * (float)(f == 0 ? 5 : f * 6),
                                 base.y - dir.y * 5.0f + side.y * (float)(f == 0 ? 5 : f * 6)};
            const Vector2 fpts[2] = {base, tip};
            ink_stroke(r, fpts, 2,
                       (BrushProfile){.w_max = 2.0f,
                                      .w_min_ratio = 0.4f,
                                      .entry = 0.1f,
                                      .exit = 0.4f,
                                      .dry = 0.0f,
                                      .seed = seed + 10u + (uint32_t)(f + 1)},
                       (InkTone){.color = PAL_SUMI_DARK, .alpha = 0.85f}, 1.0f);
        }
    } else if (pr->kind == PROJ_FIREBALL) {
        ink_teardrop(r, pos, dir, rad * 1.9f, rad * 1.35f, pal_color_alpha(PAL_SHU, 0.94f));
        ink_teardrop(r, pos, dir, rad * 1.1f, rad * 0.78f, pal_color_alpha(PAL_SHU_HOT, 0.98f));
        if (pr->electrified) {
            ink_outline_tone(r, ko, 6, true, 2.4f, k_shade_dir,
                             (InkTone){.color = PAL_AO_HOT, .alpha = 0.90f}, seed + 8u);
        }
    } else {
        /* PROJ_STORM_ORB: pointed azure jewel + two short lightning arcs */
        ink_teardrop(r, pos, (Vector2){0.0f, -1.0f}, rad * 1.45f, rad * 1.35f,
                     pal_color_alpha(PAL_AO, 0.94f));
        ink_teardrop(r, pos, (Vector2){0.0f, -1.0f}, rad * 0.80f, rad * 0.75f,
                     pal_color_alpha(PAL_AO_HOT, 0.98f));
        const Vector2 z1[3] = {{pos.x - rad * 1.3f, pos.y - 3.0f},
                               {pos.x - rad * 0.6f, pos.y - rad * 1.1f},
                               {pos.x + 2.0f, pos.y - rad * 0.8f}};
        ink_stroke(r, z1, 3,
                   (BrushProfile){.w_max = 2.2f,
                                  .w_min_ratio = 0.35f,
                                  .entry = 0.15f,
                                  .exit = 0.35f,
                                  .dry = 0.0f,
                                  .seed = seed + 12u},
                   (InkTone){.color = PAL_AO_HOT, .alpha = 0.92f}, 1.0f);
    }
}

static void draw_ink_orb(RenderState *r, const TransformComp *tr, const InkOrbComp *orb,
                         uint32_t seed, int act_index)
{
    /* Blink during final 2 seconds of 11.5s lifetime */
    if (orb != NULL && orb->age > 9.5f && ((int)(orb->age * 10.0f) & 1) == 0) {
        return;
    }
    const float cx = tr->x;
    const float cy = tr->y;
    const float rad = tr->radius > 3.0f ? tr->radius : 7.0f;

    Vector2 body[7];
    for (int i = 0; i < 7; ++i) {
        const float a = k_ent_two_pi * (float)i / 7.0f;
        const float j = 0.88f + 0.22f * ((float)((seed + (uint32_t)i * 19u) & 7u) / 7.0f);
        body[i] = (Vector2){cx + cosf(a) * rad * j, cy + sinf(a) * rad * j};
    }
    ink_knockout(r, body, 7, 3.0f, act_index);
    ink_fill_poly(r, body, 7, pal_color_alpha(PAL_SUMI_SCORCH, 0.96f));
    /* Single gold highlight fleck */
    ink_teardrop(r, (Vector2){cx - rad * 0.28f, cy - rad * 0.28f}, (Vector2){0.7f, -0.7f}, 3.6f,
                 2.6f, pal_color_alpha(PAL_KIN_HOT, 0.96f));
}

void render_draw_entities(RenderState *r, const GameState *g, int act_index)
{
    if (r == NULL || g == NULL) {
        return;
    }
    const uint64_t boil = g->ticks / 6u;
    const uint32_t hw = g->ecs.high_water;

    render_set_blend(r, BLEND_ALPHA);

    /* Layer 5: Ink orbs & projectiles */
    for (uint32_t i = 0; i < hw; ++i) {
        if (!g->ecs.alive[i]) {
            continue;
        }
        const EntityId id = {.index = i, .gen = g->ecs.gen[i]};
        const TransformComp *tr = game_transform(g, id);
        if (tr == NULL) {
            continue;
        }
        const InkOrbComp *orb = game_orb(g, id);
        if (orb != NULL) {
            draw_ink_orb(r, tr, orb, entity_seed(id, 1u, boil), act_index);
        }
    }

    for (uint32_t i = 0; i < hw; ++i) {
        if (!g->ecs.alive[i]) {
            continue;
        }
        const EntityId id = {.index = i, .gen = g->ecs.gen[i]};
        const TransformComp *tr = game_transform(g, id);
        const ProjComp *pr = game_proj(g, id);
        if (tr != NULL && pr != NULL) {
            const uint32_t pseed = entity_seed(id, 2u, boil);
            if (tr->dissolving) {
                const float u = ink_clampf(tr->dissolve_t / 0.6f, 0.0f, 1.0f);
                const float rad = (tr->radius > 3.0f ? tr->radius : 7.0f) * (1.1f + 0.8f * u);
                ink_wash(r, (Vector2){tr->x, tr->y}, rad, rad * 0.88f, u * 0.8f,
                         (InkTone){.color = PAL_SUMI_LIGHT, .alpha = (1.0f - u) * 0.65f}, 1, pseed);
            } else {
                draw_projectile(r, tr, game_velocity(g, id), pr, pseed, act_index);
            }
        }
    }

    /* Layer 6: Shrines, enemies, boss */
    for (int32_t s = 0; s < g->shrine_count && s < GAME_MAX_SHRINES; ++s) {
        draw_shrine(r, &g->shrines[s], g->ticks, act_index);
    }

    const ShrineState *primary_shrine = g->shrine_count > 0 ? &g->shrines[0] : NULL;
    for (uint32_t i = 0; i < hw; ++i) {
        if (!g->ecs.alive[i]) {
            continue;
        }
        const EntityId id = {.index = i, .gen = g->ecs.gen[i]};
        const TransformComp *tr = game_transform(g, id);
        const EnemyComp *en = game_enemy(g, id);
        if (tr == NULL || en == NULL) {
            continue;
        }

        const uint32_t seed = entity_seed(id, 3u, boil);
        if (tr->dissolving) {
            const float u = ink_clampf(tr->dissolve_t / 0.6f, 0.0f, 1.0f);
            const float rad = (tr->radius > 6.0f ? tr->radius : 18.0f) * (1.0f + 0.55f * u);
            ink_wash(r, (Vector2){tr->x, tr->y}, rad, rad * 0.86f, u * 0.6f,
                     (InkTone){.color = PAL_SUMI_HEAVY, .alpha = (1.0f - u) * 0.72f}, 2, seed);
            continue;
        }

        switch (en->def_id) {
        case ENEMY_ARCHER:
            draw_enemy_archer(r, tr, en, seed, act_index, 1.0f);
            break;
        case ENEMY_LANTERN_FIRE:
            draw_enemy_lantern(r, tr, en, seed, g->ticks, act_index, 1.0f);
            break;
        case ENEMY_UMBRELLA:
            draw_enemy_umbrella(r, tr, en, seed, act_index, 1.0f);
            break;
        case ENEMY_BOSS_TENGU:
            draw_boss_tengu(r, tr, game_boss(g, id), primary_shrine, seed, act_index, 1.0f);
            break;
        default:
            break;
        }

        draw_status_overlays(r, tr, game_status(g, id), game_health(g, id), seed);
    }
}
