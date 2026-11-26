#include "draw_entities_internal.h"
#include "ink_prims.h"
#include "render_internal.h"
#include "sprites.h"

#include "core.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

static const float k_spr_pi = 3.14159265359f;
static const float k_spr_two_pi = 6.28318530718f;
static const float k_spr_deg = 0.01745329252f;
static const float k_spr_halo_alpha = 0.85f;

typedef SpritePlan SprPlan;

static float finite_or(float v, float fallback) { return isfinite(v) ? v : fallback; }

static float clamp01_finite(float v) { return isfinite(v) ? ink_clampf(v, 0.0f, 1.0f) : 0.0f; }

static float game_time_s(const GameState *g) { return (float)((double)g->ticks / INK_TICK_HZ); }

static float stable_phase(EntityId id)
{
    return (float)(render_entity_seed(id, 5u, 0u) & 0xffffu) * (k_spr_two_pi / 65536.0f);
}

static Color white_a(float a)
{
    return (Color){255, 255, 255, (unsigned char)(clamp01_finite(a) * 255.0f + 0.5f)};
}

static Color halo_tint(int act_index, float a)
{
    Color c = ink_knockout_color(act_index);
    c.a = (unsigned char)(clamp01_finite(a) * 255.0f + 0.5f);
    return c;
}

/* 乗算合成は dst·(src.rgb + 1 − src.a) なので、紙を pal へ a だけ寄せるには乗算済みの色を渡す */
static Color stain_color(PaletteId pal, float a)
{
    const Color c = pal_color(pal);
    const float k = clamp01_finite(a);
    return (Color){(unsigned char)((float)c.r * k + 0.5f), (unsigned char)((float)c.g * k + 0.5f),
                   (unsigned char)((float)c.b * k + 0.5f), (unsigned char)(k * 255.0f + 0.5f)};
}

static Vector2 plan_point(const SprPlan *p, SpriteUv uv)
{
    Vector2 out = {p->xf.x, p->xf.y};
    sprite_xform_point(p->def, &p->xf, uv, &out.x, &out.y);
    return out;
}

static Vector2 plan_anchor(const SprPlan *p, SpriteAnchorId a, Vector2 fallback)
{
    SpriteUv uv;
    return sprite_anchor(p->def, a, &uv) ? plan_point(p, uv) : fallback;
}

static void apply_boil(SpriteXform *xf, uint32_t seed)
{
    const SpriteBoil b = sprite_boil(seed, xf->size);
    xf->sx *= b.scale;
    xf->sy *= b.scale;
    xf->rot += b.rot;
    xf->dx += b.dx;
    xf->dy += b.dy;
}

/* 足元まわりの回転を、キャンバス中心まわりの回転に置き換える */
static void spin_about_centre(SpriteXform *xf, const SpriteDef *d, float spin)
{
    const float my = xf->flip_v ? -1.0f : 1.0f;
    const float lx = (0.5f - d->foot.u) * xf->size * xf->sx;
    const float ly = (0.5f - d->foot.v) * xf->size * xf->sy * my;
    const float c0 = cosf(xf->rot);
    const float s0 = sinf(xf->rot);
    const float c1 = cosf(xf->rot + spin);
    const float s1 = sinf(xf->rot + spin);
    xf->dx += (lx * c0 - ly * s0) - (lx * c1 - ly * s1);
    xf->dy += (lx * s0 + ly * c0) - (lx * s1 + ly * c1);
    xf->rot += spin;
}

static float compute_cos_facing(const SpriteDef *d, const TransformComp *tr, const EnemyComp *en,
                                const VelocityComp *vel, const ShrineState *shrine)
{
    if (d->orient == SPR_ORIENT_FACE_SHRINE && shrine != NULL) {
        return shrine->x >= tr->x ? 1.0f : -1.0f;
    }
    if (en->def_id == ENEMY_BOSS_NUE) {
        const float vx = vel != NULL ? finite_or(vel->vx, 0.0f) : 0.0f;
        const float vy = vel != NULL ? finite_or(vel->vy, 0.0f) : 0.0f;
        const float sp = hypotf(vx, vy);
        return sp > 8.0f ? vx / sp : 0.0f;
    }
    return cosf(finite_or(tr->facing, 0.0f));
}

static int8_t update_face(RenderState *r, EntityId id, float cos_facing, uint64_t ticks,
                          uint32_t *elapsed)
{
    const uint32_t now = (uint32_t)ticks;
    if (id.index >= ECS_MAX_ENTITIES) {
        if (elapsed != NULL) {
            *elapsed = (uint32_t)SPRITE_TURN_TICKS;
        }
        return cos_facing < 0.0f ? (int8_t)-1 : (int8_t)1;
    }
    const uint32_t i = id.index;
    if (r->sprite_face_gen[i] != id.gen) {
        r->sprite_face_gen[i] = id.gen;
        r->sprite_face[i] = 0;
    }
    const int8_t prev = r->sprite_face[i];
    const int8_t next = sprite_face_update(prev, finite_or(cos_facing, 0.0f));
    if (prev == 0) {
        r->sprite_turn_tick[i] = now - (uint32_t)SPRITE_TURN_TICKS;
    } else if (next != prev) {
        r->sprite_turn_tick[i] = now;
    }
    r->sprite_face[i] = next;
    if (elapsed != NULL) {
        *elapsed = now - r->sprite_turn_tick[i];
    }
    return next;
}

/* 描画段では tick 側で更新した向きを読むだけにし、未登録スロット（見本帳など）だけ初回初期化する */
static int8_t read_face(RenderState *r, EntityId id, float cos_facing, uint64_t ticks,
                        uint32_t *elapsed)
{
    if (id.index >= ECS_MAX_ENTITIES || r->sprite_face_gen[id.index] != id.gen ||
        r->sprite_face[id.index] == 0) {
        return update_face(r, id, cos_facing, ticks, elapsed);
    }
    if (elapsed != NULL) {
        *elapsed = (uint32_t)ticks - r->sprite_turn_tick[id.index];
    }
    return r->sprite_face[id.index];
}

static bool has_ryu_body_trail(const GameState *g, EntityId id)
{
    return g->ryu_body.head_id.index == id.index && g->ryu_body.head_id.gen == id.gen &&
           g->ryu_body.trail_count >= 2;
}

/* 胴の軌跡（本体 128 点・描画側 24 点・仮の曲線の順に試す）を 64 点へ置き直す */
static int32_t ryu_raw_spine(const RenderState *r, const GameState *g, EntityId id,
                             const TransformComp *tr, GameVec2 out[RENDER_RYU_SPINE_PTS])
{
    GameVec2 raw[GAME_RYU_TRAIL_CAP + 1];
    int32_t n = 0;
    if (has_ryu_body_trail(g, id)) {
        const RyuBodyState *body = &g->ryu_body;
        const int32_t cnt =
            body->trail_count > GAME_RYU_TRAIL_CAP ? GAME_RYU_TRAIL_CAP : body->trail_count;
        int32_t head = body->trail_head % GAME_RYU_TRAIL_CAP;
        head += head < 0 ? GAME_RYU_TRAIL_CAP : 0;
        raw[n++] = (GameVec2){tr->x, tr->y};
        /* 最新の記録点（k=0）が現在の頭位置と離れているときも落とさずに残す */
        const GameVec2 newest = body->trail[head];
        if (hypotf(newest.x - tr->x, newest.y - tr->y) > 1e-3f) {
            raw[n++] = newest;
        }
        for (int32_t k = 1; k < cnt; ++k) {
            raw[n++] = body->trail[(head - k + GAME_RYU_TRAIL_CAP) % GAME_RYU_TRAIL_CAP];
        }
    }
    if (n < 4 && r->ryu_trail_id.index == id.index && r->ryu_trail_id.gen == id.gen &&
        r->ryu_trail_n >= 4) {
        n = r->ryu_trail_n < RENDER_RYU_TRAIL_LEN ? r->ryu_trail_n : RENDER_RYU_TRAIL_LEN;
        for (int32_t i = 0; i < n; ++i) {
            raw[i] = (GameVec2){r->ryu_trail[i].x, r->ryu_trail[i].y};
        }
        raw[0] = (GameVec2){tr->x, tr->y};
    }
    if (n < 4) {
        n = 18;
        for (int32_t i = 0; i < n; ++i) {
            const float u = (float)i / (float)(n - 1);
            raw[i] = (GameVec2){tr->x + u * 260.0f, tr->y + sinf(u * 5.2f) * 42.0f + u * 18.0f};
        }
    }
    return sprite_resample_polyline(raw, n, out, RENDER_RYU_SPINE_PTS);
}

static float ryu_head_angle_from_raw(const GameVec2 *spine, int32_t n, float fallback_facing)
{
    if (spine != NULL && n >= 3) {
        const float fx = spine[0].x - spine[2].x;
        const float fy = spine[0].y - spine[2].y;
        if (fx * fx + fy * fy > 1e-6f) {
            return atan2f(fy, fx);
        }
    }
    return finite_or(fallback_facing, 0.0f);
}

static float orb_pulse(const BossComp *boss, uint64_t ticks)
{
    const int32_t part = boss != NULL ? boss->part_id : 0;
    const float tele = boss != NULL ? clamp01_finite(boss->telegraph) : 0.0f;
    return 1.0f + 0.05f * sinf((float)ticks * 0.16f + (float)part * 2.1f) + 0.22f * tele;
}

static void apply_orientation(RenderState *r, const GameState *g, EntityId id,
                              const TransformComp *tr, const EnemyComp *en,
                              const ShrineState *shrine, const GameVec2 *ryu_raw, int32_t ryu_raw_n,
                              SprPlan *p)
{
    const VelocityComp *vel = game_velocity(g, id);
    const float vx = vel != NULL ? finite_or(vel->vx, 0.0f) : 0.0f;
    const float vy = vel != NULL ? finite_or(vel->vy, 0.0f) : 0.0f;
    switch (p->def->orient) {
    case SPR_ORIENT_MIRROR_X:
    case SPR_ORIENT_FACE_SHRINE: {
        const float cos_f = compute_cos_facing(p->def, tr, en, vel, shrine);
        uint32_t elapsed = 0u;
        const int8_t face = read_face(r, id, cos_f, g->ticks, &elapsed);
        p->face = face < 0 ? -1.0f : 1.0f;
        p->xf.sx *= sprite_turn_sx(face, elapsed);
        p->xf.rot += 6.0f * k_spr_deg * ink_clampf(vx / 160.0f, -1.0f, 1.0f);
        break;
    }
    case SPR_ORIENT_ROTATE_MIRROR: {
        float theta = finite_or(tr->facing, 0.0f);
        if (en->def_id == ENEMY_BOSS_RYU) {
            theta = ryu_head_angle_from_raw(ryu_raw, ryu_raw_n, tr->facing);
        } else if (hypotf(vx, vy) > 1.0f) {
            theta = atan2f(vy, vx);
        }
        p->xf.rot += theta;
        p->xf.flip_v = cosf(theta) < 0.0f;
        p->face = p->xf.flip_v ? -1.0f : 1.0f;
        break;
    }
    case SPR_ORIENT_NONE:
    default:
        break;
    }
}

