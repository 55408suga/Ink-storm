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

static void draw_elite_rim(RenderState *r, const TransformComp *tr, const EnemyComp *en,
                           uint32_t seed)
{
    if (en == NULL || !en->is_elite) {
        return;
    }
    const uint32_t mask = en->elite_flags & ELITE_ALL_MASK;
    if (mask == 0u) {
        return;
    }
    PaletteId pals[4];
    int count = 0;
    if ((mask & ELITE_RIGID) != 0u) {
        pals[count++] = PAL_KIN;
    }
    if ((mask & ELITE_INK_REPEL) != 0u) {
        pals[count++] = PAL_GOFUN;
    }
    if ((mask & ELITE_FIREPROOF) != 0u) {
        pals[count++] = PAL_SHU;
    }
    if ((mask & ELITE_STORMPROOF) != 0u) {
        pals[count++] = PAL_AO;
    }
    if (count == 0) {
        return;
    }

    const Vector2 c = {tr->x, tr->y};
    const float rad = (tr->radius > 6.0f ? tr->radius : 16.0f) + 6.5f;
    const BrushProfile prof = {.w_max = 2.4f,
                               .w_min_ratio = 0.40f,
                               .entry = 0.04f,
                               .exit = 0.04f,
                               .dry = 0.0f,
                               .seed = seed + 99u};
    if (count == 1) {
        /* Continuous solid ring (never dashed, distinct from crimson telegraph warnings) */
        ink_arc_stroke(r, c, rad, 0.0f, k_ent_two_pi + 0.08f, prof,
                       (InkTone){.color = pals[0], .alpha = 0.94f});
    } else {
        const float step = k_ent_two_pi / (float)count;
        for (int i = 0; i < count; ++i) {
            const float a0 = (float)i * step;
            const float a1 = a0 + step + 0.08f;
            ink_arc_stroke(r, c, rad, a0, a1, prof, (InkTone){.color = pals[i], .alpha = 0.94f});
        }
    }
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

    ink_wash(r, (Vector2){cx, cy + 22.0f}, 34.0f, 13.0f, 0.04f,
             (InkTone){.color = PAL_SUMI_CLEAR, .alpha = 0.55f}, 2, seed + 1u);

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

    const Vector2 roof[5] = {
        {cx - 42.0f, cy - 8.0f},  {cx - 20.0f, cy - 22.0f}, {cx + 1.0f, cy - 31.0f},
        {cx + 22.0f, cy - 21.0f}, {cx + 44.0f, cy - 6.0f},
    };
    ink_stroke(r, roof, 5,
               (BrushProfile){.w_max = 8.5f,
                              .w_min_ratio = 0.25f,
                              .entry = 0.14f,
                              .exit = 0.18f,
                              .dry = 0.25f,
                              .seed = seed + 4u},
               (InkTone){.color = PAL_SUMI_DARK, .alpha = 0.96f}, 1.0f);

    const Vector2 eave[3] = {{cx - 32.0f, cy - 7.0f}, {cx, cy - 11.0f}, {cx + 33.0f, cy - 6.0f}};
    ink_stroke(r, eave, 3,
               (BrushProfile){.w_max = 3.6f,
                              .w_min_ratio = 0.32f,
                              .entry = 0.15f,
                              .exit = 0.20f,
                              .dry = 0.15f,
                              .seed = seed + 5u},
               (InkTone){.color = PAL_SUMI_SCORCH, .alpha = 0.90f}, 1.0f);

    const Vector2 rope[4] = {
        {cx - 19.0f, cy - 4.0f},
        {cx - 6.0f, cy + 1.0f},
        {cx + 7.0f, cy + 1.5f},
        {cx + 20.0f, cy - 4.5f},
    };
    ink_stroke(r, rope, 4,
               (BrushProfile){.w_max = 2.6f,
                              .w_min_ratio = 0.35f,
                              .entry = 0.15f,
                              .exit = 0.20f,
                              .dry = 0.10f,
                              .seed = seed + 6u},
               (InkTone){.color = PAL_SUMI_HEAVY, .alpha = 0.85f}, 1.0f);

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
            ink_stroke(r, bolt, 4,
                       (BrushProfile){.w_max = 2.8f,
                                      .w_min_ratio = 0.35f,
                                      .entry = 0.15f,
                                      .exit = 0.25f,
                                      .dry = 0.0f,
                                      .seed = seed + 41u},
                       (InkTone){.color = PAL_AO, .alpha = 0.90f}, 1.0f);
            ink_stroke(r, bolt, 4,
                       (BrushProfile){.w_max = 1.4f,
                                      .w_min_ratio = 0.40f,
                                      .entry = 0.1f,
                                      .exit = 0.1f,
                                      .dry = 0.0f,
                                      .seed = seed + 42u},
                       (InkTone){.color = PAL_AO_HOT, .alpha = 0.95f}, 1.0f);
        }
        if (st->barrier_t > 0.0f) {
            const float rot = r->vfx_time * 1.6f;
            const float brad = rad + 9.5f;
            for (int k = 0; k < 2; ++k) {
                const float a0 = rot + (float)k * k_ent_pi;
                const float a1 = a0 + 1.85f;
                ink_arc_stroke(r, c, brad, a0, a1,
                               (BrushProfile){.w_max = 2.8f,
                                              .w_min_ratio = 0.32f,
                                              .entry = 0.16f,
                                              .exit = 0.28f,
                                              .dry = 0.18f,
                                              .seed = seed + 50u + (uint32_t)k},
                               (InkTone){.color = PAL_SUMI_LIGHT, .alpha = 0.85f});
                ink_arc_stroke(r, c, brad - 1.2f, a0 + 0.08f, a1 - 0.08f,
                               (BrushProfile){.w_max = 1.6f,
                                              .w_min_ratio = 0.38f,
                                              .entry = 0.14f,
                                              .exit = 0.22f,
                                              .dry = 0.0f,
                                              .seed = seed + 52u + (uint32_t)k},
                               (InkTone){.color = PAL_GOFUN, .alpha = 0.95f});

                /* Small orbiting gofun talisman slip */
                const float ta = a1 + 0.62f;
                const Vector2 tc = {c.x + cosf(ta) * (brad + 1.5f), c.y + sinf(ta) * (brad + 1.5f)};
                const Vector2 td = {-sinf(ta), cosf(ta)};
                const Vector2 tn = {cosf(ta), sinf(ta)};
                const Vector2 slip[4] = {
                    {tc.x - td.x * 5.0f - tn.x * 3.0f, tc.y - td.y * 5.0f - tn.y * 3.0f},
                    {tc.x + td.x * 5.0f - tn.x * 2.6f, tc.y + td.y * 5.0f - tn.y * 2.6f},
                    {tc.x + td.x * 4.6f + tn.x * 3.0f, tc.y + td.y * 4.6f + tn.y * 3.0f},
                    {tc.x - td.x * 4.8f + tn.x * 2.8f, tc.y - td.y * 4.8f + tn.y * 2.8f},
                };
                ink_fill_poly(r, slip, 4, pal_color_alpha(PAL_GOFUN, 0.96f));
                ink_outline_tone(r, slip, 4, true, 1.8f, k_shade_dir,
                                 (InkTone){.color = PAL_SUMI_DARK, .alpha = 0.90f},
                                 seed + 54u + (uint32_t)k);
                const Vector2 glyph[2] = {
                    {tc.x - td.x * 2.6f, tc.y - td.y * 2.6f},
                    {tc.x + td.x * 2.6f, tc.y + td.y * 2.6f},
                };
                ink_stroke(r, glyph, 2,
                           (BrushProfile){.w_max = 1.4f,
                                          .w_min_ratio = 0.40f,
                                          .entry = 0.15f,
                                          .exit = 0.25f,
                                          .dry = 0.0f,
                                          .seed = seed + 56u + (uint32_t)k},
                           (InkTone){.color = PAL_SUMI_SCORCH, .alpha = 0.92f}, 1.0f);
            }
        }
    }

    if (hp != NULL && hp->flash_t > 0.0f) {
        const Vector2 slash[2] = {{c.x - rad * 0.85f, c.y - rad * 0.55f},
                                  {c.x + rad * 0.85f, c.y + rad * 0.55f}};
        ink_stroke(r, slash, 2,
                   (BrushProfile){.w_max = 4.5f,
                                  .w_min_ratio = 0.25f,
                                  .entry = 0.15f,
                                  .exit = 0.35f,
                                  .dry = 0.2f,
                                  .seed = seed + 45u},
                   (InkTone){.color = PAL_GOFUN, .alpha = 0.92f}, 1.0f);
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

    const Vector2 body[7] = {
        {cx - rad * 0.55f, cy - rad * 0.95f}, {cx + rad * 0.32f, cy - rad * 1.05f},
        {cx + rad * 0.72f, cy - rad * 0.25f}, {cx + rad * 0.55f, cy + rad * 0.92f},
        {cx - rad * 0.10f, cy + rad * 1.05f}, {cx - rad * 0.76f, cy + rad * 0.78f},
        {cx - rad * 0.80f, cy - rad * 0.18f},
    };
    ink_knockout(r, body, 7, 3.0f, act_index);
    draw_elite_rim(r, tr, en, seed);
    ink_wash(r, (Vector2){cx, cy - rad * 0.15f}, rad * 0.72f, rad * 0.92f, 0.12f,
             (InkTone){.color = PAL_SUMI_LIGHT, .alpha = 0.52f * alpha}, 2, seed + 1u);
    ink_wash(r, (Vector2){cx - rad * 0.1f, cy - rad * 0.52f}, rad * 0.52f, rad * 0.42f, -0.15f,
             (InkTone){.color = PAL_SUMI_HEAVY, .alpha = 0.46f * alpha}, 2, seed + 2u);
    ink_outline_tone(r, body, 7, true, 3.0f, k_shade_dir,
                     (InkTone){.color = PAL_SUMI_DARK, .alpha = 0.96f * alpha}, seed + 3u);

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
    ink_stroke(r, bow, 4,
               (BrushProfile){.w_max = 3.2f,
                              .w_min_ratio = 0.25f,
                              .entry = 0.15f,
                              .exit = 0.30f,
                              .dry = 0.2f,
                              .seed = seed + 4u},
               (InkTone){.color = PAL_SUMI_SCORCH, .alpha = 0.95f * alpha}, 1.0f);

    if (pull > 0.05f) {
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
                               uint32_t seed, uint64_t ticks, int act_index, bool is_storm,
                               float alpha)
{
    const float cx = tr->x;
    const float cy = tr->y;
    const float rad = tr->radius > 6.0f ? tr->radius : 18.0f;
    const float pull = en != NULL ? ink_clampf(en->windup, 0.0f, 1.0f) : 0.0f;
    const float pulse =
        0.85f + 0.15f * sinf((float)ticks * 0.14f + (float)(seed & 15u)) + 0.28f * pull;

    const Vector2 body[8] = {
        {cx - rad * 0.42f, cy - rad * 1.08f}, {cx + rad * 0.48f, cy - rad * 1.02f},
        {cx + rad * 0.96f, cy - rad * 0.32f}, {cx + rad * 0.88f, cy + rad * 0.56f},
        {cx + rad * 0.38f, cy + rad * 1.04f}, {cx - rad * 0.46f, cy + rad * 0.96f},
        {cx - rad * 0.92f, cy + rad * 0.40f}, {cx - rad * 0.84f, cy - rad * 0.42f},
    };
    ink_knockout(r, body, 8, 3.0f, act_index);
    draw_elite_rim(r, tr, en, seed);
    ink_fill_poly(r, body, 8, pal_color_alpha(PAL_GOFUN, 0.88f * alpha));

    const PaletteId glow_outer = is_storm ? PAL_AO : PAL_SHU;
    const PaletteId glow_hot = is_storm ? PAL_AO_HOT : PAL_SHU_HOT;

    ink_wash(r, (Vector2){cx, cy + 1.0f}, rad * 0.56f * pulse, rad * 0.62f * pulse, 0.08f,
             (InkTone){.color = glow_outer, .alpha = 0.68f * alpha}, 2, seed + 1u);
    ink_wash(r, (Vector2){cx, cy + 1.0f}, rad * 0.30f * pulse, rad * 0.34f * pulse, -0.1f,
             (InkTone){.color = glow_hot, .alpha = 0.88f * alpha}, 1, seed + 2u);

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

    if (is_storm) {
        /* Flickering short azure lightning spark */
        const float jitter = ((ticks / 4u + seed) & 1u) ? 4.0f : -4.0f;
        const Vector2 spark[4] = {
            {cx - rad * 0.5f, cy + rad * 0.35f},
            {cx - 2.0f, cy + rad * 0.65f + jitter * 0.4f},
            {cx + 5.0f, cy + rad * 0.55f},
            {cx + rad * 0.65f, cy + rad * 1.05f + pull * 4.0f},
        };
        ink_stroke(r, spark, 4,
                   (BrushProfile){.w_max = 2.6f,
                                  .w_min_ratio = 0.30f,
                                  .entry = 0.1f,
                                  .exit = 0.35f,
                                  .dry = 0.0f,
                                  .seed = seed + 16u},
                   (InkTone){.color = PAL_AO_HOT, .alpha = 0.94f * alpha}, 1.0f);
    } else {
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
    }

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
    draw_elite_rim(r, tr, en, seed);
    ink_wash(r, (Vector2){cx, cy - rad * 0.22f}, rad * 0.98f, rad * 0.62f * squash, -0.08f,
             (InkTone){.color = PAL_SUMI_LIGHT, .alpha = 0.58f * alpha}, 2, seed + 1u);

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

    draw_eye_brush(r, (Vector2){cx + rad * 0.08f, cy - rad * 0.24f}, 7.0f, PAL_GOFUN, alpha);
    draw_eye_brush(r, (Vector2){cx + rad * 0.10f, cy - rad * 0.24f}, 3.6f, PAL_SUMI_SCORCH, alpha);
}

static void draw_enemy_kamaitachi(RenderState *r, const TransformComp *tr, const VelocityComp *vel,
                                  const EnemyComp *en, uint32_t seed, int act_index, float alpha)
{
    const float cx = tr->x;
    const float cy = tr->y;
    const float rad = tr->radius > 6.0f ? tr->radius : 16.0f;
    const float vx = vel != NULL ? vel->vx : cosf(tr->facing) * 120.0f;
    const float vy = vel != NULL ? vel->vy : sinf(tr->facing) * 120.0f;
    const float sp = sqrtf(vx * vx + vy * vy);
    const Vector2 dir = sp > 1.0f ? (Vector2){vx / sp, vy / sp} : (Vector2){1.0f, 0.0f};
    const Vector2 side = {-dir.y, dir.x};

    /* 0.3s pale-ink trailing afterimage behind motion */
    const Vector2 tail[4] = {
        {cx - dir.x * rad * 0.4f, cy - dir.y * rad * 0.4f},
        {cx - dir.x * rad * 1.4f + side.x * 7.0f, cy - dir.y * rad * 1.4f + side.y * 7.0f},
        {cx - dir.x * rad * 2.4f - side.x * 5.0f, cy - dir.y * rad * 2.4f - side.y * 5.0f},
        {cx - dir.x * rad * 3.1f, cy - dir.y * rad * 3.1f},
    };
    ink_stroke(r, tail, 4,
               (BrushProfile){.w_max = 5.0f,
                              .w_min_ratio = 0.18f,
                              .entry = 0.08f,
                              .exit = 0.65f,
                              .dry = 0.55f,
                              .seed = seed + 1u},
               (InkTone){.color = PAL_SUMI_CLEAR, .alpha = 0.58f * alpha}, 1.0f);

    /* Knockout polygon around sleek crescent body */
    const Vector2 ko[6] = {
        {cx + dir.x * rad * 1.15f, cy + dir.y * rad * 1.15f},
        {cx + side.x * rad * 0.95f, cy + side.y * rad * 0.95f},
        {cx - dir.x * rad * 0.95f + side.x * rad * 0.4f,
         cy - dir.y * rad * 0.95f + side.y * rad * 0.4f},
        {cx - dir.x * rad * 1.05f, cy - dir.y * rad * 1.05f},
        {cx - dir.x * rad * 0.7f - side.x * rad * 0.6f,
         cy - dir.y * rad * 0.7f - side.y * rad * 0.6f},
        {cx - side.x * rad * 0.85f, cy - side.y * rad * 0.85f},
    };
    ink_knockout(r, ko, 6, 3.0f, act_index);
    draw_elite_rim(r, tr, en, seed);

    ink_wash(r, (Vector2){cx, cy}, rad * 0.80f, rad * 0.52f, atan2f(dir.y, dir.x),
             (InkTone){.color = PAL_SUMI_LIGHT, .alpha = 0.48f * alpha}, 2, seed + 2u);

    /* 3 sharp sickle flick strokes (w_max = 3.0px) */
    for (int k = -1; k <= 1; ++k) {
        const float off = (float)k * rad * 0.42f;
        const Vector2 sickle[3] = {
            {cx - dir.x * rad * 0.85f + side.x * (off - 3.0f),
             cy - dir.y * rad * 0.85f + side.y * (off - 3.0f)},
            {cx + dir.x * rad * 0.15f + side.x * (off + (float)k * 6.0f),
             cy + dir.y * rad * 0.15f + side.y * (off + (float)k * 6.0f)},
            {cx + dir.x * rad * 1.15f + side.x * (off * 0.3f),
             cy + dir.y * rad * 1.15f + side.y * (off * 0.3f)},
        };
        ink_stroke(r, sickle, 3,
                   (BrushProfile){.w_max = 3.0f,
                                  .w_min_ratio = 0.20f,
                                  .entry = 0.15f,
                                  .exit = 0.55f,
                                  .dry = 0.25f,
                                  .seed = seed + 10u + (uint32_t)(k + 1)},
                   (InkTone){.color = PAL_SUMI_DARK, .alpha = 0.96f * alpha}, 1.0f);
    }

    draw_eye_brush(r, (Vector2){cx + dir.x * rad * 0.35f, cy + dir.y * rad * 0.35f}, 3.2f,
                   PAL_GOFUN, alpha);
}

static void draw_enemy_stone_oni(RenderState *r, const TransformComp *tr, const EnemyComp *en,
                                 uint32_t seed, int act_index, float alpha)
{
    const float cx = tr->x;
    const float cy = tr->y;
    const float rad = tr->radius > 8.0f ? tr->radius : 24.0f;

    /* Stocky angular rock-oni silhouette (thickest regular enemy: w_max = 6.0px) */
    const Vector2 body[8] = {
        {cx - rad * 0.88f, cy - rad * 0.42f}, {cx - rad * 0.48f, cy - rad * 0.98f},
        {cx + rad * 0.42f, cy - rad * 0.94f}, {cx + rad * 0.96f, cy - rad * 0.35f},
        {cx + rad * 0.86f, cy + rad * 0.68f}, {cx + rad * 0.30f, cy + rad * 0.98f},
        {cx - rad * 0.45f, cy + rad * 0.94f}, {cx - rad * 0.94f, cy + rad * 0.55f},
    };
    ink_knockout(r, body, 8, 4.0f, act_index);
    draw_elite_rim(r, tr, en, seed);

    ink_wash(r, (Vector2){cx, cy + 2.0f}, rad * 0.92f, rad * 0.88f, 0.06f,
             (InkTone){.color = PAL_SUMI_HEAVY, .alpha = 0.72f * alpha}, 2, seed + 1u);
    ink_wash(r, (Vector2){cx - 3.0f, cy - 3.0f}, rad * 0.65f, rad * 0.62f, -0.14f,
             (InkTone){.color = PAL_SUMI_DARK, .alpha = 0.48f * alpha}, 2, seed + 2u);

    /* Angular Sesshu-style shunpo rock facets & cracks */
    const Vector2 cr1[3] = {{cx - rad * 0.55f, cy - rad * 0.15f},
                            {cx - rad * 0.08f, cy + rad * 0.08f},
                            {cx + rad * 0.50f, cy - rad * 0.22f}};
    const Vector2 cr2[2] = {{cx - rad * 0.22f, cy + rad * 0.24f},
                            {cx + rad * 0.32f, cy + rad * 0.60f}};
    ink_stroke(r, cr1, 3,
               (BrushProfile){.w_max = 3.2f,
                              .w_min_ratio = 0.28f,
                              .entry = 0.12f,
                              .exit = 0.35f,
                              .dry = 0.30f,
                              .seed = seed + 6u},
               (InkTone){.color = PAL_SUMI_SCORCH, .alpha = 0.85f * alpha}, 1.0f);
    ink_stroke(r, cr2, 2,
               (BrushProfile){.w_max = 2.8f,
                              .w_min_ratio = 0.30f,
                              .entry = 0.12f,
                              .exit = 0.35f,
                              .dry = 0.30f,
                              .seed = seed + 7u},
               (InkTone){.color = PAL_SUMI_SCORCH, .alpha = 0.80f * alpha}, 1.0f);

    /* Horns */
    const Vector2 lh[2] = {{cx - rad * 0.45f, cy - rad * 0.82f},
                           {cx - rad * 0.68f, cy - rad * 1.24f}};
    const Vector2 rh[2] = {{cx + rad * 0.42f, cy - rad * 0.78f},
                           {cx + rad * 0.66f, cy - rad * 1.20f}};
    const BrushProfile hp = {.w_max = 4.6f,
                             .w_min_ratio = 0.22f,
                             .entry = 0.10f,
                             .exit = 0.60f,
                             .dry = 0.15f,
                             .seed = seed + 8u};
    ink_stroke(r, lh, 2, hp, (InkTone){.color = PAL_SUMI_SCORCH, .alpha = 0.96f * alpha}, 1.0f);
    ink_stroke(r, rh, 2, hp, (InkTone){.color = PAL_SUMI_SCORCH, .alpha = 0.96f * alpha}, 1.0f);

    ink_outline_tone(r, body, 8, true, 6.0f, k_shade_dir,
                     (InkTone){.color = PAL_SUMI_SCORCH, .alpha = 0.98f * alpha}, seed + 3u);

    draw_eye_brush(r, (Vector2){cx - rad * 0.26f, cy - rad * 0.30f}, 4.8f, PAL_GOFUN, alpha);
    draw_eye_brush(r, (Vector2){cx + rad * 0.26f, cy - rad * 0.30f}, 4.8f, PAL_GOFUN, alpha);
}

static void draw_enemy_ink_leech(RenderState *r, const TransformComp *tr, const EnemyComp *en,
                                 uint32_t seed, int act_index, float alpha)
{
    /* Sole exception with NO ink_outline: knockout + 3-layer ink_wash + 2 gofun eyes */
    const float cx = tr->x;
    const float cy = tr->y;
    const float absorbed = en != NULL ? ink_clampf(en->absorbed_ink, 0.0f, 3.0f) : 0.0f;
    const float base_rad = tr->radius > 6.0f ? tr->radius : 18.0f;
    const float rad = base_rad * (1.0f + absorbed * 0.18f);

    Vector2 ko[8];
    for (int i = 0; i < 8; ++i) {
        const float a = k_ent_two_pi * (float)i / 8.0f;
        const float j = 0.88f + 0.24f * ((float)((seed + (uint32_t)i * 13u) & 7u) / 7.0f);
        ko[i] = (Vector2){cx + cosf(a) * rad * j, cy + sinf(a) * rad * j};
    }
    ink_knockout(r, ko, 8, 3.5f, act_index);
    draw_elite_rim(r, tr, en, seed);

    ink_wash(r, (Vector2){cx, cy}, rad * 1.05f, rad * 0.94f, 0.25f,
             (InkTone){.color = PAL_SUMI_DARK, .alpha = 0.80f * alpha}, 3, seed + 1u);
    ink_wash(r, (Vector2){cx + 2.0f, cy - 1.5f}, rad * 0.68f, rad * 0.62f, -0.4f,
             (InkTone){.color = PAL_SUMI_SCORCH, .alpha = 0.72f * alpha}, 2, seed + 2u);

    draw_eye_brush(r, (Vector2){cx - rad * 0.24f, cy - rad * 0.14f}, 4.2f, PAL_GOFUN, alpha);
    draw_eye_brush(r, (Vector2){cx + rad * 0.26f, cy - rad * 0.10f}, 3.6f, PAL_GOFUN, alpha);
}

static void draw_enemy_ofuda_fox(RenderState *r, const TransformComp *tr, const EnemyComp *en,
                                 uint32_t seed, uint64_t ticks, int act_index, float alpha)
{
    const float cx = tr->x;
    const float cy = tr->y;
    const float rad = tr->radius > 6.0f ? tr->radius : 17.0f;
    const float pull = en != NULL ? ink_clampf(en->windup, 0.0f, 1.0f) : 0.0f;
    const float sway = sinf((float)ticks * 0.12f + (float)(seed & 31u)) * 6.5f;

    /* Gofun fox mask silhouette with tall pointed ears & tapered snout */
    const Vector2 mask[9] = {
        {cx - rad * 0.88f, cy - rad * 0.12f},
        {cx - rad * 0.72f, cy - rad * 1.18f}, /* left ear tip */
        {cx - rad * 0.22f, cy - rad * 0.62f},
        {cx + rad * 0.24f, cy - rad * 0.64f},
        {cx + rad * 0.76f, cy - rad * 1.14f}, /* right ear tip */
        {cx + rad * 0.92f, cy - rad * 0.06f},
        {cx + rad * 0.48f, cy + rad * 0.68f},
        {cx + rad * 0.04f, cy + rad * 1.04f}, /* pointed chin */
        {cx - rad * 0.48f, cy + rad * 0.62f},
    };
    ink_knockout(r, mask, 9, 3.5f, act_index);
    draw_elite_rim(r, tr, en, seed);

    /* Sweeping brush-flick fox tail swaying behind mask */
    const Vector2 tail[4] = {
        {cx + rad * 0.35f, cy + rad * 0.55f},
        {cx + rad * 1.05f, cy + rad * 0.72f + sway * 0.35f},
        {cx + rad * 1.62f, cy + rad * 0.22f + sway * 0.75f},
        {cx + rad * 1.95f, cy - rad * 0.38f + sway},
    };
    ink_wash(r, (Vector2){cx + rad * 1.15f, cy + rad * 0.42f + sway * 0.4f}, rad * 0.75f,
             rad * 0.34f, -0.35f, (InkTone){.color = PAL_SUMI_LIGHT, .alpha = 0.46f * alpha}, 2,
             seed + 1u);
    ink_stroke(r, tail, 4,
               (BrushProfile){.w_max = 8.2f,
                              .w_min_ratio = 0.16f,
                              .entry = 0.14f,
                              .exit = 0.58f,
                              .dry = 0.48f,
                              .seed = seed + 2u},
               (InkTone){.color = PAL_SUMI_DARK, .alpha = 0.94f * alpha}, 1.0f);

    /* Mask fill + subtle pale shading + crisp 3px dark sumi outline */
    ink_fill_poly(r, mask, 9, pal_color_alpha(PAL_GOFUN, 0.96f * alpha));
    ink_wash(r, (Vector2){cx + 1.5f, cy + 2.0f}, rad * 0.52f, rad * 0.50f, 0.10f,
             (InkTone){.color = PAL_SUMI_CLEAR, .alpha = 0.30f * alpha}, 1, seed + 3u);
    ink_outline_tone(r, mask, 9, true, 3.0f, k_shade_dir,
                     (InkTone){.color = PAL_SUMI_DARK, .alpha = 0.98f * alpha}, seed + 4u);

    /* Narrow fox eyes + single vermilion eye-corner brush accent */
    draw_eye_brush(r, (Vector2){cx - rad * 0.30f, cy - rad * 0.04f}, 4.2f, PAL_SUMI_SCORCH, alpha);
    draw_eye_brush(r, (Vector2){cx + rad * 0.32f, cy - rad * 0.06f}, 4.2f, PAL_SUMI_SCORCH, alpha);
    const Vector2 l_shu[2] = {
        {cx - rad * 0.44f, cy - rad * 0.08f},
        {cx - rad * 0.64f, cy - rad * 0.24f},
    };
    const Vector2 r_shu[2] = {
        {cx + rad * 0.46f, cy - rad * 0.10f},
        {cx + rad * 0.66f, cy - rad * 0.26f},
    };
    const BrushProfile shu_p = {.w_max = 2.4f,
                                .w_min_ratio = 0.22f,
                                .entry = 0.15f,
                                .exit = 0.55f,
                                .dry = 0.15f,
                                .seed = seed + 8u};
    ink_stroke(r, l_shu, 2, shu_p, (InkTone){.color = PAL_SHU, .alpha = 0.92f * alpha}, 1.0f);
    ink_stroke(r, r_shu, 2, shu_p, (InkTone){.color = PAL_SHU, .alpha = 0.92f * alpha}, 1.0f);

    /* Telegraph: floating pale talismans around the mask before barrier cast or shot */
    if (pull > 0.05f) {
        const float orbit = (float)ticks * 0.09f;
        const float orad = rad * (1.28f + 0.18f * pull);
        for (int k = 0; k < 2; ++k) {
            const float a = orbit + (float)k * k_ent_pi;
            const Vector2 fc = {cx + cosf(a) * orad, cy + sinf(a) * orad};
            const Vector2 slip[4] = {
                {fc.x - 4.0f, fc.y - 6.5f},
                {fc.x + 4.5f, fc.y - 5.8f},
                {fc.x + 3.8f, fc.y + 6.5f},
                {fc.x - 4.4f, fc.y + 5.8f},
            };
            ink_fill_poly(r, slip, 4, pal_color_alpha(PAL_GOFUN, (0.55f + 0.40f * pull) * alpha));
            ink_outline_tone(r, slip, 4, true, 1.8f, k_shade_dir,
                             (InkTone){.color = PAL_SHU, .alpha = (0.50f + 0.45f * pull) * alpha},
                             seed + 14u + (uint32_t)k);
        }
    }
}

static void draw_boss_tengu(RenderState *r, const TransformComp *tr, const BossComp *boss,
                            const ShrineState *shrine, uint32_t seed, int act_index, float alpha)
{
    const float cx = tr->x;
    const float cy = tr->y;
    const float rad = tr->radius > 10.0f ? tr->radius : 44.0f;

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

    const Vector2 ko[8] = {
        {cx - rad * 1.65f, cy - rad * 0.35f}, {cx - rad * 0.80f, cy - rad * 1.10f},
        {cx + rad * 0.08f, cy - rad * 1.05f}, {cx + rad * 0.90f, cy - rad * 1.08f},
        {cx + rad * 1.72f, cy - rad * 0.25f}, {cx + rad * 0.95f, cy + rad * 0.92f},
        {cx - rad * 0.04f, cy + rad * 1.15f}, {cx - rad * 0.98f, cy + rad * 0.88f},
    };
    ink_knockout(r, ko, 8, 5.0f, act_index);

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
            ink_stroke(r, wpts, 3,
                       (BrushProfile){.w_max = 10.5f - (float)f * 0.9f,
                                      .w_min_ratio = 0.20f,
                                      .entry = 0.12f,
                                      .exit = 0.48f,
                                      .dry = 0.55f,
                                      .seed = seed + 30u + (uint32_t)(side > 0 ? 10 + f : f)},
                       (InkTone){.color = (f % 2 == 0) ? PAL_SUMI_DARK : PAL_SUMI_HEAVY,
                                 .alpha = 0.90f * alpha},
                       1.0f);
        }
    }

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

    draw_eye_brush(r, (Vector2){cx - rad * 0.26f, cy - rad * 0.24f}, 7.2f, PAL_GOFUN, alpha);
    draw_eye_brush(r, (Vector2){cx + rad * 0.26f, cy - rad * 0.24f}, 7.2f, PAL_GOFUN, alpha);
    draw_eye_brush(r, (Vector2){cx - rad * 0.24f, cy - rad * 0.24f}, 3.6f, PAL_SUMI_SCORCH, alpha);
    draw_eye_brush(r, (Vector2){cx + rad * 0.24f, cy - rad * 0.24f}, 3.6f, PAL_SUMI_SCORCH, alpha);

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