static void apply_character_motion(const GameState *g, EntityId id, const EnemyComp *en,
                                   float phase, SprPlan *p)
{
    const float t = game_time_s(g);
    const StatusComp *st = game_status(g, id);
    switch (en->def_id) {
    case ENEMY_LANTERN_FIRE:
    case ENEMY_LANTERN_STORM:
        p->xf.dy += 2.0f * sinf(k_spr_two_pi * t / 1.8f + phase);
        break;
    case ENEMY_UMBRELLA: {
        const float h = sprite_umbrella_hop(en->phase);
        p->xf.dy -= 12.0f * powf(h, 0.8f);
        if (st != NULL && isfinite(st->knockback_t) && st->knockback_t > 0.0f) {
            /* 9 rad/s のまま減速すると 4 rad で止まって跳ね戻るため、1 回転で止まる初速にする */
            const float k = 1.0f - clamp01_finite(st->knockback_t / 0.9f);
            const float spin = p->face * k_spr_two_pi * (1.0f - (1.0f - k) * (1.0f - k));
            p->xf.sx *= 1.0f + 0.06f * sinf(k_spr_two_pi * 6.0f * t);
            spin_about_centre(&p->xf, p->def, spin);
        }
        break;
    }
    case ENEMY_STONE_ONI:
        p->xf.dy += 1.5f * sinf(k_spr_two_pi * 1.2f * t + phase);
        if (en->ai_state == 1) {
            p->xf.rot -= 6.0f * k_spr_deg * p->face;
        }
        break;
    case ENEMY_OFUDA_FOX:
        if (en->ai_state != 0) {
            p->xf.rot += 5.0f * k_spr_deg * (en->ai_state > 0 ? 1.0f : -1.0f);
        }
        break;
    case ENEMY_BOSS_TENGU: {
        const float flap = sinf(k_spr_two_pi * 0.8f * t + phase);
        p->xf.sx *= 1.0f + 0.035f * flap;
        p->xf.dy -= 2.0f * (0.5f + 0.5f * flap);
        break;
    }
    case ENEMY_BOSS_NUE:
        if (en->ai_state == 2) {
            p->xf.sx *= 1.12f;
        }
        break;
    default:
        break;
    }
}

static bool plan_enemy_with_raw(RenderState *r, const GameState *g, EntityId id,
                                const ShrineState *shrine, const GameVec2 *ryu_raw,
                                int32_t ryu_raw_n, SprPlan *p)
{
    const TransformComp *tr = game_transform(g, id);
    const EnemyComp *en = game_enemy(g, id);
    if (tr == NULL || en == NULL) {
        return false;
    }
    const BossComp *boss = game_boss(g, id);
    const SpriteStateIn in = {.def_id = en->def_id,
                              .windup = finite_or(en->windup, 0.0f),
                              .ai_state = en->ai_state,
                              .phase_milli = en->phase,
                              .telegraph = boss != NULL ? finite_or(boss->telegraph, 0.0f) : 0.0f,
                              .phase_idx = boss != NULL ? boss->phase_idx : 0,
                              .part_id = boss != NULL ? boss->part_id : 0};
    const SpriteId spr = render_sprite_resolve(r, sprite_select(&in));
    if (spr == SPR_NONE) {
        return false;
    }
    const SpriteDef *d = sprite_def(spr);
    float radius = (isfinite(tr->radius) && tr->radius > 1.0f) ? tr->radius : d->ref_radius;
    if (en->def_id == ENEMY_INK_LEECH) {
        const float absorbed =
            isfinite(en->absorbed_ink) ? ink_clampf(en->absorbed_ink, 0.0f, 3.0f) : 0.0f;
        radius *= 1.0f + 0.18f * absorbed;
    }
    float size = sprite_display_size(d, radius);
    if (en->def_id == ENEMY_BOSS_RYU_ORB) {
        size *= orb_pulse(boss, g->ticks);
    }

    const float phase = stable_phase(id);
    *p = (SprPlan){.id = spr,
                   .def = d,
                   .xf = {.x = finite_or(tr->x, 0.0f),
                          .y = finite_or(tr->y, 0.0f),
                          .size = size,
                          .sx = 1.0f,
                          .sy = 1.0f},
                   .body_a = 1.0f,
                   .halo_a = k_spr_halo_alpha,
                   .face = 1.0f};
    sprite_breath(d->breath_amp, d->breath_period_s, game_time_s(g), phase, d->jelly, &p->xf.sx,
                  &p->xf.sy);
    apply_boil(&p->xf, render_entity_seed(id, 6u, g->ticks / 6u));
    apply_orientation(r, g, id, tr, en, shrine, ryu_raw, ryu_raw_n, p);
    apply_character_motion(g, id, en, phase, p);

    const HealthComp *hp = game_health(g, id);
    if (hp != NULL && isfinite(hp->flash_t) && hp->flash_t > 0.0f) {
        /* 白く光らせず、打たれた濡れ墨が持ち上がって紙が透けるように見せる */
        const float f = clamp01_finite(hp->flash_t / 0.45f);
        p->body_a *= 1.0f - 0.45f * f * f;
        p->xf.sx *= 1.0f - 0.06f * f;
        p->xf.sy *= 1.0f - 0.06f * f;
    }
    if (tr->dissolving) {
        const float dt_val = finite_or(tr->dissolve_t, 0.0f);
        const float u = clamp01_finite(dt_val / 0.6f);
        p->dissolving = true;
        p->dissolve = u;
        p->halo_a = k_spr_halo_alpha * powf(1.0f - u, 0.7f);
        p->xf.size *= 1.0f + 0.10f * u;
        p->xf.dy += 4.0f * u;
        if (en->def_id == ENEMY_BOSS_RYU) {
            p->body_a *= 1.0f - clamp01_finite(dt_val / 0.3f);
        } else {
            p->body_a *= powf(1.0f - u, 1.6f);
        }
    }
    return true;
}

static bool plan_shrine(const RenderState *r, const GameState *g, int32_t s, SprPlan *p)
{
    if (s < 0 || s >= g->shrine_count || s >= GAME_MAX_SHRINES) {
        return false;
    }
    const SpriteId spr = render_sprite_resolve(r, SPR_SHRINE);
    if (spr == SPR_NONE) {
        return false;
    }
    const ShrineState *sh = &g->shrines[s];
    const SpriteDef *d = sprite_def(spr);
    *p = (SprPlan){.id = spr,
                   .def = d,
                   .xf = {.x = sh->x,
                          .y = sh->y,
                          .size = sprite_display_size(d, sh->hit_radius_contact),
                          .sx = 1.0f,
                          .sy = 1.0f},
                   .body_a = 1.0f,
                   .halo_a = k_spr_halo_alpha,
                   .face = 1.0f};
    const EntityId shrine_key = {.index = UINT32_C(0xfffff000) + (uint32_t)s, .gen = 0u};
    apply_boil(&p->xf, render_entity_seed(shrine_key, 6u, g->ticks / 6u));
    if (r->shrine_flicker_t > 0.0f) {
        const float k = 1.0f - ink_clampf((0.45f - r->shrine_flicker_t) / 0.2f, 0.0f, 1.0f);
        p->xf.dx += 2.0f * k * sinf((float)g->ticks * 1.9f);
        p->xf.dy += 1.0f * k * cosf((float)g->ticks * 2.7f);
    }
    if (sh->invuln_t > 0.0f) {
        /* 3 ティックごとの明滅をやめ、縁取りの濃さだけをゆっくり脈打たせる */
        p->halo_a = 0.85f + 0.15f * sinf(k_spr_two_pi * 2.5f * game_time_s(g));
    }
    return true;
}

static void draw_body(RenderState *r, const SprPlan *p)
{
    render_sprite_draw(r, p->id, &p->xf, white_a(p->body_a));
}

static void draw_halo(RenderState *r, const SprPlan *p, int act_index)
{
    render_sprite_draw_halo(r, p->id, &p->xf, halo_tint(act_index, p->halo_a));
}

static void draw_elite_rims(RenderState *r, const EnemyComp *en, const SprPlan *p, int act_index)
{
    PaletteId pals[4];
    const int count = render_elite_palettes(en, pals);
    if (count == 0) {
        return;
    }
    /* 外側の縁から重ね、最後に紙抜きを描き直して縁と体の間に紙を残す */
    for (int i = count - 1; i >= 0; --i) {
        SpriteXform rim = p->xf;
        rim.size += 2.0f * (4.0f + 2.0f * (float)i);
        render_sprite_draw_halo(r, p->id, &rim,
                                pal_color_alpha(pals[i], 0.94f * (p->halo_a / k_spr_halo_alpha)));
    }
    draw_halo(r, p, act_index);
}

static float entity_radius(const TransformComp *tr, float fallback)
{
    return tr->radius > 6.0f ? tr->radius : fallback;
}

static void draw_archer(RenderState *r, const TransformComp *tr, const EnemyComp *en,
                        const SprPlan *p, uint32_t seed)
{
    draw_body(r, p);
    const float rad = entity_radius(tr, 16.0f);
    const float pull = clamp01_finite(en->windup);
    const Vector2 grip = plan_anchor(p, SPR_ANCHOR_BOW_GRIP, (Vector2){tr->x, tr->y});
    const float ca = cosf(tr->facing);
    const float sa = sinf(tr->facing);
    const float bow_r = rad * 1.15f;
    const float back = bow_r * (0.60f - 0.15f * pull);
    const Vector2 bow[4] = {
        {grip.x - ca * back - sa * bow_r, grip.y - sa * back + ca * bow_r},
        {grip.x - sa * bow_r * 0.35f, grip.y + ca * bow_r * 0.35f},
        {grip.x + sa * bow_r * 0.35f, grip.y - ca * bow_r * 0.35f},
        {grip.x - ca * back + sa * bow_r, grip.y - sa * back - ca * bow_r},
    };
    ink_stroke(r, bow, 4,
               (BrushProfile){.w_max = 3.2f,
                              .w_min_ratio = 0.25f,
                              .entry = 0.15f,
                              .exit = 0.30f,
                              .dry = 0.2f,
                              .seed = seed + 4u},
               (InkTone){.color = PAL_SUMI_SCORCH, .alpha = 0.95f * p->body_a}, 1.0f);
    if (pull > 0.05f) {
        const Vector2 fallback_nock = {grip.x - ca * rad * (0.45f + 0.6f * pull),
                                       grip.y - sa * rad * (0.45f + 0.6f * pull)};
        const Vector2 arr[2] = {
            plan_anchor(p, SPR_ANCHOR_NOCK, fallback_nock),
            {grip.x + ca * rad * 0.7f, grip.y + sa * rad * 0.7f},
        };
        ink_stroke(r, arr, 2,
                   (BrushProfile){.w_max = 2.6f,
                                  .w_min_ratio = 0.3f,
                                  .entry = 0.1f,
                                  .exit = 0.35f,
                                  .dry = 0.0f,
                                  .seed = seed + 5u},
                   (InkTone){.color = PAL_SHU, .alpha = 0.85f * p->body_a}, 1.0f);
    }
}

static float lantern_pulse(const EnemyComp *en, uint64_t ticks, float phase)
{
    const float pull = clamp01_finite(en->windup);
    return 0.85f + 0.15f * sinf((float)ticks * 0.14f + phase) + 0.28f * pull;
}

static void draw_lantern(RenderState *r, const TransformComp *tr, const EnemyComp *en,
                         const SprPlan *p, uint32_t seed)
{
    draw_body(r, p);
    const bool storm = en->def_id == ENEMY_LANTERN_STORM;
    const float rad = entity_radius(tr, 17.0f);
    const float pull = clamp01_finite(en->windup);
    const Vector2 m = plan_anchor(p, SPR_ANCHOR_MOUTH, (Vector2){tr->x, tr->y + rad * 0.35f});
    if (!storm) {
        const Vector2 tongue[3] = {
            {m.x - 2.0f, m.y},
            {m.x + 6.0f, m.y + rad * 0.43f},
            {m.x + 12.0f + pull * 5.0f, m.y + rad * 0.83f},
        };
        ink_stroke(r, tongue, 3,
                   (BrushProfile){.w_max = 3.8f,
                                  .w_min_ratio = 0.22f,
                                  .entry = 0.1f,
                                  .exit = 0.45f,
                                  .dry = 0.1f,
                                  .seed = seed + 15u},
                   (InkTone){.color = PAL_SHU, .alpha = 0.92f * p->body_a}, 1.0f);
    }
}

static void draw_umbrella(RenderState *r, const TransformComp *tr, const EnemyComp *en,
                          const SprPlan *p, uint32_t seed)
{
    const float h = sprite_umbrella_hop(en->phase);
    const float rad = entity_radius(tr, 18.0f);
    const Vector2 ground = {tr->x + p->face * (p->def->foot.u - 0.5f) * p->xf.size,
                            tr->y + (p->def->foot.v - 0.5f) * p->xf.size};
    const float shrink = 1.0f - 0.5f * h;
    ink_wash(r, ground, rad * 0.9f * shrink, rad * 0.28f * shrink, 0.0f,
             (InkTone){.color = PAL_SUMI_CLEAR, .alpha = 0.18f * (1.0f - 0.6f * h) * p->body_a}, 1,
             seed + 70u);
    draw_body(r, p);
}

static void draw_kamaitachi(RenderState *r, const GameState *g, EntityId id,
                            const TransformComp *tr, const SprPlan *p, uint32_t seed)
{
    const VelocityComp *vel = game_velocity(g, id);
    const float vx = vel != NULL ? vel->vx : cosf(tr->facing) * 120.0f;
    const float vy = vel != NULL ? vel->vy : sinf(tr->facing) * 120.0f;
    const float sp = sqrtf(vx * vx + vy * vy);
    const Vector2 dir = sp > 1.0f ? (Vector2){vx / sp, vy / sp} : (Vector2){1.0f, 0.0f};
    const Vector2 side = {-dir.y, dir.x};
    const float rad = entity_radius(tr, 16.0f);
    const Vector2 root = plan_anchor(p, SPR_ANCHOR_TRAIL_ROOT, (Vector2){tr->x, tr->y});
    const Vector2 tail[4] = {
        root,
        {root.x - dir.x * rad * 0.55f + side.x * 7.0f,
         root.y - dir.y * rad * 0.55f + side.y * 7.0f},
        {root.x - dir.x * rad * 1.10f - side.x * 5.0f,
         root.y - dir.y * rad * 1.10f - side.y * 5.0f},
        {root.x - dir.x * rad * 1.55f, root.y - dir.y * rad * 1.55f},
    };
    ink_stroke(r, tail, 4,
               (BrushProfile){.w_max = 5.0f,
                              .w_min_ratio = 0.18f,
                              .entry = 0.08f,
                              .exit = 0.65f,
                              .dry = 0.55f,
                              .seed = seed + 1u},
               (InkTone){.color = PAL_SUMI_CLEAR, .alpha = 0.58f * p->body_a}, 1.0f);
    const float ghost_a[2] = {0.25f, 0.12f};
    for (int k = 2; k >= 1; --k) {
        SpriteXform ghost = p->xf;
        ghost.x -= vx * 0.1f * (float)k;
        ghost.y -= vy * 0.1f * (float)k;
        render_sprite_draw(r, p->id, &ghost, white_a(ghost_a[k - 1] * p->body_a));
    }
    draw_body(r, p);
}

static void draw_ink_leech(RenderState *r, const TransformComp *tr, const EnemyComp *en,
                           EntityId id, const SprPlan *p, uint32_t seed, uint64_t ticks)
{
    const float absorbed =
        isfinite(en->absorbed_ink) ? ink_clampf(en->absorbed_ink, 0.0f, 3.0f) : 0.0f;
    const float rad = entity_radius(tr, 19.0f) * (1.0f + 0.18f * absorbed);
    ink_wash(r, (Vector2){tr->x - rad * 0.3f, tr->y + rad * 0.2f}, rad * (0.9f + 0.2f * absorbed),
             rad * (0.55f + 0.12f * absorbed), 0.3f,
             (InkTone){.color = PAL_SUMI_HEAVY, .alpha = (0.10f + 0.08f * absorbed) * p->body_a}, 2,
             seed + 60u);
    ink_wash(r, (Vector2){tr->x + rad * 0.35f, tr->y + rad * 0.1f}, rad * (0.8f + 0.25f * absorbed),
             rad * (0.5f + 0.1f * absorbed), -0.4f,
             (InkTone){.color = PAL_SUMI_DARK, .alpha = (0.08f + 0.07f * absorbed) * p->body_a}, 2,
             seed + 61u);
    draw_body(r, p);

    int specks = (int)floorf(absorbed * 4.0f);
    specks = specks > 12 ? 12 : specks;
    const Vector2 core = plan_anchor(p, SPR_ANCHOR_CORE, (Vector2){tr->x, tr->y});
    const float core_r = p->def->extent.u * p->xf.size;
    for (int k = 0; k < specks; ++k) {
        const uint32_t h = render_entity_seed(id, 8u, (uint64_t)k);
        const float ang = (float)(h & 0xffffu) * (k_spr_two_pi / 65536.0f);
        const float dist = sqrtf((float)((h >> 16) & 0xffu) / 255.0f) * core_r;
        const float twinkle = 0.55f + 0.45f * sinf((float)ticks * 0.2f + (float)k * 1.7f);
        const Vector2 at = {core.x + cosf(ang) * dist, core.y + sinf(ang) * dist * 0.8f};
        ink_teardrop(r, at, (Vector2){0.7f, -0.7f}, 3.0f, 2.2f,
                     pal_color_alpha(PAL_KIN, twinkle * p->body_a));
        ink_teardrop(r, at, (Vector2){0.7f, -0.7f}, 1.4f, 1.0f,
                     pal_color_alpha(PAL_KIN_HOT, twinkle * p->body_a));
    }
}

/* 尻尾は絵の外へ伸びるので、キャンバス座標で組んでから絵と同じ変換をかける */
static int fox_tail(const SprPlan *p, uint64_t ticks, float phase, Vector2 out[4])
{
    SpriteUv root;
    if (!sprite_anchor(p->def, SPR_ANCHOR_TAIL_ROOT, &root) || p->xf.size <= 1.0f) {
        return 0;
    }
    const float sway = sinf((float)ticks * 0.12f + phase) * 6.5f / p->xf.size;
    const SpriteUv uv[4] = {
        root,
        {root.u - 0.126f, root.v + 0.031f + sway * 0.35f},
        {root.u - 0.229f, root.v - 0.059f + sway * 0.75f},
        {root.u - 0.288f, root.v - 0.167f + sway},
    };
    for (int k = 0; k < 4; ++k) {
        out[k] = plan_point(p, uv[k]);
    }
    return 4;
}

static BrushProfile fox_tail_profile(uint32_t seed)
{
    return (BrushProfile){.w_max = 8.2f,
                          .w_min_ratio = 0.16f,
                          .entry = 0.14f,
                          .exit = 0.58f,
                          .dry = 0.48f,
                          .seed = seed + 2u};
}

static int nue_tail(const SprPlan *p, uint64_t ticks, float phase, Vector2 out[4])
{
    SpriteUv root;
    if (!sprite_anchor(p->def, SPR_ANCHOR_TAIL_ROOT, &root) || p->xf.size <= 1.0f) {
        return 0;
    }
    const float sway = sinf((float)ticks * 0.09f + phase) * 4.0f / p->xf.size;
    const SpriteUv uv[4] = {
        root,
        {root.u - 0.070f, root.v - 0.030f + sway * 0.4f},
        {root.u - 0.105f, root.v - 0.100f + sway * 0.8f},
        {root.u - 0.066f, root.v - 0.160f + sway},
    };
    for (int k = 0; k < 4; ++k) {
        out[k] = plan_point(p, uv[k]);
    }
    return 4;
}

static BrushProfile nue_tail_profile(uint32_t seed)
{
    return (BrushProfile){.w_max = 4.6f,
                          .w_min_ratio = 0.22f,
                          .entry = 0.10f,
                          .exit = 0.55f,
                          .dry = 0.25f,
                          .seed = seed + 20u};
}