static void draw_boss_nue(RenderState *r, const TransformComp *tr, const EnemyComp *en,
                          const BossComp *boss, uint32_t seed, int act_index, float alpha)
{
    const float cx = tr->x;
    const float cy = tr->y;
    const float rad = tr->radius > 10.0f ? tr->radius : 40.0f;
    const bool paused = en != NULL && en->ai_state == 1;
    const float crouch_y = paused ? 6.0f : 0.0f;
    const float outline_w = paused ? (5.8f * 1.3f) : 5.8f;

    /* Azure lightning dash telegraph */
    if (boss != NULL && boss->telegraph > 0.01f) {
        const float dx = (boss->gale_dx != 0.0f || boss->gale_dy != 0.0f) ? boss->gale_dx : 1.0f;
        const float dy = (boss->gale_dx != 0.0f || boss->gale_dy != 0.0f) ? boss->gale_dy : 0.0f;
        const float len = 240.0f;
        const Vector2 dst = {cx + dx * len, cy + dy * len};
        const InkTone warn = {.color = PAL_AO, .alpha = 0.65f + 0.30f * boss->telegraph};
        ink_dashed_stroke(r, (Vector2){cx, cy}, dst, 20.0f, 10.0f, 4.2f, warn, seed + 88u);
        const Vector2 core[3] = {
            {cx, cy},
            {cx + dx * len * 0.5f - dy * 10.0f, cy + dy * len * 0.5f + dx * 10.0f},
            dst,
        };
        ink_stroke(r, core, 3,
                   (BrushProfile){.w_max = 2.0f,
                                  .w_min_ratio = 0.4f,
                                  .entry = 0.1f,
                                  .exit = 0.2f,
                                  .dry = 0.0f,
                                  .seed = seed + 89u},
                   (InkTone){.color = PAL_AO_HOT, .alpha = 0.90f}, 1.0f);
    }

    const Vector2 ko[8] = {
        {cx - rad * 1.35f, cy - rad * 0.45f + crouch_y},
        {cx - rad * 0.55f, cy - rad * 1.05f + crouch_y},
        {cx + rad * 0.55f, cy - rad * 1.00f + crouch_y},
        {cx + rad * 1.40f, cy - rad * 0.35f + crouch_y},
        {cx + rad * 1.25f, cy + rad * 0.85f},
        {cx + rad * 0.20f, cy + rad * 1.08f},
        {cx - rad * 0.80f, cy + rad * 0.98f},
        {cx - rad * 1.35f, cy + rad * 0.42f},
    };
    ink_knockout(r, ko, 8, 5.0f, act_index);

    /* Black thundercloud aura */
    ink_wash(r, (Vector2){cx, cy + 4.0f}, rad * 1.32f, rad * 0.92f, -0.06f,
             (InkTone){.color = PAL_SUMI_LIGHT, .alpha = 0.45f * alpha}, 2, seed + 1u);

    /* Tanuki torso (mokkotsu wash + crouch when paused) */
    const Vector2 torso[7] = {
        {cx - rad * 0.82f, cy - rad * 0.25f + crouch_y},
        {cx - rad * 0.25f, cy - rad * 0.72f + crouch_y},
        {cx + rad * 0.65f, cy - rad * 0.52f + crouch_y},
        {cx + rad * 0.94f, cy + rad * 0.24f},
        {cx + rad * 0.55f, cy + rad * 0.76f},
        {cx - rad * 0.45f, cy + rad * 0.78f},
        {cx - rad * 0.92f, cy + rad * 0.32f},
    };
    ink_wash(r, (Vector2){cx + 4.0f, cy + crouch_y * 0.5f}, rad * 0.88f, rad * 0.66f, 0.05f,
             (InkTone){.color = PAL_SUMI_HEAVY, .alpha = 0.66f * alpha}, 2, seed + 2u);
    ink_outline_tone(r, torso, 7, true, outline_w, k_shade_dir,
                     (InkTone){.color = PAL_SUMI_DARK, .alpha = 0.98f * alpha}, seed + 3u);

    /* Tiger-striped legs (bold dark sumi strokes) */
    for (int leg = 0; leg < 2; ++leg) {
        const float lx = cx + (leg == 0 ? -rad * 0.52f : rad * 0.46f);
        const Vector2 lpts[3] = {{lx, cy + rad * 0.42f},
                                 {lx + (leg == 0 ? -8.0f : 8.0f), cy + rad * 0.86f},
                                 {lx + (leg == 0 ? -14.0f : 12.0f), cy + rad * 1.12f}};
        ink_stroke(r, lpts, 3,
                   (BrushProfile){.w_max = paused ? 8.2f : 6.4f,
                                  .w_min_ratio = 0.28f,
                                  .entry = 0.12f,
                                  .exit = 0.30f,
                                  .dry = 0.15f,
                                  .seed = seed + 12u + (uint32_t)leg},
                   (InkTone){.color = PAL_SUMI_SCORCH, .alpha = 0.95f * alpha}, 1.0f);
    }

    /* Snake tail sweeping behind */
    const Vector2 stail[4] = {
        {cx + rad * 0.78f, cy + rad * 0.25f},
        {cx + rad * 1.25f, cy + rad * 0.05f},
        {cx + rad * 1.48f, cy - rad * 0.42f},
        {cx + rad * 1.22f, cy - rad * 0.82f},
    };
    ink_stroke(r, stail, 4,
               (BrushProfile){.w_max = 4.6f,
                              .w_min_ratio = 0.22f,
                              .entry = 0.10f,
                              .exit = 0.55f,
                              .dry = 0.25f,
                              .seed = seed + 20u},
               (InkTone){.color = PAL_SUMI_DARK, .alpha = 0.94f * alpha}, 1.0f);

    /* Gofun monkey face mask */
    const Vector2 face[6] = {
        {cx - rad * 0.66f, cy - rad * 0.52f + crouch_y},
        {cx - rad * 0.22f, cy - rad * 0.86f + crouch_y},
        {cx + rad * 0.20f, cy - rad * 0.62f + crouch_y},
        {cx + rad * 0.16f, cy - rad * 0.08f + crouch_y},
        {cx - rad * 0.28f, cy + rad * 0.06f + crouch_y},
        {cx - rad * 0.72f, cy - rad * 0.12f + crouch_y},
    };
    ink_fill_poly(r, face, 6, pal_color_alpha(PAL_GOFUN, 0.94f * alpha));
    ink_outline_tone(r, face, 6, true, outline_w * 0.72f, k_shade_dir,
                     (InkTone){.color = PAL_SUMI_SCORCH, .alpha = 0.96f * alpha}, seed + 24u);
    draw_eye_brush(r, (Vector2){cx - rad * 0.42f, cy - rad * 0.36f + crouch_y}, 4.8f, PAL_AO_HOT,
                   alpha);
    draw_eye_brush(r, (Vector2){cx - rad * 0.08f, cy - rad * 0.36f + crouch_y}, 4.8f, PAL_AO_HOT,
                   alpha);

    /* Crackling azure lightning arcs clinging to cloud */
    const Vector2 z1[4] = {
        {cx - rad * 1.10f, cy - rad * 0.20f},
        {cx - rad * 0.75f, cy - rad * 0.78f},
        {cx - rad * 0.30f, cy - rad * 0.58f},
        {cx + rad * 0.15f, cy - rad * 0.96f},
    };
    ink_stroke(r, z1, 4,
               (BrushProfile){.w_max = 3.2f,
                              .w_min_ratio = 0.32f,
                              .entry = 0.15f,
                              .exit = 0.30f,
                              .dry = 0.0f,
                              .seed = seed + 30u},
               (InkTone){.color = PAL_AO, .alpha = 0.90f * alpha}, 1.0f);
    ink_stroke(r, z1, 4,
               (BrushProfile){.w_max = 1.5f,
                              .w_min_ratio = 0.40f,
                              .entry = 0.10f,
                              .exit = 0.15f,
                              .dry = 0.0f,
                              .seed = seed + 31u},
               (InkTone){.color = PAL_AO_HOT, .alpha = 0.95f * alpha}, 1.0f);
}

static int resample_ryu_body_trail(const RyuBodyState *body, Vector2 hx_hy,
                                   Vector2 out[RENDER_RYU_TRAIL_LEN])
{
    int cnt = body->trail_count;
    if (cnt < 2) {
        return 0;
    }
    if (cnt > GAME_RYU_TRAIL_CAP) {
        cnt = GAME_RYU_TRAIL_CAP;
    }
    int head = body->trail_head % GAME_RYU_TRAIL_CAP;
    if (head < 0) {
        head += GAME_RYU_TRAIL_CAP;
    }

    Vector2 raw[GAME_RYU_TRAIL_CAP];
    float cum[GAME_RYU_TRAIL_CAP];
    cum[0] = 0.0f;
    for (int k = 0; k < cnt; ++k) {
        const int idx = (head - k + GAME_RYU_TRAIL_CAP) % GAME_RYU_TRAIL_CAP;
        raw[k] = (Vector2){body->trail[idx].x, body->trail[idx].y};
        if (k == 0) {
            raw[0] = hx_hy;
        } else {
            const float dx = raw[k].x - raw[k - 1].x;
            const float dy = raw[k].y - raw[k - 1].y;
            cum[k] = cum[k - 1] + sqrtf(dx * dx + dy * dy);
        }
    }

    const float total_len = cum[cnt - 1];
    if (total_len <= 1e-3f) {
        return 0;
    }

    out[0] = raw[0];
    int seg = 0;
    for (int i = 1; i < RENDER_RYU_TRAIL_LEN; ++i) {
        const float target = total_len * ((float)i / (float)(RENDER_RYU_TRAIL_LEN - 1));
        while (seg < cnt - 2 && cum[seg + 1] < target) {
            ++seg;
        }
        const float seg_len = cum[seg + 1] - cum[seg];
        const float t =
            seg_len > 1e-4f ? ink_clampf((target - cum[seg]) / seg_len, 0.0f, 1.0f) : 0.0f;
        out[i] = (Vector2){
            raw[seg].x + (raw[seg + 1].x - raw[seg].x) * t,
            raw[seg].y + (raw[seg + 1].y - raw[seg].y) * t,
        };
    }
    return RENDER_RYU_TRAIL_LEN;
}