static void draw_ofuda_fox(RenderState *r, const TransformComp *tr, const EnemyComp *en,
                           EntityId id, const SprPlan *p, uint32_t seed, uint64_t ticks)
{
    Vector2 tail[4];
    if (fox_tail(p, ticks, stable_phase(id), tail) == 4) {
        ink_stroke(r, tail, 4, fox_tail_profile(seed),
                   (InkTone){.color = PAL_SUMI_DARK, .alpha = 0.94f * p->body_a}, 1.0f);
    }
    draw_body(r, p);

    const float pull = clamp01_finite(en->windup);
    if (pull <= 0.05f) {
        return;
    }
    const float rad = entity_radius(tr, 17.0f);
    const float orbit = (float)ticks * 0.09f;
    const float orad = rad * (1.28f + 0.18f * pull);
    for (int k = 0; k < 2; ++k) {
        const float a = orbit + (float)k * k_spr_pi;
        const Vector2 fc = {tr->x + cosf(a) * orad, tr->y + sinf(a) * orad};
        const Vector2 slip[4] = {
            {fc.x - 4.0f, fc.y - 6.5f},
            {fc.x + 4.5f, fc.y - 5.8f},
            {fc.x + 3.8f, fc.y + 6.5f},
            {fc.x - 4.4f, fc.y + 5.8f},
        };
        const float a_slip = (0.55f + 0.40f * pull) * p->body_a;
        ink_fill_poly(r, slip, 4, pal_color_alpha(PAL_GOFUN, a_slip));
        ink_outline_tone(
            r, slip, 4, true, 1.8f, (Vector2){0.7071f, 0.7071f},
            (InkTone){.color = PAL_SUMI_DARK, .alpha = (0.50f + 0.45f * pull) * p->body_a},
            seed + 14u + (uint32_t)k);
        const Vector2 seal[4] = {
            {fc.x - 1.6f, fc.y + 1.6f},
            {fc.x + 1.7f, fc.y + 1.5f},
            {fc.x + 1.5f, fc.y + 4.6f},
            {fc.x - 1.5f, fc.y + 4.5f},
        };
        ink_fill_poly(r, seal, 4, pal_color_alpha(PAL_SHU, (0.6f + 0.35f * pull) * p->body_a));
    }
}

static void draw_tengu(RenderState *r, const TransformComp *tr, const BossComp *boss,
                       const ShrineState *shrine, const SprPlan *p, uint32_t seed)
{
    draw_body(r, p);
    render_draw_tengu_telegraph(r, tr, boss, shrine, seed);
    const float tele = boss != NULL ? ink_clampf(boss->telegraph, 0.0f, 1.0f) : 0.0f;
    if (tele <= 0.01f) {
        return;
    }
    const bool has_dir = boss->gale_dx != 0.0f || boss->gale_dy != 0.0f;
    const float base = atan2f(has_dir ? boss->gale_dy : 1.0f, has_dir ? boss->gale_dx : 0.0f);
    const Vector2 tip = plan_anchor(p, SPR_ANCHOR_FAN_TIP, (Vector2){tr->x, tr->y});
    for (int k = -1; k <= 1; ++k) {
        const float a = base + (float)k * 0.3f;
        const float len = 26.0f + 30.0f * tele;
        const Vector2 n = {-sinf(a), cosf(a)};
        const Vector2 pts[3] = {
            tip,
            {tip.x + cosf(a) * len * 0.5f + n.x * 4.0f * (float)k,
             tip.y + sinf(a) * len * 0.5f + n.y * 4.0f * (float)k},
            {tip.x + cosf(a) * len, tip.y + sinf(a) * len},
        };
        ink_stroke(r, pts, 3,
                   (BrushProfile){.w_max = 3.0f,
                                  .w_min_ratio = 0.2f,
                                  .entry = 0.1f,
                                  .exit = 0.55f,
                                  .dry = 0.5f,
                                  .seed = seed + 100u + (uint32_t)(k + 1)},
                   (InkTone){.color = PAL_SUMI_LIGHT, .alpha = 0.7f * tele * p->body_a}, 1.0f);
    }
}

static void draw_nue(RenderState *r, const TransformComp *tr, const EnemyComp *en,
                     const BossComp *boss, EntityId id, const SprPlan *p, uint32_t seed,
                     uint64_t ticks)
{
    const float rad = entity_radius(tr, 40.0f);
    const bool crouched = en->ai_state == 1;
    ink_wash(r, (Vector2){tr->x, tr->y + 4.0f}, rad * 1.32f, rad * 0.92f, -0.06f,
             (InkTone){.color = PAL_SUMI_LIGHT, .alpha = 0.45f * p->body_a}, 2, seed + 1u);
    Vector2 tail[4];
    if (nue_tail(p, ticks, stable_phase(id), tail) == 4) {
        ink_stroke(r, tail, 4, nue_tail_profile(seed),
                   (InkTone){.color = PAL_SUMI_DARK, .alpha = 0.94f * p->body_a}, 1.0f);
    }
    draw_body(r, p);

    const SpriteUv arc_uv[4] = {{0.30f, 0.40f}, {0.38f, 0.28f}, {0.47f, 0.32f}, {0.56f, 0.22f}};
    Vector2 arc[4];
    for (int k = 0; k < 4; ++k) {
        arc[k] = plan_point(p, arc_uv[k]);
    }
    ink_stroke(r, arc, 4,
               (BrushProfile){.w_max = 3.2f,
                              .w_min_ratio = 0.32f,
                              .entry = 0.15f,
                              .exit = 0.30f,
                              .dry = 0.0f,
                              .seed = seed + 30u},
               (InkTone){.color = PAL_AO, .alpha = 0.90f * p->body_a}, 1.0f);
    ink_stroke(r, arc, 4,
               (BrushProfile){.w_max = 1.5f,
                              .w_min_ratio = 0.40f,
                              .entry = 0.10f,
                              .exit = 0.15f,
                              .dry = 0.0f,
                              .seed = seed + 31u},
               (InkTone){.color = PAL_AO_HOT, .alpha = 0.95f * p->body_a}, 1.0f);

    const float eye_a = (crouched ? 0.6f : 1.0f) * p->body_a;
    const float eye_size = 4.8f * rad / 40.0f;
    const SpriteAnchorId eyes[2] = {SPR_ANCHOR_EYE_L, SPR_ANCHOR_EYE_R};
    for (int k = 0; k < 2; ++k) {
        SpriteUv uv;
        if (sprite_anchor(p->def, eyes[k], &uv)) {
            render_draw_eye_brush(r, plan_point(p, uv), eye_size, PAL_AO_HOT, eye_a);
        }
    }
    render_draw_nue_telegraph(r, tr, boss, seed);
}

static BrushProfile ryu_spine_profile(float width, uint32_t seed)
{
    return (BrushProfile){.w_max = width,
                          .w_min_ratio = 0.14f,
                          .entry = 0.06f,
                          .exit = 0.35f,
                          .dry = 0.60f,
                          .seed = seed + 1u};
}

/* 首の差し込み口から尾まで 64 点。玉の位置と首ではうねりを止め、玉の当たり判定から胴を外さない */
static int32_t ryu_prepare_spine(const GameState *g, EntityId id, const TransformComp *tr,
                                 const BossComp *boss, const SprPlan *p,
                                 const GameVec2 raw[RENDER_RYU_SPINE_PTS], int32_t raw_n,
                                 GameVec2 out[RENDER_RYU_SPINE_PTS])
{
    if (raw_n < 2) {
        return 0;
    }
    const Vector2 neck = plan_anchor(p, SPR_ANCHOR_NECK, (Vector2){tr->x, tr->y});
    const float d_neck = hypotf(neck.x - tr->x, neck.y - tr->y);
    GameVec2 socketed[RENDER_RYU_SPINE_PTS + 1];
    const int32_t sn = sprite_spine_socket(raw, raw_n, (GameVec2){neck.x, neck.y}, d_neck, socketed,
                                           RENDER_RYU_SPINE_PTS + 1);
    if (sn < 2) {
        return 0;
    }
    GameVec2 even[RENDER_RYU_SPINE_PTS];
    if (sprite_resample_polyline(socketed, sn, even, RENDER_RYU_SPINE_PTS) < 2) {
        return 0;
    }

    float orb_s[GAME_RYU_ORB_COUNT];
    int32_t orb_n = 0;
    if (has_ryu_body_trail(g, id)) {
        for (int k = 0; k < GAME_RYU_ORB_COUNT; ++k) {
            const TransformComp *otr = game_transform(g, g->ryu_body.orb_ids[k]);
            const bool broken = (g->ryu_body.orb_broken_mask & (uint8_t)(1u << (unsigned)k)) != 0u;
            if (otr != NULL && !otr->dissolving && !broken) {
                orb_s[orb_n++] = sprite_polyline_nearest_arc(even, RENDER_RYU_SPINE_PTS,
                                                             (GameVec2){otr->x, otr->y});
            }
        }
    }
    const bool awakened = boss != NULL && boss->phase_idx >= 1;
    const SpriteSpineWave wave = sprite_ryu_wave(awakened);
    sprite_spine_undulate(even, RENDER_RYU_SPINE_PTS, &wave, game_time_s(g), orb_s, orb_n, out);
    return RENDER_RYU_SPINE_PTS;
}

static int ryu_spine_to_vec2(const GameVec2 *src, int32_t n, Vector2 out[RENDER_RYU_SPINE_PTS])
{
    const int cnt = n > RENDER_RYU_SPINE_PTS ? RENDER_RYU_SPINE_PTS : (int)n;
    for (int i = 0; i < cnt; ++i) {
        out[i] = (Vector2){src[i].x, src[i].y};
    }
    return cnt;
}

static void reverse_points(Vector2 *pts, int n)
{
    for (int i = 0, j = n - 1; i < j; ++i, --j) {
        const Vector2 t = pts[i];
        pts[i] = pts[j];
        pts[j] = t;
    }
}

static void draw_ryu_spine_halo_pts(RenderState *r, const GameVec2 *src, int32_t src_n,
                                    bool awakened, float width_t, bool unravel, float dissolve,
                                    float halo_a, uint32_t seed, int act_index)
{
    if (src == NULL || src_n < 2) {
        return;
    }
    Vector2 spine[RENDER_RYU_SPINE_PTS];
    const int n = ryu_spine_to_vec2(src, src_n, spine);
    float reveal = 1.0f;
    if (unravel) {
        reverse_points(spine, n);
        reveal = 1.0f - dissolve;
    }
    ink_stroke_knockout(r, spine, n,
                        ryu_spine_profile(sprite_ryu_spine_width(awakened, width_t), seed), 4.0f,
                        act_index, halo_a / k_spr_halo_alpha, reveal);
}