static void draw_boss_ryu_orb(RenderState *r, const TransformComp *tr, const BossComp *boss,
                              uint32_t seed, uint64_t ticks, int act_index, float alpha)
{
    const float cx = tr->x;
    const float cy = tr->y;
    const float base_rad = tr->radius > 10.0f ? tr->radius : 30.0f;
    const int32_t part = boss != NULL ? boss->part_id : 0;
    const float tele = boss != NULL ? ink_clampf(boss->telegraph, 0.0f, 1.0f) : 0.0f;
    const float pulse =
        1.0f + 0.05f * sinf((float)ticks * 0.16f + (float)part * 2.1f) + 0.22f * tele;
    const float rad = base_rad * pulse;

    /* Pointed-top hoju (sacred jewel) silhouette */
    const Vector2 jewel[7] = {
        {cx, cy - rad * 1.18f},
        {cx + rad * 0.72f, cy - rad * 0.42f},
        {cx + rad * 0.94f, cy + rad * 0.28f},
        {cx + rad * 0.46f, cy + rad * 0.88f},
        {cx - rad * 0.48f, cy + rad * 0.86f},
        {cx - rad * 0.92f, cy + rad * 0.24f},
        {cx - rad * 0.70f, cy - rad * 0.44f},
    };
    ink_knockout(r, jewel, 7, 4.5f, act_index);

    if (part == 0) {
        /* Fire Orb: Shu body + Shu Hot core + flame rim flicks */
        ink_wash(r, (Vector2){cx, cy + 2.0f}, rad * 1.08f, rad * 1.12f, 0.06f,
                 (InkTone){.color = PAL_SHU, .alpha = (0.62f + 0.28f * tele) * alpha}, 2,
                 seed + 1u);
        ink_fill_poly(r, jewel, 7, pal_color_alpha(PAL_SHU, 0.94f * alpha));
        ink_teardrop(r, (Vector2){cx, cy + rad * 0.12f}, (Vector2){0.0f, -1.0f},
                     rad * (0.85f + 0.35f * tele), rad * (0.68f + 0.22f * tele),
                     pal_color_alpha(PAL_SHU_HOT, 0.98f * alpha));
        for (int k = -1; k <= 1; k += 2) {
            const Vector2 rim[3] = {
                {cx + (float)k * rad * 0.82f, cy + rad * 0.28f},
                {cx + (float)k * rad * 1.12f, cy - rad * 0.36f},
                {cx + (float)k * rad * 0.42f, cy - rad * 1.24f},
            };
            ink_stroke(
                r, rim, 3,
                (BrushProfile){.w_max = 3.6f,
                               .w_min_ratio = 0.20f,
                               .entry = 0.14f,
                               .exit = 0.55f,
                               .dry = 0.22f,
                               .seed = seed + 12u + (uint32_t)(k + 1)},
                (InkTone){.color = tele > 0.01f ? PAL_SHU_HOT : PAL_SHU, .alpha = 0.94f * alpha},
                1.0f);
        }
    } else if (part == 1) {
        /* Storm Orb: Ao body + Ao Hot core + crackling lightning rim */
        ink_wash(r, (Vector2){cx, cy + 2.0f}, rad * 1.08f, rad * 1.12f, -0.06f,
                 (InkTone){.color = PAL_AO, .alpha = (0.62f + 0.28f * tele) * alpha}, 2, seed + 1u);
        ink_fill_poly(r, jewel, 7, pal_color_alpha(PAL_AO, 0.94f * alpha));
        ink_teardrop(r, (Vector2){cx, cy + rad * 0.12f}, (Vector2){0.0f, -1.0f},
                     rad * (0.85f + 0.35f * tele), rad * (0.68f + 0.22f * tele),
                     pal_color_alpha(PAL_AO_HOT, 0.98f * alpha));
        for (int k = -1; k <= 1; k += 2) {
            const Vector2 arc[4] = {
                {cx + (float)k * rad * 0.95f, cy + rad * 0.35f},
                {cx + (float)k * rad * 1.22f, cy - rad * 0.08f},
                {cx + (float)k * rad * 0.88f, cy - rad * 0.58f},
                {cx + (float)k * rad * 0.38f, cy - rad * 1.18f},
            };
            ink_stroke(r, arc, 4,
                       (BrushProfile){.w_max = 3.0f,
                                      .w_min_ratio = 0.28f,
                                      .entry = 0.12f,
                                      .exit = 0.42f,
                                      .dry = 0.0f,
                                      .seed = seed + 22u + (uint32_t)(k + 1)},
                       (InkTone){.color = PAL_AO_HOT, .alpha = 0.96f * alpha}, 1.0f);
        }
    } else {
        /* Wind Orb: Gofun body + pale/dark sumi vortex spiral (achromatic wind) */
        ink_wash(r, (Vector2){cx, cy + 2.0f}, rad * 1.08f, rad * 1.12f, 0.10f,
                 (InkTone){.color = PAL_SUMI_CLEAR, .alpha = (0.58f + 0.25f * tele) * alpha}, 2,
                 seed + 1u);
        ink_fill_poly(r, jewel, 7, pal_color_alpha(PAL_GOFUN, 0.97f * alpha));
        const float spin = r->vfx_time * 3.2f;
        ink_arc_stroke(r, (Vector2){cx, cy + 2.0f}, rad * 0.56f, spin, spin + 4.7f,
                       (BrushProfile){.w_max = 3.4f,
                                      .w_min_ratio = 0.24f,
                                      .entry = 0.14f,
                                      .exit = 0.42f,
                                      .dry = 0.20f,
                                      .seed = seed + 30u},
                       (InkTone){.color = tele > 0.01f ? PAL_SUMI_SCORCH : PAL_SUMI_LIGHT,
                                 .alpha = 0.94f * alpha});
        ink_arc_stroke(r, (Vector2){cx, cy + 2.0f}, rad * 0.28f, spin + 1.4f, spin + 5.6f,
                       (BrushProfile){.w_max = 2.2f,
                                      .w_min_ratio = 0.30f,
                                      .entry = 0.12f,
                                      .exit = 0.35f,
                                      .dry = 0.15f,
                                      .seed = seed + 31u},
                       (InkTone){.color = PAL_SUMI_DARK, .alpha = 0.90f * alpha});
    }

    if (tele > 0.01f) {
        const PaletteId glow = (part == 0) ? PAL_SHU_HOT : ((part == 1) ? PAL_AO_HOT : PAL_GOFUN);
        ink_wash(r, (Vector2){cx, cy}, rad * 0.62f, rad * 0.62f, 0.0f,
                 (InkTone){.color = glow, .alpha = 0.65f * tele * alpha}, 1, seed + 40u);
    }

    ink_outline_tone(r, jewel, 7, true, 3.8f, k_shade_dir,
                     (InkTone){.color = PAL_SUMI_SCORCH, .alpha = 0.96f * alpha}, seed + 5u);
}