static void draw_ryu_broken_overlay(RenderState *r, const Vector2 *spine, int spine_n,
                                    uint8_t broken_mask, uint32_t seed, int act_index)
{
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
        if (count < 2) {
            continue;
        }
        const float seg_w = 18.0f - (float)seg * 3.5f;
        ink_stroke(r, &spine[i0], count,
                   (BrushProfile){.w_max = seg_w,
                                  .w_min_ratio = 0.18f,
                                  .entry = 0.12f,
                                  .exit = 0.35f,
                                  .dry = 0.95f,
                                  .seed = seed + 60u + (uint32_t)seg * 7u},
                   (InkTone){.color = paper_pal, .alpha = 0.56f}, 1.0f);
        ink_stroke(r, &spine[i0], count,
                   (BrushProfile){.w_max = seg_w * 0.48f,
                                  .w_min_ratio = 0.15f,
                                  .entry = 0.10f,
                                  .exit = 0.40f,
                                  .dry = 0.95f,
                                  .seed = seed + 90u + (uint32_t)seg * 11u},
                   (InkTone){.color = PAL_GOFUN, .alpha = 0.42f}, 1.0f);
    }
}

static void draw_ryu_scales(RenderState *r, const Vector2 *spine, int spine_n, uint8_t broken_mask,
                            uint32_t seed)
{
    const int scale_count = 44;
    for (int s = 0; s < scale_count; ++s) {
        const float norm_s = ((float)s + 0.5f) / (float)scale_count;
        const int third = (int)(norm_s * (float)GAME_RYU_ORB_COUNT);
        const int clamped = third < GAME_RYU_ORB_COUNT ? third : (GAME_RYU_ORB_COUNT - 1);
        if ((broken_mask & (uint8_t)(1u << (unsigned)clamped)) != 0u && (s % 2 == 0)) {
            continue;
        }
        const float pos_f = norm_s * (float)(spine_n - 1);
        int idx = (int)pos_f;
        idx = idx >= spine_n - 1 ? spine_n - 2 : idx;
        const float frac = pos_f - (float)idx;
        const Vector2 p0 = spine[idx];
        const Vector2 p1 = spine[idx + 1];
        const Vector2 at = {p0.x + (p1.x - p0.x) * frac, p0.y + (p1.y - p0.y) * frac};
        const float dx = p1.x - p0.x;
        const float dy = p1.y - p0.y;
        const float dl = sqrtf(dx * dx + dy * dy);
        const Vector2 nrm = dl > 1e-3f ? (Vector2){-dy / dl, dx / dl} : (Vector2){0.0f, 1.0f};
        const Vector2 tan_v = dl > 1e-3f ? (Vector2){dx / dl, dy / dl} : (Vector2){0.0f, 0.0f};
        const float side = (s & 1) ? 1.0f : -1.0f;
        const float taper = 1.0f - ((float)s / (float)scale_count) * 0.65f;
        const Vector2 tick[2] = {
            {at.x + nrm.x * side * 3.0f * taper, at.y + nrm.y * side * 3.0f * taper},
            {at.x + nrm.x * side * 12.0f * taper + tan_v.x * 4.0f,
             at.y + nrm.y * side * 12.0f * taper + tan_v.y * 4.0f},
        };
        ink_stroke(
            r, tick, 2,
            (BrushProfile){.w_max = 2.6f,
                           .w_min_ratio = 0.25f,
                           .entry = 0.15f,
                           .exit = 0.45f,
                           .dry = 0.35f,
                           .seed = seed + 200u + (uint32_t)s},
            (InkTone){.color = (s % 2 == 0) ? PAL_SUMI_LIGHT : PAL_SUMI_HEAVY, .alpha = 0.72f},
            1.0f);
    }
}

static void draw_ryu_whiskers(RenderState *r, const Vector2 *spine, int spine_n, const SprPlan *p,
                              float rad, float t, uint32_t seed)
{
    if (spine_n < 4) {
        return;
    }
    Vector2 back[3];
    for (int k = 0; k < 3; ++k) {
        const float dx = spine[k + 1].x - spine[k].x;
        const float dy = spine[k + 1].y - spine[k].y;
        const float dl = sqrtf(dx * dx + dy * dy);
        back[k] = dl > 1e-3f ? (Vector2){dx / dl, dy / dl} : (Vector2){-1.0f, 0.0f};
    }
    const SpriteAnchorId roots[2] = {SPR_ANCHOR_WHISKER_A, SPR_ANCHOR_WHISKER_B};
    const PaletteId pals[2] = {PAL_SUMI_DARK, PAL_SUMI_HEAVY};
    for (int w = 0; w < 2; ++w) {
        SpriteUv uv;
        if (!sprite_anchor(p->def, roots[w], &uv)) {
            continue;
        }
        const Vector2 root = plan_point(p, uv);
        const float len = rad * (w == 0 ? 1.25f : 1.05f);
        const float flutter = 0.15f * sinf(k_spr_two_pi * 1.3f * t + (float)w * 1.1f);
        Vector2 pts[4] = {root, root, root, root};
        for (int k = 0; k < 3; ++k) {
            const float a = flutter * (float)(k + 1) / 3.0f;
            const float c = cosf(a);
            const float s = sinf(a);
            const Vector2 d = {back[k].x * c - back[k].y * s, back[k].x * s + back[k].y * c};
            const float reach = len * (float)(k + 1) / 3.0f;
            pts[k + 1] = (Vector2){root.x + d.x * reach, root.y + d.y * reach};
        }
        ink_stroke(r, pts, 4,
                   (BrushProfile){.w_max = 3.8f,
                                  .w_min_ratio = 0.20f,
                                  .entry = 0.12f,
                                  .exit = 0.55f,
                                  .dry = 0.30f,
                                  .seed = seed + 18u + (uint32_t)w},
                   (InkTone){.color = pals[w], .alpha = (w == 0 ? 0.88f : 0.84f) * p->body_a},
                   1.0f);
    }
}

static void draw_ryu_horn_gold(RenderState *r, const SprPlan *p, uint32_t seed)
{
    const BrushProfile gold_p = {.w_max = 2.0f,
                                 .w_min_ratio = 0.25f,
                                 .entry = 0.18f,
                                 .exit = 0.55f,
                                 .dry = 0.20f,
                                 .seed = seed + 22u};
    const InkTone gold_t = {.color = PAL_KIN_HOT, .alpha = 0.82f * p->body_a};
    Vector2 horn_a[3];
    Vector2 horn_b[3];
    for (int k = 0; k < 3; ++k) {
        horn_a[k] = plan_point(p, p->def->horn_a[k]);
        horn_b[k] = plan_point(p, p->def->horn_b[k]);
    }
    ink_stroke(r, horn_a, 3, gold_p, gold_t, 1.0f);
    ink_stroke(r, horn_b, 3, gold_p, gold_t, 1.0f);
}

static void draw_ryu_snout_orb(RenderState *r, const BossComp *boss, const SprPlan *p)
{
    SpriteUv uv;
    if (!sprite_anchor(p->def, SPR_ANCHOR_WHISKER_A, &uv)) {
        return;
    }
    const Vector2 at = plan_point(p, (SpriteUv){uv.u + 0.04f, uv.v - 0.06f});
    const bool tele = boss != NULL && boss->telegraph > 0.01f;
    const int32_t kind = boss != NULL ? boss->part_id : 0;
    const float orb_r = tele ? 16.5f : 13.0f;
    const PaletteId outer = kind == 0 ? PAL_SHU : (kind == 1 ? PAL_AO : PAL_GOFUN);
    const PaletteId inner = kind == 0 ? PAL_SHU_HOT : (kind == 1 ? PAL_AO_HOT : PAL_SUMI_LIGHT);
    ink_teardrop(r, at, (Vector2){0.0f, -1.0f}, orb_r * 1.45f, orb_r * 1.2f,
                 pal_color_alpha(outer, 0.94f * p->body_a));
    ink_teardrop(r, at, (Vector2){0.0f, -1.0f}, orb_r * (tele ? 0.95f : 0.72f), orb_r * 0.72f,
                 pal_color_alpha(inner, 0.98f * p->body_a));
}

static void draw_ryu(RenderState *r, const GameState *g, EntityId id, const TransformComp *tr,
                     const BossComp *boss, const ShrineState *shrine, const SprPlan *p,
                     const GameVec2 *cached_spine, int32_t cached_n, uint32_t seed, int act_index)
{
    const bool awakened = boss != NULL && boss->phase_idx >= 1;
    const bool ascending = boss != NULL && boss->phase_idx >= 2;
    const bool body_trail = has_ryu_body_trail(g, id);
    const uint8_t broken_mask = body_trail ? g->ryu_body.orb_broken_mask : 0u;
    const float t = game_time_s(g);
    if (p->dissolve <= 0.0f) {
        render_draw_ryu_telegraph(r, boss, shrine, seed);
    }

    Vector2 spine[RENDER_RYU_SPINE_PTS];
    const int n = (cached_spine != NULL && cached_n >= 2)
                      ? ryu_spine_to_vec2(cached_spine, cached_n, spine)
                      : 0;
    if (n >= 2) {
        const BrushProfile prof = ryu_spine_profile(sprite_ryu_spine_width(awakened, t), seed);
        const InkTone tone = {.color = awakened ? PAL_SUMI_SCORCH : PAL_SUMI_DARK, .alpha = 0.96f};
        if (p->dissolving) {
            Vector2 unravel[RENDER_RYU_SPINE_PTS];
            for (int i = 0; i < n; ++i) {
                unravel[i] = spine[i];
            }
            reverse_points(unravel, n);
            ink_stroke(r, unravel, n, prof, tone, 1.0f - p->dissolve);
        } else {
            ink_stroke(r, spine, n, prof, tone, 1.0f);
            draw_ryu_broken_overlay(r, spine, n, broken_mask, seed, act_index);
            draw_ryu_scales(r, spine, n, broken_mask, seed);
        }
    }

    if (p->body_a <= 0.001f) {
        return;
    }
    draw_body(r, p);
    if (n >= 2) {
        draw_ryu_whiskers(r, spine, n, p, entity_radius(tr, 46.0f), t, seed);
    }
    if (awakened || r->ryu_awaken_t > 0.0f) {
        const float dot_scale =
            r->ryu_awaken_t > 0.0f ? (1.6f - 0.6f * (1.0f - r->ryu_awaken_t / 1.5f)) : 1.15f;
        const Vector2 eye = plan_anchor(p, SPR_ANCHOR_EYE_L, (Vector2){tr->x, tr->y});
        render_draw_eye_brush(r, eye, 5.2f * dot_scale, PAL_SHU, p->body_a);
        render_draw_eye_brush(r, eye, 2.5f * dot_scale, PAL_SHU_HOT, p->body_a);
    }
    if (ascending) {
        draw_ryu_horn_gold(r, p, seed);
    }
    if (!body_trail) {
        draw_ryu_snout_orb(r, boss, p);
    }
}

static void draw_hoju(RenderState *r, const TransformComp *tr, const BossComp *boss,
                      const SprPlan *p, uint32_t seed, uint64_t ticks)
{
    draw_body(r, p);
    const int32_t part = boss != NULL ? boss->part_id : 0;
    const float tele = boss != NULL ? clamp01_finite(boss->telegraph) : 0.0f;
    const float rad = entity_radius(tr, 30.0f) * orb_pulse(boss, ticks);
    const Vector2 core = plan_anchor(p, SPR_ANCHOR_CORE, (Vector2){tr->x, tr->y});
    const float a = p->body_a;
    if (part == 0 || part == 1) {
        const PaletteId hot = part == 0 ? PAL_SHU_HOT : PAL_AO_HOT;
        ink_teardrop(r, core, (Vector2){0.0f, -1.0f}, rad * (0.55f + 0.35f * tele),
                     rad * (0.42f + 0.22f * tele),
                     pal_color_alpha(hot, (0.85f + 0.13f * tele) * a));
        for (int k = -1; k <= 1; k += 2) {
            const float sk = (float)k;
            if (part == 0) {
                const Vector2 flick[3] = {
                    {tr->x + sk * rad * 0.82f, tr->y + rad * 0.28f},
                    {tr->x + sk * rad * 1.12f, tr->y - rad * 0.36f},
                    {tr->x + sk * rad * 0.42f, tr->y - rad * 1.24f},
                };
                ink_stroke(
                    r, flick, 3,
                    (BrushProfile){.w_max = 2.6f,
                                   .w_min_ratio = 0.20f,
                                   .entry = 0.14f,
                                   .exit = 0.55f,
                                   .dry = 0.22f,
                                   .seed = seed + 12u + (uint32_t)(k + 1)},
                    (InkTone){.color = tele > 0.01f ? PAL_SHU_HOT : PAL_SHU, .alpha = 0.85f * a},
                    1.0f);
            } else {
                const Vector2 crackle[4] = {
                    {tr->x + sk * rad * 0.95f, tr->y + rad * 0.35f},
                    {tr->x + sk * rad * 1.22f, tr->y - rad * 0.08f},
                    {tr->x + sk * rad * 0.88f, tr->y - rad * 0.58f},
                    {tr->x + sk * rad * 0.38f, tr->y - rad * 1.18f},
                };
                ink_stroke(r, crackle, 4,
                           (BrushProfile){.w_max = 2.4f,
                                          .w_min_ratio = 0.28f,
                                          .entry = 0.12f,
                                          .exit = 0.42f,
                                          .dry = 0.0f,
                                          .seed = seed + 22u + (uint32_t)(k + 1)},
                           (InkTone){.color = PAL_AO_HOT, .alpha = 0.92f * a}, 1.0f);
            }
        }
    } else {
        const float calm = p->def->extent.u * p->xf.size;
        const float spin = (float)ticks * (3.2f / (float)INK_TICK_HZ);
        ink_arc_stroke(r, core, calm, spin, spin + 4.7f,
                       (BrushProfile){.w_max = 3.0f,
                                      .w_min_ratio = 0.24f,
                                      .entry = 0.14f,
                                      .exit = 0.42f,
                                      .dry = 0.20f,
                                      .seed = seed + 30u},
                       (InkTone){.color = tele > 0.01f ? PAL_SUMI_SCORCH : PAL_SUMI_LIGHT,
                                 .alpha = 0.94f * a});
        ink_arc_stroke(r, core, calm * 0.5f, spin + 1.4f, spin + 5.6f,
                       (BrushProfile){.w_max = 2.2f,
                                      .w_min_ratio = 0.30f,
                                      .entry = 0.12f,
                                      .exit = 0.35f,
                                      .dry = 0.15f,
                                      .seed = seed + 31u},
                       (InkTone){.color = PAL_SUMI_DARK, .alpha = 0.90f * a});
    }
    if (tele > 0.01f) {
        const PaletteId glow = (part == 0) ? PAL_SHU_HOT : ((part == 1) ? PAL_AO_HOT : PAL_GOFUN);
        ink_wash(r, core, rad * 0.62f, rad * 0.62f, 0.0f,
                 (InkTone){.color = glow, .alpha = 0.65f * tele * a}, 1, seed + 40u);
    }
}

static void draw_dissolve(RenderState *r, const TransformComp *tr, EntityId id, const SprPlan *p,
                          uint32_t seed)
{
    const float u = p->dissolve;
    const float rad0 = entity_radius(tr, 18.0f);
    const float rad = rad0 * (1.0f + 0.55f * u);
    ink_wash(r, (Vector2){tr->x, tr->y}, rad, rad * 0.86f, u * 0.6f,
             (InkTone){.color = PAL_SUMI_HEAVY, .alpha = (1.0f - u) * 0.72f}, 2, seed);
    draw_body(r, p);
    const uint32_t h = render_entity_seed(id, 7u, 0u);
    for (int k = 0; k < 3; ++k) {
        const float ang = (float)((h >> (k * 8)) & 0xffu) * (k_spr_two_pi / 256.0f) +
                          (float)k * (k_spr_two_pi / 3.0f);
        const Vector2 d = {cosf(ang), sinf(ang)};
        const Vector2 n = {-d.y, d.x};
        const float r0 = rad0 * 0.8f;
        const Vector2 pts[3] = {
            {tr->x + d.x * r0, tr->y + d.y * r0},
            {tr->x + d.x * (r0 + rad0 * 0.7f) + n.x * rad0 * 0.25f,
             tr->y + d.y * (r0 + rad0 * 0.7f) + n.y * rad0 * 0.25f},
            {tr->x + d.x * (r0 + rad0 * 1.4f), tr->y + d.y * (r0 + rad0 * 1.4f)},
        };
        ink_stroke(r, pts, 3,
                   (BrushProfile){.w_max = 3.2f,
                                  .w_min_ratio = 0.2f,
                                  .entry = 0.1f,
                                  .exit = 0.5f,
                                  .dry = 0.5f,
                                  .seed = seed + 300u + (uint32_t)k},
                   (InkTone){.color = PAL_SUMI_HEAVY, .alpha = (1.0f - u) * 0.8f}, u);
    }
}

static void snapshot_live_ryu(RenderState *r, const GameState *g, EntityId id,
                              const TransformComp *tr, const BossComp *boss, const SprPlan *p,
                              const GameVec2 *spine, int32_t spine_n, uint32_t seed)
{
    if (r->ryu_snap.active || tr->dissolving) {
        return;
    }
    const bool body_trail = has_ryu_body_trail(g, id);
    r->ryu_snap.live = true;
    r->ryu_snap.snap_tick = g->ticks;
    r->ryu_snap.head_id = id;
    r->ryu_snap.plan = *p;
    r->ryu_snap.spine_n = spine_n > RENDER_RYU_SPINE_PTS ? RENDER_RYU_SPINE_PTS : spine_n;
    if (spine != NULL && r->ryu_snap.spine_n > 0) {
        memcpy(r->ryu_snap.spine, spine, (size_t)r->ryu_snap.spine_n * sizeof(GameVec2));
    }
    r->ryu_snap.awakened = boss != NULL && boss->phase_idx >= 1;
    r->ryu_snap.broken_mask = body_trail ? g->ryu_body.orb_broken_mask : 0u;
    r->ryu_snap.seed = seed;
    r->ryu_snap.x = finite_or(tr->x, 0.0f);
    r->ryu_snap.y = finite_or(tr->y, 0.0f);
    r->ryu_snap.radius = entity_radius(tr, 46.0f);
}

void render_sprite_on_ryu_kill(RenderState *r)
{
    if (r == NULL || !r->sprites.enabled || !r->ryu_snap.live || r->ryu_snap.active) {
        return;
    }
    r->ryu_snap.active = true;
    /* 撃破と同 tick の render_sprite_tick で 0.0s に揃え、初フレームを飛ばさない */
    r->ryu_snap.t_s = -1.0f;
}

void render_sprite_tick(RenderState *r, const GameState *g, float dt)
{
    if (r == NULL || g == NULL) {
        return;
    }
    if (r->ryu_snap.active) {
        if (r->ryu_snap.t_s < 0.0f) {
            r->ryu_snap.t_s = 0.0f;
        } else {
            r->ryu_snap.t_s += finite_or(dt, 0.0f);
            if (r->ryu_snap.t_s >= 0.6f) {
                r->ryu_snap.active = false;
                r->ryu_snap.live = false;
            }
        }
    }
    if (!r->sprites.enabled) {
        return;
    }
    const ShrineState *shrine = g->shrine_count > 0 ? &g->shrines[0] : NULL;
    const uint32_t hw = g->ecs.high_water < ECS_MAX_ENTITIES ? g->ecs.high_water : ECS_MAX_ENTITIES;
    const uint64_t boil = g->ticks / 6u;
    for (uint32_t i = 0; i < hw; ++i) {
        if (!g->ecs.alive[i]) {
            continue;
        }
        const EntityId id = {.index = i, .gen = g->ecs.gen[i]};
        const EnemyComp *en = game_enemy(g, id);
        const TransformComp *tr = game_transform(g, id);
        if (en == NULL || tr == NULL) {
            continue;
        }
        const BossComp *boss = game_boss(g, id);
        const SpriteStateIn in = {.def_id = en->def_id,
                                  .windup = finite_or(en->windup, 0.0f),
                                  .ai_state = en->ai_state,
                                  .phase_milli = en->phase,
                                  .telegraph =
                                      boss != NULL ? finite_or(boss->telegraph, 0.0f) : 0.0f,
                                  .phase_idx = boss != NULL ? boss->phase_idx : 0,
                                  .part_id = boss != NULL ? boss->part_id : 0};
        const SpriteId spr = render_sprite_resolve(r, sprite_select(&in));
        if (spr == SPR_NONE) {
            continue;
        }
        const SpriteDef *d = sprite_def(spr);
        if (d->orient == SPR_ORIENT_MIRROR_X || d->orient == SPR_ORIENT_FACE_SHRINE) {
            const float cos_f = compute_cos_facing(d, tr, en, game_velocity(g, id), shrine);
            (void)update_face(r, id, cos_f, g->ticks, NULL);
        }
        if (en->def_id == ENEMY_BOSS_RYU && !tr->dissolving && !r->ryu_snap.active) {
            GameVec2 raw[RENDER_RYU_SPINE_PTS];
            const int32_t raw_n = ryu_raw_spine(r, g, id, tr, raw);
            SprPlan plan;
            if (plan_enemy_with_raw(r, g, id, shrine, raw, raw_n, &plan)) {
                GameVec2 spine[RENDER_RYU_SPINE_PTS];
                const int32_t sn = ryu_prepare_spine(g, id, tr, boss, &plan, raw, raw_n, spine);
                snapshot_live_ryu(r, g, id, tr, boss, &plan, spine, sn,
                                  render_entity_seed(id, 3u, boil));
            }
        }
    }
}