static void draw_boss_ryu(RenderState *r, const GameState *g, EntityId id, const TransformComp *tr,
                          const BossComp *boss, const ShrineState *shrine, uint32_t seed,
                          int act_index, float alpha)
{
    const float hx = tr->x;
    const float hy = tr->y;
    const float rad = tr->radius > 10.0f ? tr->radius : 44.0f;
    const bool awakened = boss != NULL && boss->phase_idx >= 1;
    const bool ascending = boss != NULL && boss->phase_idx >= 2;
    const int32_t orb_kind = boss != NULL ? boss->part_id : 0;

    /* Dragon coil telegraph: crimson dashed circle radius 240px around shrine */
    if (boss != NULL && boss->telegraph > 0.01f && boss->telegraph_pattern == 3 && shrine != NULL) {
        const InkTone warn = {.color = PAL_SHU, .alpha = 0.62f + 0.30f * boss->telegraph};
        const float vr = 240.0f;
        const int segs = 16;
        for (int k = 0; k < segs; ++k) {
            const float a0 = k_ent_two_pi * (float)k / (float)segs;
            const float a1 = k_ent_two_pi * ((float)k + 0.58f) / (float)segs;
            const Vector2 p0 = {shrine->x + cosf(a0) * vr, shrine->y + sinf(a0) * vr};
            const Vector2 p1 = {shrine->x + cosf(a1) * vr, shrine->y + sinf(a1) * vr};
            ink_dashed_stroke(r, p0, p1, 18.0f, 8.0f, 3.8f, warn, seed + 120u + (uint32_t)k);
        }
    }

    /* Multi-part body trail (arc-length resampled 24 pts) or P1 render-side trail fallback */
    const bool has_body_trail = (g != NULL && g->ryu_body.head_id.index == id.index &&
                                 g->ryu_body.head_id.gen == id.gen && g->ryu_body.trail_count >= 2);
    const uint8_t broken_mask = has_body_trail ? g->ryu_body.orb_broken_mask : 0u;

    Vector2 spine[RENDER_RYU_TRAIL_LEN];
    int spine_n = 0;
    if (has_body_trail) {
        spine_n = resample_ryu_body_trail(&g->ryu_body, (Vector2){hx, hy}, spine);
    }
    if (spine_n < 4 && r->ryu_trail_id.index == id.index && r->ryu_trail_id.gen == id.gen &&
        r->ryu_trail_n >= 4) {
        spine_n = r->ryu_trail_n < RENDER_RYU_TRAIL_LEN ? r->ryu_trail_n : RENDER_RYU_TRAIL_LEN;
        for (int i = 0; i < spine_n; ++i) {
            spine[i] = r->ryu_trail[i];
        }
        spine[0] = (Vector2){hx, hy};
    }
    if (spine_n < 4) {
        spine_n = 18;
        for (int i = 0; i < spine_n; ++i) {
            const float u = (float)i / (float)(spine_n - 1);
            spine[i] = (Vector2){hx + u * 260.0f, hy + sinf(u * 5.2f) * 42.0f + u * 18.0f};
        }
    }

    /* Paper knockout behind spine & head so dragon never sinks into dark ink */
    for (int i = 0; i < spine_n; i += 2) {
        const float u = (float)i / (float)(spine_n > 1 ? spine_n - 1 : 1);
        const float r_seg = (1.0f - u * 0.65f) * 20.0f + 6.0f;
        const Vector2 p = spine[i];
        const Vector2 seg_ko[6] = {
            {p.x - r_seg, p.y - r_seg * 0.6f},
            {p.x, p.y - r_seg},
            {p.x + r_seg, p.y - r_seg * 0.6f},
            {p.x + r_seg, p.y + r_seg * 0.6f},
            {p.x, p.y + r_seg},
            {p.x - r_seg, p.y + r_seg * 0.6f},
        };
        ink_knockout(r, seg_ko, 6, 4.0f, act_index);
    }

    /* Continuous single-brush dragon body: w_max = 26px, dry = 0.6 on tail 30% */
    const PaletteId body_pal = awakened ? PAL_SUMI_SCORCH : PAL_SUMI_DARK;
    ink_stroke(r, spine, spine_n,
               (BrushProfile){.w_max = 26.0f,
                              .w_min_ratio = 0.14f,
                              .entry = 0.06f,
                              .exit = 0.35f,
                              .dry = 0.60f,
                              .seed = seed + 1u},
               (InkTone){.color = body_pal, .alpha = 0.96f * alpha}, 1.0f);

    /* Enhanced dry-brush fracture overlay on segments whose Dragon Orb has been shattered */
    if (broken_mask != 0u) {
        const PaletteId paper_pal = pal_paper_for_act(act_index);
        for (int seg = 0; seg < GAME_RYU_ORB_COUNT; ++seg) {
            if ((broken_mask & (uint8_t)(1u << (unsigned)seg)) == 0u) {
                continue;
            }
            const int i0 = (seg * (spine_n - 1)) / GAME_RYU_ORB_COUNT;
            int i1 = ((seg + 1) * (spine_n - 1)) / GAME_RYU_ORB_COUNT;
            if (i1 <= i0) {
                i1 = i0 + 1 < spine_n ? i0 + 1 : spine_n - 1;
            }
            const int count = i1 - i0 + 1;
            if (count >= 2) {
                const float seg_w = 18.0f - (float)seg * 3.5f;
                ink_stroke(r, &spine[i0], count,
                           (BrushProfile){.w_max = seg_w,
                                          .w_min_ratio = 0.18f,
                                          .entry = 0.12f,
                                          .exit = 0.35f,
                                          .dry = 0.95f,
                                          .seed = seed + 60u + (uint32_t)seg * 7u},
                           (InkTone){.color = paper_pal, .alpha = 0.56f * alpha}, 1.0f);
                ink_stroke(r, &spine[i0], count,
                           (BrushProfile){.w_max = seg_w * 0.48f,
                                          .w_min_ratio = 0.15f,
                                          .entry = 0.10f,
                                          .exit = 0.40f,
                                          .dry = 0.95f,
                                          .seed = seed + 90u + (uint32_t)seg * 11u},
                           (InkTone){.color = PAL_GOFUN, .alpha = 0.42f * alpha}, 1.0f);
            }
        }
    }

    /* 44 short shunpo scale ticks along the dragon body (thinned in broken orb segments) */
    const int scale_count = 44;
    for (int s = 0; s < scale_count; ++s) {
        const float norm_s = ((float)s + 0.5f) / (float)scale_count;
        const int third = (int)(norm_s * (float)GAME_RYU_ORB_COUNT);
        const int clamped_third = third < GAME_RYU_ORB_COUNT ? third : (GAME_RYU_ORB_COUNT - 1);
        if ((broken_mask & (uint8_t)(1u << (unsigned)clamped_third)) != 0u && (s % 2 == 0)) {
            continue;
        }
        const float pos_f = norm_s * (float)(spine_n - 1);
        int idx = (int)pos_f;
        if (idx >= spine_n - 1) {
            idx = spine_n - 2;
        }
        const float frac = pos_f - (float)idx;
        const Vector2 p0 = spine[idx];
        const Vector2 p1 = spine[idx + 1];
        const Vector2 p = {p0.x + (p1.x - p0.x) * frac, p0.y + (p1.y - p0.y) * frac};
        const float dx = p1.x - p0.x;
        const float dy = p1.y - p0.y;
        const float dl = sqrtf(dx * dx + dy * dy);
        const Vector2 nrm = dl > 1e-3f ? (Vector2){-dy / dl, dx / dl} : (Vector2){0.0f, 1.0f};
        const float side = (s & 1) ? 1.0f : -1.0f;
        const float taper = 1.0f - ((float)s / (float)scale_count) * 0.65f;
        const Vector2 tick[2] = {
            {p.x + nrm.x * side * 3.0f * taper, p.y + nrm.y * side * 3.0f * taper},
            {p.x + nrm.x * side * 12.0f * taper + (dl > 1e-3f ? (dx / dl) * 4.0f : 0.0f),
             p.y + nrm.y * side * 12.0f * taper + (dl > 1e-3f ? (dy / dl) * 4.0f : 0.0f)},
        };
        ink_stroke(r, tick, 2,
                   (BrushProfile){.w_max = 2.6f,
                                  .w_min_ratio = 0.25f,
                                  .entry = 0.15f,
                                  .exit = 0.45f,
                                  .dry = 0.35f,
                                  .seed = seed + 200u + (uint32_t)s},
                   (InkTone){.color = (s % 2 == 0) ? PAL_SUMI_LIGHT : PAL_SUMI_HEAVY,
                             .alpha = 0.72f * alpha},
                   1.0f);
    }

    /* Dragon head silhouette */
    const Vector2 head_poly[7] = {
        {hx - rad * 0.85f, hy - rad * 0.25f}, {hx - rad * 0.35f, hy - rad * 0.72f},
        {hx + rad * 0.52f, hy - rad * 0.68f}, {hx + rad * 0.86f, hy - rad * 0.05f},
        {hx + rad * 0.45f, hy + rad * 0.62f}, {hx - rad * 0.38f, hy + rad * 0.56f},
        {hx - rad * 0.88f, hy + rad * 0.22f},
    };
    ink_knockout(r, head_poly, 7, 5.0f, act_index);
    ink_wash(r, (Vector2){hx, hy}, rad * 0.76f, rad * 0.58f, -0.05f,
             (InkTone){.color = body_pal, .alpha = 0.88f * alpha}, 2, seed + 5u);
    ink_outline_tone(r, head_poly, 7, true, awakened ? 7.4f : 5.6f, k_shade_dir,
                     (InkTone){.color = PAL_SUMI_SCORCH, .alpha = 0.98f * alpha}, seed + 6u);

    /* 2 horns + 2 whiskers (subtle gold aura highlights during Phase 3 Ascending Dragon) */
    const Vector2 horn1[3] = {{hx + rad * 0.18f, hy - rad * 0.55f},
                              {hx + rad * 0.62f, hy - rad * 1.15f},
                              {hx + rad * 1.05f, hy - rad * 1.32f}};
    const Vector2 horn2[3] = {{hx + rad * 0.36f, hy - rad * 0.42f},
                              {hx + rad * 0.82f, hy - rad * 0.88f},
                              {hx + rad * 1.20f, hy - rad * 0.94f}};
    const Vector2 whisk1[3] = {{hx - rad * 0.65f, hy + rad * 0.10f},
                               {hx - rad * 1.18f, hy + rad * 0.32f},
                               {hx - rad * 1.55f, hy + rad * 0.12f}};
    const Vector2 whisk2[3] = {{hx - rad * 0.58f, hy + rad * 0.28f},
                               {hx - rad * 1.08f, hy + rad * 0.66f},
                               {hx - rad * 1.44f, hy + rad * 0.54f}};
    const BrushProfile fine_p = {.w_max = 3.8f,
                                 .w_min_ratio = 0.20f,
                                 .entry = 0.12f,
                                 .exit = 0.55f,
                                 .dry = 0.30f,
                                 .seed = seed + 18u};
    ink_stroke(r, horn1, 3, fine_p, (InkTone){.color = PAL_SUMI_SCORCH, .alpha = 0.95f * alpha},
               1.0f);
    ink_stroke(r, horn2, 3, fine_p, (InkTone){.color = PAL_SUMI_DARK, .alpha = 0.92f * alpha},
               1.0f);
    ink_stroke(r, whisk1, 3, fine_p, (InkTone){.color = PAL_SUMI_DARK, .alpha = 0.88f * alpha},
               1.0f);
    ink_stroke(r, whisk2, 3, fine_p, (InkTone){.color = PAL_SUMI_HEAVY, .alpha = 0.84f * alpha},
               1.0f);
    if (ascending) {
        const BrushProfile gold_p = {.w_max = 2.0f,
                                     .w_min_ratio = 0.25f,
                                     .entry = 0.18f,
                                     .exit = 0.55f,
                                     .dry = 0.20f,
                                     .seed = seed + 22u};
        const InkTone gold_t = {.color = PAL_KIN_HOT, .alpha = 0.82f * alpha};
        ink_stroke(r, horn1, 3, gold_p, gold_t, 1.0f);
        ink_stroke(r, horn2, 3, gold_p, gold_t, 1.0f);
        ink_stroke(r, whisk1, 3, gold_p, gold_t, 1.0f);
        ink_stroke(r, whisk2, 3, gold_p, gold_t, 1.0f);
    }

    /* Eye: blank gofun outline before awakening; large vermilion dot painted on awakening */
    const Vector2 eye_pos = {hx - rad * 0.18f, hy - rad * 0.16f};
    draw_eye_brush(r, eye_pos, 8.2f, PAL_GOFUN, alpha);
    ink_arc_stroke(r, eye_pos, 6.2f, 0.0f, k_ent_two_pi + 0.1f,
                   (BrushProfile){.w_max = 2.4f,
                                  .w_min_ratio = 0.35f,
                                  .entry = 0.1f,
                                  .exit = 0.1f,
                                  .dry = 0.0f,
                                  .seed = seed + 25u},
                   (InkTone){.color = PAL_SUMI_SCORCH, .alpha = 0.95f * alpha});
    if (awakened || r->ryu_awaken_t > 0.0f) {
        const float dot_scale =
            r->ryu_awaken_t > 0.0f ? (1.6f - 0.6f * (1.0f - r->ryu_awaken_t / 1.5f)) : 1.15f;
        draw_eye_brush(r, eye_pos, 5.2f * dot_scale, PAL_SHU, alpha);
        draw_eye_brush(r, eye_pos, 2.5f * dot_scale, PAL_SHU_HOT, alpha);
    }

    /* Legacy P1 snout orb (omitted when multi-part ECS Dragon Orb entities are active) */
    if (!has_body_trail) {
        const Vector2 orb_pos = {hx - rad * 0.92f, hy - rad * 0.42f};
        const bool tele = boss != NULL && boss->telegraph > 0.01f;
        const float orb_r = tele ? 16.5f : 13.0f;
        if (orb_kind == 0) {
            ink_teardrop(r, orb_pos, (Vector2){0.0f, -1.0f}, orb_r * 1.45f, orb_r * 1.2f,
                         pal_color_alpha(PAL_SHU, 0.94f * alpha));
            ink_teardrop(r, orb_pos, (Vector2){0.0f, -1.0f}, orb_r * (tele ? 0.95f : 0.72f),
                         orb_r * 0.72f, pal_color_alpha(PAL_SHU_HOT, 0.98f * alpha));
        } else if (orb_kind == 1) {
            ink_teardrop(r, orb_pos, (Vector2){0.0f, -1.0f}, orb_r * 1.45f, orb_r * 1.2f,
                         pal_color_alpha(PAL_AO, 0.94f * alpha));
            ink_teardrop(r, orb_pos, (Vector2){0.0f, -1.0f}, orb_r * (tele ? 0.95f : 0.72f),
                         orb_r * 0.72f, pal_color_alpha(PAL_AO_HOT, 0.98f * alpha));
        } else {
            ink_teardrop(r, orb_pos, (Vector2){0.0f, -1.0f}, orb_r * 1.45f, orb_r * 1.2f,
                         pal_color_alpha(PAL_GOFUN, 0.96f * alpha));
            ink_arc_stroke(
                r, orb_pos, orb_r * 0.58f, 0.2f, 5.2f,
                (BrushProfile){.w_max = 2.6f,
                               .w_min_ratio = 0.35f,
                               .entry = 0.1f,
                               .exit = 0.3f,
                               .dry = 0.15f,
                               .seed = seed + 33u},
                (InkTone){.color = tele ? PAL_SUMI_SCORCH : PAL_SUMI_LIGHT, .alpha = 0.90f});
        }
    }
}