static void ensure_frame_cache(RenderState *r, const GameState *g)
{
    SpriteFrameCache *fc = &r->sprite_frame;
    if (fc->built && fc->frame == r->frames_drawn) {
        return;
    }
    const uint32_t prev_hw = fc->high_water < ECS_MAX_ENTITIES ? fc->high_water : ECS_MAX_ENTITIES;
    for (uint32_t i = 0; i < prev_hw; ++i) {
        fc->ent[i].valid = false;
    }
    fc->frame = r->frames_drawn;
    fc->built = true;
    fc->lantern_n = 0;
    fc->ryu_ok = false;
    fc->ryu_spine_n = 0;

    for (int32_t s = 0; s < GAME_MAX_SHRINES; ++s) {
        fc->shrine_ok[s] = s < g->shrine_count && plan_shrine(r, g, s, &fc->shrine[s]);
    }

    const ShrineState *shrine = g->shrine_count > 0 ? &g->shrines[0] : NULL;
    const uint32_t hw = g->ecs.high_water < ECS_MAX_ENTITIES ? g->ecs.high_water : ECS_MAX_ENTITIES;
    fc->high_water = hw;
    const uint64_t boil = g->ticks / 6u;
    for (uint32_t i = 0; i < hw; ++i) {
        fc->ent[i].valid = false;
        if (!g->ecs.alive[i]) {
            continue;
        }
        const EntityId id = {.index = i, .gen = g->ecs.gen[i]};
        const EnemyComp *en = game_enemy(g, id);
        const TransformComp *tr = game_transform(g, id);
        if (en == NULL || tr == NULL) {
            continue;
        }
        GameVec2 raw[RENDER_RYU_SPINE_PTS];
        int32_t raw_n = 0;
        if (en->def_id == ENEMY_BOSS_RYU) {
            if (!tr->dissolving && r->ryu_snap.live && !r->ryu_snap.active &&
                r->ryu_snap.snap_tick == g->ticks && r->ryu_snap.head_id.index == id.index &&
                r->ryu_snap.head_id.gen == id.gen) {
                const uint32_t seed = r->ryu_snap.seed;
                fc->ent[i] = (SpriteFrameEntry){
                    .valid = true, .gen = id.gen, .seed = seed, .plan = r->ryu_snap.plan};
                fc->ryu_spine_n = r->ryu_snap.spine_n;
                if (fc->ryu_spine_n > 0) {
                    memcpy(fc->ryu_spine, r->ryu_snap.spine,
                           (size_t)fc->ryu_spine_n * sizeof(GameVec2));
                }
                fc->ryu_ok = true;
                fc->ryu_id = id;
                continue;
            }
            raw_n = ryu_raw_spine(r, g, id, tr, raw);
        }
        SprPlan plan;
        if (!plan_enemy_with_raw(r, g, id, shrine, raw_n > 0 ? raw : NULL, raw_n, &plan)) {
            continue;
        }
        const uint32_t seed = render_entity_seed(id, 3u, boil);
        fc->ent[i] = (SpriteFrameEntry){.valid = true, .gen = id.gen, .seed = seed, .plan = plan};
        if (!tr->dissolving &&
            (en->def_id == ENEMY_LANTERN_FIRE || en->def_id == ENEMY_LANTERN_STORM) &&
            fc->lantern_n < ECS_MAX_ENTITIES) {
            fc->lanterns[fc->lantern_n++] = (uint16_t)i;
        }
        if (en->def_id == ENEMY_BOSS_RYU) {
            const BossComp *boss = game_boss(g, id);
            fc->ryu_spine_n = ryu_prepare_spine(g, id, tr, boss, &plan, raw, raw_n, fc->ryu_spine);
            fc->ryu_ok = true;
            fc->ryu_id = id;
            snapshot_live_ryu(r, g, id, tr, boss, &plan, fc->ryu_spine, fc->ryu_spine_n, seed);
        }
    }
}

void render_sprite_draw_halos(RenderState *r, const GameState *g, int act_index)
{
    if (r == NULL || g == NULL || !r->sprites.enabled) {
        return;
    }
    ensure_frame_cache(r, g);
    SpriteFrameCache *fc = &r->sprite_frame;
    for (int32_t s = 0; s < g->shrine_count && s < GAME_MAX_SHRINES; ++s) {
        if (fc->shrine_ok[s]) {
            draw_halo(r, &fc->shrine[s], act_index);
        }
    }
    const float t_s = game_time_s(g);
    for (uint32_t i = 0; i < fc->high_water; ++i) {
        if (!fc->ent[i].valid || !g->ecs.alive[i]) {
            continue;
        }
        const EntityId id = {.index = i, .gen = fc->ent[i].gen};
        const EnemyComp *en = game_enemy(g, id);
        if (en == NULL || en->def_id == ENEMY_BOSS_RYU_ORB) {
            continue;
        }
        SprPlan p = fc->ent[i].plan;
        const uint32_t seed = fc->ent[i].seed;
        const float appendage_a = p.halo_a / k_spr_halo_alpha;
        Vector2 tail[4];
        switch (en->def_id) {
        case ENEMY_BOSS_RYU: {
            const BossComp *boss = game_boss(g, id);
            const bool awakened = boss != NULL && boss->phase_idx >= 1;
            if (fc->ryu_ok && fc->ryu_id.index == id.index && fc->ryu_id.gen == id.gen) {
                draw_ryu_spine_halo_pts(r, fc->ryu_spine, fc->ryu_spine_n, awakened, t_s,
                                        p.dissolving, p.dissolve, p.halo_a, seed, act_index);
            }
            if (p.dissolving) {
                /* 頭は胴より先に消えるので、頭の形の紙抜きだけが残らないよう縁取りも合わせて薄める
                 */
                p.halo_a *= clamp01_finite(p.body_a);
            }
            break;
        }
        case ENEMY_OFUDA_FOX:
            if (!p.dissolving && fox_tail(&p, g->ticks, stable_phase(id), tail) == 4) {
                ink_stroke_knockout(r, tail, 4, fox_tail_profile(seed), 3.0f, act_index,
                                    appendage_a, 1.0f);
            }
            break;
        case ENEMY_BOSS_NUE:
            if (!p.dissolving && nue_tail(&p, g->ticks, stable_phase(id), tail) == 4) {
                ink_stroke_knockout(r, tail, 4, nue_tail_profile(seed), 4.0f, act_index,
                                    appendage_a, 1.0f);
            }
            break;
        default:
            break;
        }
        draw_halo(r, &p, act_index);
    }

    if (r->ryu_snap.active) {
        const RyuDeathSnap *sn = &r->ryu_snap;
        const float ts = sn->t_s > 0.0f ? sn->t_s : 0.0f;
        const float u = clamp01_finite(ts / 0.6f);
        const float halo_a = k_spr_halo_alpha * powf(1.0f - u, 0.7f);
        const float head_a = 1.0f - clamp01_finite(ts / 0.3f);
        draw_ryu_spine_halo_pts(r, sn->spine, sn->spine_n, sn->awakened, ts, true, u, halo_a,
                                sn->seed, act_index);
        if (head_a > 0.001f) {
            SprPlan hp = sn->plan;
            hp.xf.size *= 1.0f + 0.10f * u;
            hp.xf.dy += 4.0f * u;
            hp.halo_a = halo_a * head_a;
            draw_halo(r, &hp, act_index);
        }
    }
}

void render_sprite_draw_ryu_death(RenderState *r, int act_index)
{
    (void)act_index;
    if (r == NULL || !r->sprites.enabled || !r->ryu_snap.active) {
        return;
    }
    const RyuDeathSnap *sn = &r->ryu_snap;
    const float ts = sn->t_s > 0.0f ? sn->t_s : 0.0f;
    const float u = clamp01_finite(ts / 0.6f);
    const float head_a = 1.0f - clamp01_finite(ts / 0.3f);
    const float rad = sn->radius * (1.0f + 0.55f * u);
    ink_wash(r, (Vector2){sn->x, sn->y}, rad, rad * 0.86f, u * 0.6f,
             (InkTone){.color = PAL_SUMI_HEAVY, .alpha = (1.0f - u) * 0.72f}, 2, sn->seed);
    if (sn->spine_n >= 2) {
        Vector2 unravel[RENDER_RYU_SPINE_PTS];
        const int n = ryu_spine_to_vec2(sn->spine, sn->spine_n, unravel);
        reverse_points(unravel, n);
        const BrushProfile prof =
            ryu_spine_profile(sprite_ryu_spine_width(sn->awakened, ts), sn->seed);
        const InkTone tone = {.color = sn->awakened ? PAL_SUMI_SCORCH : PAL_SUMI_DARK,
                              .alpha = 0.96f};
        ink_stroke(r, unravel, n, prof, tone, 1.0f - u);
    }
    if (head_a > 0.001f) {
        SprPlan hp = sn->plan;
        hp.xf.size *= 1.0f + 0.10f * u;
        hp.xf.dy += 4.0f * u;
        hp.body_a *= head_a;
        draw_body(r, &hp);
    }
}

bool render_sprite_draw_shrine(RenderState *r, const GameState *g, int32_t shrine_index,
                               int act_index)
{
    (void)act_index;
    if (r == NULL || g == NULL || !r->sprites.enabled || shrine_index < 0 ||
        shrine_index >= GAME_MAX_SHRINES) {
        return false;
    }
    ensure_frame_cache(r, g);
    if (!r->sprite_frame.shrine_ok[shrine_index]) {
        return false;
    }
    draw_body(r, &r->sprite_frame.shrine[shrine_index]);
    return true;
}

bool render_sprite_draw_enemy(RenderState *r, const GameState *g, EntityId id,
                              const ShrineState *shrine, int act_index)
{
    if (r == NULL || g == NULL || !r->sprites.enabled || id.index >= ECS_MAX_ENTITIES) {
        return false;
    }
    ensure_frame_cache(r, g);
    const SpriteFrameEntry *entry = &r->sprite_frame.ent[id.index];
    if (!entry->valid || entry->gen != id.gen) {
        return false;
    }
    const TransformComp *tr = game_transform(g, id);
    const EnemyComp *en = game_enemy(g, id);
    if (tr == NULL || en == NULL) {
        return false;
    }
    const BossComp *boss = game_boss(g, id);
    const SprPlan *p = &entry->plan;
    const uint32_t seed = entry->seed;
    const GameVec2 *ryu_pts = (r->sprite_frame.ryu_ok && r->sprite_frame.ryu_id.index == id.index &&
                               r->sprite_frame.ryu_id.gen == id.gen)
                                  ? r->sprite_frame.ryu_spine
                                  : NULL;
    const int32_t ryu_n = ryu_pts != NULL ? r->sprite_frame.ryu_spine_n : 0;
    if (en->def_id == ENEMY_BOSS_RYU_ORB) {
        draw_halo(r, p, act_index);
    }
    if (tr->dissolving) {
        if (en->def_id == ENEMY_BOSS_RYU) {
            const float rad = entity_radius(tr, 18.0f) * (1.0f + 0.55f * p->dissolve);
            ink_wash(r, (Vector2){tr->x, tr->y}, rad, rad * 0.86f, p->dissolve * 0.6f,
                     (InkTone){.color = PAL_SUMI_HEAVY, .alpha = (1.0f - p->dissolve) * 0.72f}, 2,
                     seed);
            draw_ryu(r, g, id, tr, boss, shrine, p, ryu_pts, ryu_n, seed, act_index);
        } else {
            draw_dissolve(r, tr, id, p, seed);
        }
        return true;
    }

    draw_elite_rims(r, en, p, act_index);
    switch (en->def_id) {
    case ENEMY_ARCHER:
        draw_archer(r, tr, en, p, seed);
        break;
    case ENEMY_LANTERN_FIRE:
    case ENEMY_LANTERN_STORM:
        draw_lantern(r, tr, en, p, seed);
        break;
    case ENEMY_UMBRELLA:
        draw_umbrella(r, tr, en, p, seed);
        break;
    case ENEMY_KAMAITACHI:
        draw_kamaitachi(r, g, id, tr, p, seed);
        break;
    case ENEMY_INK_LEECH:
        draw_ink_leech(r, tr, en, id, p, seed, g->ticks);
        break;
    case ENEMY_OFUDA_FOX:
        draw_ofuda_fox(r, tr, en, id, p, seed, g->ticks);
        break;
    case ENEMY_BOSS_TENGU:
        draw_tengu(r, tr, boss, shrine, p, seed);
        break;
    case ENEMY_BOSS_NUE:
        draw_nue(r, tr, en, boss, id, p, seed, g->ticks);
        break;
    case ENEMY_BOSS_RYU:
        draw_ryu(r, g, id, tr, boss, shrine, p, ryu_pts, ryu_n, seed, act_index);
        break;
    case ENEMY_BOSS_RYU_ORB:
        draw_hoju(r, tr, boss, p, seed, g->ticks);
        break;
    case ENEMY_STONE_ONI:
    default:
        draw_body(r, p);
        break;
    }
    return true;
}

static void begin_stain(RenderState *r, bool *blending)
{
    if (!*blending) {
        render_set_blend(r, BLEND_MULTIPLIED);
        *blending = true;
    }
}

void render_sprite_draw_tints(RenderState *r, const GameState *g)
{
    if (r == NULL || g == NULL || !r->sprites.enabled) {
        return;
    }
    ensure_frame_cache(r, g);
    const SpriteFrameCache *fc = &r->sprite_frame;
    bool blending = false;
    const Color clear = {0, 0, 0, 0};
    for (int32_t s = 0; s < g->shrine_count && s < GAME_MAX_SHRINES; ++s) {
        const ShrineState *sh = &g->shrines[s];
        if (sh->hp <= 0 || sh->max_hp <= 0 || !fc->shrine_ok[s]) {
            continue;
        }
        const SprPlan *p = &fc->shrine[s];
        const float hp_ratio = clamp01_finite((float)sh->hp / (float)sh->max_hp);
        const Vector2 niche = plan_anchor(p, SPR_ANCHOR_FLAME, (Vector2){sh->x, sh->y});
        begin_stain(r, &blending);
        ink_radial_glow(r, niche, p->def->extent.u * p->xf.size, p->def->extent.v * p->xf.size,
                        stain_color(PAL_SHU, 0.25f * hp_ratio), clear);
    }
    for (int32_t k = 0; k < fc->lantern_n; ++k) {
        const uint32_t i = fc->lanterns[k];
        if (!fc->ent[i].valid || !g->ecs.alive[i]) {
            continue;
        }
        const EntityId id = {.index = i, .gen = fc->ent[i].gen};
        const EnemyComp *en = game_enemy(g, id);
        const TransformComp *tr = game_transform(g, id);
        if (en == NULL || tr == NULL) {
            continue;
        }
        const SprPlan *p = &fc->ent[i].plan;
        const float pulse = lantern_pulse(en, g->ticks, stable_phase(id));
        const Vector2 glow = plan_anchor(p, SPR_ANCHOR_GLOW, (Vector2){tr->x, tr->y});
        const PaletteId pal = en->def_id == ENEMY_LANTERN_STORM ? PAL_AO : PAL_SHU;
        begin_stain(r, &blending);
        ink_radial_glow(r, glow, p->def->extent.u * p->xf.size * pulse,
                        p->def->extent.v * p->xf.size * pulse,
                        stain_color(pal, clamp01_finite(0.62f * pulse) * p->body_a), clear);
    }
    if (blending) {
        render_set_blend(r, BLEND_ALPHA);
    }
}

void render_sprite_draw_cores(RenderState *r, const GameState *g)
{
    if (r == NULL || g == NULL || !r->sprites.enabled) {
        return;
    }
    ensure_frame_cache(r, g);
    const SpriteFrameCache *fc = &r->sprite_frame;
    for (int32_t s = 0; s < g->shrine_count && s < GAME_MAX_SHRINES; ++s) {
        const ShrineState *sh = &g->shrines[s];
        if (sh->hp <= 0 || !fc->shrine_ok[s]) {
            continue;
        }
        const SprPlan *p = &fc->shrine[s];
        const float hp_ratio =
            sh->max_hp > 0 ? clamp01_finite((float)sh->hp / (float)sh->max_hp) : 0.0f;
        const Vector2 niche = plan_anchor(p, SPR_ANCHOR_FLAME, (Vector2){sh->x, sh->y});
        const float flicker =
            sinf((float)g->ticks * 0.22f) * (r->shrine_flicker_t > 0.0f ? 4.5f : 1.2f);
        const float flame_h = 9.0f + hp_ratio * 11.0f;
        const float flame_w = 5.5f + hp_ratio * 5.0f;
        const Vector2 base = {niche.x + flicker * 0.3f,
                              niche.y + p->def->extent.v * p->xf.size * 0.35f};
        ink_teardrop(r, base, (Vector2){flicker * 0.08f, -1.0f}, flame_h, flame_w,
                     pal_color_alpha(PAL_SHU, 0.90f));
        ink_teardrop(r, (Vector2){base.x, base.y + 1.5f}, (Vector2){0.0f, -1.0f}, flame_h * 0.58f,
                     flame_w * 0.54f, pal_color_alpha(PAL_SHU_HOT, 0.94f));
    }
    for (int32_t k = 0; k < fc->lantern_n; ++k) {
        const uint32_t i = fc->lanterns[k];
        if (!fc->ent[i].valid || !g->ecs.alive[i]) {
            continue;
        }
        const EntityId id = {.index = i, .gen = fc->ent[i].gen};
        const EnemyComp *en = game_enemy(g, id);
        const TransformComp *tr = game_transform(g, id);
        if (en == NULL || tr == NULL) {
            continue;
        }
        const SprPlan *p = &fc->ent[i].plan;
        const float rad = entity_radius(tr, 17.0f);
        const float pulse = lantern_pulse(en, g->ticks, stable_phase(id));
        const Vector2 m = plan_anchor(p, SPR_ANCHOR_MOUTH, (Vector2){tr->x, tr->y});
        const PaletteId hot = en->def_id == ENEMY_LANTERN_STORM ? PAL_AO_HOT : PAL_SHU_HOT;
        if (en->def_id == ENEMY_LANTERN_STORM) {
            /* 乗算の段より後に描き、蒼・明の火花を暗くしない */
            const float pull = clamp01_finite(en->windup);
            const uint32_t seed = fc->ent[i].seed;
            const float jitter = ((g->ticks / 4u + seed) & 1u) ? 4.0f : -4.0f;
            const Vector2 spark[4] = {
                {m.x - rad * 0.35f, m.y - rad * 0.15f},
                {m.x - 2.0f, m.y + rad * 0.15f + jitter * 0.4f},
                {m.x + 5.0f, m.y + rad * 0.05f},
                {m.x + rad * 0.5f, m.y + rad * 0.55f + pull * 4.0f},
            };
            ink_stroke(r, spark, 4,
                       (BrushProfile){.w_max = 2.6f,
                                      .w_min_ratio = 0.30f,
                                      .entry = 0.1f,
                                      .exit = 0.35f,
                                      .dry = 0.0f,
                                      .seed = seed + 16u},
                       (InkTone){.color = PAL_AO_HOT, .alpha = 0.94f * p->body_a}, 1.0f);
        }
        ink_teardrop(r, m, (Vector2){0.0f, -1.0f}, rad * 0.36f * pulse, rad * 0.26f * pulse,
                     pal_color_alpha(hot, clamp01_finite(0.88f * pulse) * p->body_a));
    }
}