static void draw_projectile(RenderState *r, const GameState *g, const TransformComp *tr,
                            const VelocityComp *vel, const ProjComp *pr, uint32_t seed,
                            int act_index)
{
    const float vx = vel != NULL ? vel->vx : 1.0f;
    const float vy = vel != NULL ? vel->vy : 0.0f;
    const float speed = sqrtf(vx * vx + vy * vy);
    const Vector2 dir = speed > 1e-3f ? (Vector2){vx / speed, vy / speed} : (Vector2){1.0f, 0.0f};
    const Vector2 pos = {tr->x, tr->y};

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
    } else if (pr->kind == PROJ_WIND_SICKLE) {
        /* Crescent flick (pale sumi blade + gofun bright rim) */
        const Vector2 side = {-dir.y, dir.x};
        const Vector2 arc[3] = {
            {pos.x - dir.x * 6.0f - side.x * (rad * 1.55f),
             pos.y - dir.y * 6.0f - side.y * (rad * 1.55f)},
            {pos.x + dir.x * (rad * 1.15f), pos.y + dir.y * (rad * 1.15f)},
            {pos.x - dir.x * 6.0f + side.x * (rad * 1.55f),
             pos.y - dir.y * 6.0f + side.y * (rad * 1.55f)},
        };
        ink_stroke(r, arc, 3,
                   (BrushProfile){.w_max = 5.4f,
                                  .w_min_ratio = 0.18f,
                                  .entry = 0.25f,
                                  .exit = 0.25f,
                                  .dry = 0.20f,
                                  .seed = seed + 14u},
                   (InkTone){.color = pr->burning ? PAL_SHU : PAL_SUMI_HEAVY, .alpha = 0.94f},
                   1.0f);
        ink_stroke(r, arc, 3,
                   (BrushProfile){.w_max = 2.0f,
                                  .w_min_ratio = 0.30f,
                                  .entry = 0.20f,
                                  .exit = 0.20f,
                                  .dry = 0.0f,
                                  .seed = seed + 15u},
                   (InkTone){.color = PAL_GOFUN, .alpha = 0.92f}, 1.0f);
    } else if (pr->kind == PROJ_HOMING_OFUDA) {
        /* Tilted gofun paper talisman slip + dark calligraphy strokes + crimson scorch in fire */
        const Vector2 side = {-dir.y, dir.x};
        const float half_l = rad * 1.45f;
        const float half_w = rad * 0.80f;
        const Vector2 slip[4] = {
            {pos.x - dir.x * half_l - side.x * half_w, pos.y - dir.y * half_l - side.y * half_w},
            {pos.x + dir.x * half_l - side.x * (half_w * 0.90f),
             pos.y + dir.y * half_l - side.y * (half_w * 0.90f)},
            {pos.x + dir.x * (half_l * 0.95f) + side.x * half_w,
             pos.y + dir.y * (half_l * 0.95f) + side.y * half_w},
            {pos.x - dir.x * (half_l * 1.04f) + side.x * (half_w * 0.92f),
             pos.y - dir.y * (half_l * 1.04f) + side.y * (half_w * 0.92f)},
        };
        ink_fill_poly(r, slip, 4, pal_color_alpha(PAL_GOFUN, 0.96f));
        ink_outline_tone(r, slip, 4, true, 3.0f, k_shade_dir,
                         (InkTone){.color = PAL_SUMI_DARK, .alpha = 0.95f}, seed + 16u);

        /* 3 short dark calligraphy ticks down the slip */
        for (int k = -1; k <= 1; ++k) {
            const float off = (float)k * (half_l * 0.45f);
            const Vector2 stroke_pts[2] = {
                {pos.x + dir.x * (off - 2.2f) - side.x * (half_w * 0.45f),
                 pos.y + dir.y * (off - 2.2f) - side.y * (half_w * 0.45f)},
                {pos.x + dir.x * (off + 2.2f) + side.x * (half_w * 0.45f),
                 pos.y + dir.y * (off + 2.2f) + side.y * (half_w * 0.45f)},
            };
            ink_stroke(r, stroke_pts, 2,
                       (BrushProfile){.w_max = 1.9f,
                                      .w_min_ratio = 0.30f,
                                      .entry = 0.14f,
                                      .exit = 0.35f,
                                      .dry = 0.15f,
                                      .seed = seed + 18u + (uint32_t)(k + 1)},
                       (InkTone){.color = PAL_SUMI_SCORCH, .alpha = 0.94f}, 1.0f);
        }

        /* Scorched crimson edge when surrounded by high fire concentration */
        float dye[FLUID_MAX_CHANNELS] = {0.0f};
        if (g != NULL) {
            fluid_sample(&g->fluid, pos.x, pos.y, NULL, NULL, dye);
        }
        if (pr->burning || dye[1] > 0.15f) {
            const float scorch_a = pr->burning ? 0.95f : ink_clampf(dye[1] * 2.4f, 0.35f, 0.95f);
            const Vector2 edge[3] = {slip[1], slip[2], slip[3]};
            ink_stroke(r, edge, 3,
                       (BrushProfile){.w_max = 3.2f,
                                      .w_min_ratio = 0.25f,
                                      .entry = 0.12f,
                                      .exit = 0.38f,
                                      .dry = 0.25f,
                                      .seed = seed + 22u},
                       (InkTone){.color = PAL_SHU, .alpha = scorch_a}, 1.0f);
            ink_teardrop(r, slip[1], dir, 5.5f, 3.6f, pal_color_alpha(PAL_SHU_HOT, scorch_a));
        }
    } else {
        /* PROJ_STORM_ORB and PROJ_BOUNCE_ORB */
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
        if (pr->kind == PROJ_BOUNCE_ORB) {
            const Vector2 z2[3] = {{pos.x + rad * 1.25f, pos.y + 3.0f},
                                   {pos.x + rad * 0.65f, pos.y + rad * 1.15f},
                                   {pos.x - 2.0f, pos.y + rad * 0.85f}};
            ink_stroke(r, z2, 3,
                       (BrushProfile){.w_max = 2.2f,
                                      .w_min_ratio = 0.35f,
                                      .entry = 0.15f,
                                      .exit = 0.35f,
                                      .dry = 0.0f,
                                      .seed = seed + 13u},
                       (InkTone){.color = PAL_AO, .alpha = 0.92f}, 1.0f);
        }
    }
}

static void draw_ink_orb(RenderState *r, const TransformComp *tr, const InkOrbComp *orb,
                         uint32_t seed, int act_index)
{
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
    ink_teardrop(r, (Vector2){cx - rad * 0.28f, cy - rad * 0.28f}, (Vector2){0.7f, -0.7f}, 3.6f,
                 2.6f, pal_color_alpha(PAL_KIN_HOT, 0.96f));
}

static void draw_gimmick_effects(RenderState *r, const GameState *g, uint64_t boil)
{
    const uint32_t hw = g->ecs.high_water;
    for (uint32_t i = 0; i < hw; ++i) {
        if (!g->ecs.alive[i]) {
            continue;
        }
        const EntityId id = {.index = i, .gen = g->ecs.gen[i]};
        const GimmickComp *gim = game_gimmick(g, id);
        const TransformComp *tr = game_transform(g, id);
        if (gim == NULL || tr == NULL) {
            continue;
        }
        const uint32_t seed = entity_seed(id, 4u, boil);
        if (gim->kind == GIMMICK_LIGHTNING_STRIKE) {
            /* Crimson dashed warning circle (r=32px) + short center strike tick */
            const float rad = tr->radius > 8.0f ? tr->radius : 32.0f;
            const InkTone warn = {.color = PAL_SHU, .alpha = 0.88f};
            const int segs = 10;
            for (int k = 0; k < segs; ++k) {
                const float a0 = k_ent_two_pi * (float)k / (float)segs;
                const float a1 = k_ent_two_pi * ((float)k + 0.55f) / (float)segs;
                const Vector2 p0 = {tr->x + cosf(a0) * rad, tr->y + sinf(a0) * rad};
                const Vector2 p1 = {tr->x + cosf(a1) * rad, tr->y + sinf(a1) * rad};
                ink_dashed_stroke(r, p0, p1, 10.0f, 5.0f, 2.8f, warn, seed + (uint32_t)k);
            }
            const Vector2 tick[2] = {{tr->x - 5.0f, tr->y - 9.0f}, {tr->x + 5.0f, tr->y + 9.0f}};
            ink_stroke(r, tick, 2,
                       (BrushProfile){.w_max = 3.2f,
                                      .w_min_ratio = 0.25f,
                                      .entry = 0.15f,
                                      .exit = 0.35f,
                                      .dry = 0.1f,
                                      .seed = seed + 30u},
                       (InkTone){.color = PAL_SHU, .alpha = 0.94f}, 1.0f);
        } else if (gim->kind == GIMMICK_KAMAITACHI_VORTEX) {
            const float rad = tr->radius > 10.0f ? tr->radius : 72.0f;
            const float spin = r->vfx_time * -6.0f;
            ink_arc_stroke(r, (Vector2){tr->x, tr->y}, rad * 0.72f, spin, spin + 4.2f,
                           (BrushProfile){.w_max = 4.2f,
                                          .w_min_ratio = 0.25f,
                                          .entry = 0.15f,
                                          .exit = 0.45f,
                                          .dry = 0.45f,
                                          .seed = seed + 40u},
                           (InkTone){.color = PAL_SUMI_HEAVY, .alpha = 0.65f});
        }
    }
}

void render_draw_entities(RenderState *r, const GameState *g, int act_index)
{
    if (r == NULL || g == NULL) {
        return;
    }
    const uint64_t boil = g->ticks / 6u;
    const uint32_t hw = g->ecs.high_water;

    render_set_blend(r, BLEND_ALPHA);
    draw_gimmick_effects(r, g, boil);

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
                draw_projectile(r, g, tr, game_velocity(g, id), pr, pseed, act_index);
            }
        }
    }

    /* Layer 6: Shrines, enemies, bosses */
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
            draw_enemy_lantern(r, tr, en, seed, g->ticks, act_index, false, 1.0f);
            break;
        case ENEMY_LANTERN_STORM:
            draw_enemy_lantern(r, tr, en, seed, g->ticks, act_index, true, 1.0f);
            break;
        case ENEMY_UMBRELLA:
            draw_enemy_umbrella(r, tr, en, seed, act_index, 1.0f);
            break;
        case ENEMY_KAMAITACHI:
            draw_enemy_kamaitachi(r, tr, game_velocity(g, id), en, seed, act_index, 1.0f);
            break;
        case ENEMY_STONE_ONI:
            draw_enemy_stone_oni(r, tr, en, seed, act_index, 1.0f);
            break;
        case ENEMY_INK_LEECH:
            draw_enemy_ink_leech(r, tr, en, seed, act_index, 1.0f);
            break;
        case ENEMY_BOSS_TENGU:
            draw_boss_tengu(r, tr, game_boss(g, id), primary_shrine, seed, act_index, 1.0f);
            break;
        case ENEMY_BOSS_NUE:
            draw_boss_nue(r, tr, en, game_boss(g, id), seed, act_index, 1.0f);
            break;
        case ENEMY_BOSS_RYU:
            draw_boss_ryu(r, g, id, tr, game_boss(g, id), primary_shrine, seed, act_index, 1.0f);
            break;
        case ENEMY_OFUDA_FOX:
            draw_enemy_ofuda_fox(r, tr, en, seed, g->ticks, act_index, 1.0f);
            break;
        case ENEMY_BOSS_RYU_ORB:
            /* Drawn in a second pass after enemies so orbs always sit crisply atop Ryu's body */
            continue;
        default:
            break;
        }

        draw_status_overlays(r, tr, game_status(g, id), game_health(g, id), seed);
    }

    /* Pass 6b: Independent Dragon Orbs rendered over Ryu body */
    for (uint32_t i = 0; i < hw; ++i) {
        if (!g->ecs.alive[i]) {
            continue;
        }
        const EntityId id = {.index = i, .gen = g->ecs.gen[i]};
        const TransformComp *tr = game_transform(g, id);
        const EnemyComp *en = game_enemy(g, id);
        if (tr == NULL || en == NULL || en->def_id != ENEMY_BOSS_RYU_ORB || tr->dissolving) {
            continue;
        }
        const uint32_t seed = entity_seed(id, 3u, boil);
        draw_boss_ryu_orb(r, tr, game_boss(g, id), seed, g->ticks, act_index, 1.0f);
        draw_status_overlays(r, tr, game_status(g, id), game_health(g, id), seed);
    }
}
