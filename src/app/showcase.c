#include "showcase.h"

#include "core.h"
#include "paths.h"
#include "raylib.h"

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static void paint_fluid_blob(FluidState *f, int ch, float cx, float cy, float rx, float ry,
                             float peak)
{
    float *chan = fluid_channel_mut(f, ch);
    if (chan == NULL || rx <= 0.1f || ry <= 0.1f) {
        return;
    }
    for (int y = 1; y < FLUID_H - 1; ++y) {
        for (int x = 1; x < FLUID_W - 1; ++x) {
            const float dx = ((float)x - cx) / rx;
            const float dy = ((float)y - cy) / ry;
            const float d2 = dx * dx + dy * dy;
            if (d2 < 1.0f) {
                const float falloff = 1.0f - d2;
                const float v = peak * falloff * (0.75f + 0.25f * falloff);
                if (v > chan[y * FLUID_W + x]) {
                    chan[y * FLUID_W + x] = v;
                }
            }
        }
    }
}

static void populate_demo_meta(MetaState *m)
{
    meta_init_default(m);
    m->ink_crystals = 420;
    m->upgrade_lv[META_UPG_SHRINE_HP] = 2;
    m->upgrade_lv[META_UPG_INK_MAX] = 1;
    m->upgrade_lv[META_UPG_REROLLS] = 1;
    m->upgrade_lv[META_UPG_INK_REGEN] = 1;
    m->unlocked_relic_mask = (UINT64_C(1) << 0) | (UINT64_C(1) << 2);
    m->best_act = 5;
    m->best_score = 48250;
    m->runs_played = 12;
    m->clears = 3;
}

static void spawn_showcase_gimmick(GameState *g, int32_t kind, int32_t elem_kind, float x, float y,
                                   float rad, float timer)
{
    const EntityId gid = game_spawn_entity(g, COMP_TRANSFORM | COMP_GIMMICK);
    TransformComp *tr = game_transform_mut(g, gid);
    GimmickComp *gm = game_gimmick_mut(g, gid);
    if (tr != NULL) {
        tr->x = x;
        tr->y = y;
        tr->radius = rad;
    }
    if (gm != NULL) {
        gm->kind = kind;
        gm->elem_kind = elem_kind;
        gm->timer = timer;
    }
}

static void populate_p2_entities_showcase(GameState *g, RenderState *r, UiView *ui)
{
    const RunConfig cfg = meta_run_config_preset(BRUSH_STYLE_HEAVY, UINT64_C(2026));
    game_init(g, &cfg);
    g->phase = RUN_PHASE_PLAYING;
    g->phase_t = 5.0f;
    g->run.act_index = 3; /* Act 4 (Endless Loop 1): Act 1 scenery + 20 gold sunago flecks */
    g->run.endless_loop = 1;
    g->run.wave_index = GAME_BOSS_WAVE_INDEX;
    g->run.score = 42900;
    g->run.chain = 6;
    g->run.chain_t = 2.4f;
    g->player.ink = 82.0f;
    g->player.ink_max = 100.0f;
    g->player.surge_t = 2.5f;
    g->player.cursor_x = 128.0f;
    g->player.cursor_y = 148.0f;
    g->player.cyclone_cd = 0.0f;
    g->player.cyclone_cd_total = 1.4f;
    g->player.gale_max_charges = 2;
    g->player.gale_charges = 1;
    g->player.gale_cd = 0.35f;
    g->player.gale_cd_total = 0.70f;
    if (ui != NULL) {
        ui->cursor_scale = 125;
    }

    g->shrine_count = 1;
    g->shrines[0].x = 112.0f;
    g->shrines[0].y = 336.0f;
    g->shrines[0].hp = 9;
    g->shrines[0].max_hp = 11;

    /* Ofuda Fox: idle + windup with orbiting talismans */
    const EntityId fox1 = game_debug_spawn_enemy(g, ENEMY_OFUDA_FOX, 236.0f, 152.0f);
    if (game_health_mut(g, fox1) != NULL) {
        game_health_mut(g, fox1)->flash_t = 0.0f;
    }
    const EntityId fox2 = game_debug_spawn_enemy(g, ENEMY_OFUDA_FOX, 356.0f, 152.0f);
    if (game_enemy_mut(g, fox2) != NULL) {
        game_enemy_mut(g, fox2)->windup = 0.85f;
    }
    if (game_health_mut(g, fox2) != NULL) {
        game_health_mut(g, fox2)->flash_t = 0.0f;
    }

    /* Ally shielded by Ofuda Barrier (StatusComp.barrier_t > 0) */
    const EntityId shielded = game_debug_spawn_enemy(g, ENEMY_STONE_ONI, 296.0f, 274.0f);
    if (game_status_mut(g, shielded) != NULL) {
        game_status_mut(g, shielded)->barrier_t = 3.6f;
    }
    if (game_health_mut(g, shielded) != NULL) {
        game_health_mut(g, shielded)->flash_t = 0.0f;
    }

    /* Homing Ofuda projectiles: clean + flying over fire blob (crimson scorched edge) */
    const EntityId of1 = game_debug_spawn_projectile(g, PROJ_HOMING_OFUDA, 238.0f, 412.0f, 140.0f,
                                                     -24.0f);
    ProjComp *op1 = game_proj_mut(g, of1);
    if (op1 != NULL) {
        op1->trail_n = 8;
        for (int t = 0; t < 8; ++t) {
            op1->trail[t] = (GameVec2){238.0f - (float)(7 - t) * 7.0f, 412.0f + (float)(7 - t)};
        }
    }
    const EntityId of2 = game_debug_spawn_projectile(g, PROJ_HOMING_OFUDA, 404.0f, 412.0f, 140.0f,
                                                     0.0f);
    ProjComp *op2 = game_proj_mut(g, of2);
    if (op2 != NULL) {
        op2->trail_n = 8;
        for (int t = 0; t < 8; ++t) {
            op2->trail[t] = (GameVec2){404.0f - (float)(7 - t) * 7.0f, 412.0f};
        }
    }

    /* Multi-part Ippitsu-Ryu with 64-pt RyuBodyState.trail, 2 live orbs + 1 broken orb */
    const EntityId ryu = game_debug_spawn_enemy(g, ENEMY_BOSS_RYU, 540.0f, 210.0f);
    BossComp *rbc = game_boss_mut(g, ryu);
    HealthComp *rhp = game_health_mut(g, ryu);
    if (rbc != NULL) {
        rbc->phase_idx = 0;
        rbc->part_id = 0;
    }
    if (rhp != NULL) {
        rhp->hp = 900;
        rhp->max_hp = 900;
        rhp->flash_t = 0.0f;
    }

    g->run.boss_id = ryu;
    g->ryu_body.head_id = ryu;
    g->ryu_body.orb_broken_mask = 0x2u; /* Middle Storm Orb shattered -> dry-brush fracture */
    g->ryu_body.active_orb_slot = 0;
    g->ryu_body.trail_count = 64;
    g->ryu_body.trail_head = 63;
    for (int k = 0; k < 64; ++k) {
        /* k=0 is oldest tail, k=63 is head */
        const float u = 1.0f - (float)k / 63.0f;
        const float tx = 540.0f + u * 610.0f;
        const float ty = 210.0f + sinf(u * 5.6f) * 96.0f + u * 130.0f;
        g->ryu_body.trail[k] = (GameVec2){tx, ty};
    }

    /* Independent Dragon Orbs: Fire (head, telegraphing) & Wind (tail); Storm broken */
    const GameVec2 p_fire = g->ryu_body.trail[63];
    const GameVec2 p_storm = g->ryu_body.trail[32];
    const GameVec2 p_wind = g->ryu_body.trail[0];
    const EntityId orb_fire = game_debug_spawn_enemy(g, ENEMY_BOSS_RYU_ORB, p_fire.x, p_fire.y);
    const EntityId orb_wind = game_debug_spawn_enemy(g, ENEMY_BOSS_RYU_ORB, p_wind.x, p_wind.y);
    if (game_boss_mut(g, orb_fire) != NULL) {
        game_boss_mut(g, orb_fire)->part_id = 0;
        game_boss_mut(g, orb_fire)->parent_id = ryu;
        game_boss_mut(g, orb_fire)->telegraph = 0.85f;
    }
    if (game_health_mut(g, orb_fire) != NULL) {
        game_health_mut(g, orb_fire)->flash_t = 0.0f;
    }
    if (game_boss_mut(g, orb_wind) != NULL) {
        game_boss_mut(g, orb_wind)->part_id = 2;
        game_boss_mut(g, orb_wind)->parent_id = ryu;
        game_boss_mut(g, orb_wind)->telegraph = 0.0f;
    }
    if (game_health_mut(g, orb_wind) != NULL) {
        game_health_mut(g, orb_wind)->flash_t = 0.0f;
    }
    g->ryu_body.orb_ids[0] = orb_fire;
    g->ryu_body.orb_ids[1] = (EntityId){ECS_INVALID_INDEX, 0};
    g->ryu_body.orb_ids[2] = orb_wind;

    fluid_reset(&g->fluid);
    paint_fluid_blob(&g->fluid, 1, 50.5f, 51.5f, 10.0f, 6.0f, 1.05f); /* Fire under 2nd ofuda */
    paint_fluid_blob(&g->fluid, 0, 26.0f, 64.0f, 18.0f, 5.0f, 1.15f);
    fluid_sanitize_channels(&g->fluid);

    if (r != NULL) {
        g->event_count = 0;
        game_push_event(
            g, (GameEvent){.type = EV_BOSS_PART_BREAK, .x = p_storm.x, .y = p_storm.y, .value = 1});
        render_consume_events(r, g);
        render_update_vfx(r, g, 0.14f);
        g->event_count = 0;
    }
}

static void populate_entities_and_projectiles(GameState *g, RenderState *r, bool heavy_ink_60pct)
{
    const RunConfig cfg = meta_run_config_preset(BRUSH_STYLE_HEAVY, UINT64_C(2026));
    game_init(g, &cfg);
    g->phase = RUN_PHASE_PLAYING;
    g->phase_t = 5.0f;
    g->run.act_index = heavy_ink_60pct ? 2 : 1; /* Show Act 2 torii+rocks on P1, Act 3 on P2 */
    g->run.wave_index = 1;
    g->run.wave_kills = 6;
    g->run.wave_quota = 12;
    g->run.score = 24850;
    g->run.chain = 5;
    g->run.chain_t = 2.2f;
    g->run.relics[0] = RELIC_VERMILION_INKSTONE;
    g->run.relics[1] = RELIC_THUNDER_DRUM;
    g->run.relic_count = 2;
    g->player.ink = 76.0f;
    g->player.ink_max = 100.0f;
    g->player.surge_t = 2.4f;
    g->player.cursor_x = 120.0f;
    g->player.cursor_y = 148.0f;
    g->player.cyclone_cd = 0.0f;
    g->player.cyclone_cd_total = 1.4f;
    g->player.gale_cd = 0.45f;
    g->player.gale_cd_total = 1.0f;

    /* Shrine on left-center */
    g->shrine_count = 1;
    g->shrines[0].x = 112.0f;
    g->shrines[0].y = 326.0f;
    g->shrines[0].hp = 9;
    g->shrines[0].max_hp = 11;

    /* All 7 regular enemy types + 4 elite mutations */
    static const EnemyDefId regulars[7] = {
        ENEMY_ARCHER,     ENEMY_LANTERN_FIRE, ENEMY_LANTERN_STORM, ENEMY_UMBRELLA,
        ENEMY_KAMAITACHI, ENEMY_STONE_ONI,    ENEMY_INK_LEECH,
    };
    static const uint32_t elite_masks[7] = {
        0u, ELITE_FIREPROOF, ELITE_STORMPROOF, ELITE_RIGID, 0u, ELITE_RIGID | ELITE_INK_REPEL, 0u,
    };
    for (int i = 0; i < 7; ++i) {
        const float x = 200.0f + (float)(i % 4) * 92.0f;
        const float y = (i < 4) ? 136.0f : 238.0f;
        const EntityId id = game_debug_spawn_enemy(g, regulars[i], x, y);
        TransformComp *tr = game_transform_mut(g, id);
        EnemyComp *en = game_enemy_mut(g, id);
        HealthComp *hp = game_health_mut(g, id);
        StatusComp *st = game_status_mut(g, id);
        if (tr != NULL) {
            tr->facing = 0.25f;
        }
        if (hp != NULL) {
            hp->flash_t = 0.0f;
        }
        if (en != NULL) {
            if (elite_masks[i] != 0u) {
                en->is_elite = true;
                en->elite_flags = elite_masks[i];
            }
            if (i == 6) {
                en->absorbed_ink = 1.4f;
            }
        }
        if (st != NULL) {
            if (i == 0) {
                st->ink_mark_t = 2.0f;
            } else if (i == 1) {
                st->burn_acc = 0.3f;
            } else if (i == 2) {
                st->charge_t = 1.5f;
            }
        }
    }

    /* All 3 Act Bosses: O-Tengu, Nue (paused crouched), Ippitsu-Ryu */
    const EntityId tengu = game_debug_spawn_enemy(g, ENEMY_BOSS_TENGU, 648.0f, 156.0f);
    BossComp *tbc = game_boss_mut(g, tengu);
    if (tbc != NULL) {
        tbc->telegraph = 0.6f;
        tbc->gale_dx = 0.0f;
        tbc->gale_dy = 1.0f;
    }
    if (game_health_mut(g, tengu) != NULL) {
        game_health_mut(g, tengu)->flash_t = 0.0f;
    }

    const EntityId nue = game_debug_spawn_enemy(g, ENEMY_BOSS_NUE, 845.0f, 162.0f);
    EnemyComp *nen = game_enemy_mut(g, nue);
    if (nen != NULL) {
        nen->ai_state = 1; /* Paused stance -> 1.3x thick outline */
    }
    if (game_health_mut(g, nue) != NULL) {
        game_health_mut(g, nue)->flash_t = 0.0f;
    }

    const EntityId ryu = game_debug_spawn_enemy(g, ENEMY_BOSS_RYU, 1020.0f, 178.0f);
    BossComp *rbc = game_boss_mut(g, ryu);
    if (rbc != NULL) {
        rbc->part_id = 0;
        rbc->phase_idx = 1; /* Awakened red eye */
    }
    if (game_health_mut(g, ryu) != NULL) {
        game_health_mut(g, ryu)->flash_t = 0.0f;
    }

    /* Spirit pillars (Fire & Storm lit) + Lightning strike telegraph */
    spawn_showcase_gimmick(g, GIMMICK_SPIRIT_PILLAR, GIMMICK_ELEM_FIRE, 240.0f, 442.0f, 16.0f,
                           4.2f);
    spawn_showcase_gimmick(g, GIMMICK_SPIRIT_PILLAR, GIMMICK_ELEM_STORM, 326.0f, 442.0f, 16.0f,
                           4.8f);
    spawn_showcase_gimmick(g, GIMMICK_LIGHTNING_STRIKE, GIMMICK_ELEM_NONE, 424.0f, 442.0f, 32.0f,
                           0.6f);

    g->run.boss_id = (EntityId){ECS_INVALID_INDEX, 0};
    fluid_reset(&g->fluid);

    /* All 5 P1 Projectile kinds */
    static const ProjKind kinds[5] = {
        PROJ_ARROW, PROJ_FIREBALL, PROJ_STORM_ORB, PROJ_WIND_SICKLE, PROJ_BOUNCE_ORB,
    };
    for (int k = 0; k < 5; ++k) {
        const float px = 600.0f + (float)k * 124.0f;
        const float py = 356.0f + (float)(k % 2) * 58.0f;
        const EntityId pid = game_debug_spawn_projectile(g, kinds[k], px, py, 220.0f, 0.0f);
        ProjComp *pr = game_proj_mut(g, pid);
        if (pr != NULL) {
            pr->deflected = (k >= 1);
            pr->burning = (k == 1);
            pr->electrified = (k == 2 || k == 4);
            pr->trail_n = 10;
            for (int t = 0; t < 10; ++t) {
                pr->trail[t] =
                    (GameVec2){px - (float)(9 - t) * 7.5f, py + (float)((t % 2) * 2 - 1) * 1.5f};
            }
        }
    }

    game_debug_spawn_orb(g, 204.0f, 342.0f);
    game_debug_spawn_orb(g, 246.0f, 342.0f);
    game_debug_spawn_orb(g, 288.0f, 342.0f);

    if (heavy_ink_60pct) {
        /* Page 2: 60% dense ink canvas + all 4 elemental reactions snapshot */
        float *ink = fluid_channel_mut(&g->fluid, 0);
        if (ink != NULL) {
            for (int y = 9; y < 74; ++y) {
                for (int x = 8; x < 148; ++x) {
                    ink[y * FLUID_W + x] = 1.20f;
                }
            }
        }
        paint_fluid_blob(&g->fluid, 1, 48.0f, 64.0f, 20.0f, 9.0f, 1.00f);
        paint_fluid_blob(&g->fluid, 2, 114.0f, 64.0f, 20.0f, 9.0f, 1.00f);

        if (r != NULL) {
            g->event_count = 0;
            game_push_event(g, (GameEvent){.type = EV_IGNITE, .x = 340.0f, .y = 510.0f});
            game_push_event(
                g, (GameEvent){
                       .type = EV_STORM_ZAP, .x = 540.0f, .y = 490.0f, .x2 = 660.0f, .y2 = 540.0f});
            game_push_event(
                g, (GameEvent){.type = EV_THUNDERBURST, .x = 760.0f, .y = 510.0f, .x2 = 76.0f});
            game_push_event(
                g, (GameEvent){.type = EV_CATACLYSM, .x = 1040.0f, .y = 510.0f, .x2 = 96.0f});
            render_consume_events(r, g);
            render_update_vfx(r, g, 0.14f);
            g->event_count = 0;
        }
    } else {
        paint_fluid_blob(&g->fluid, 0, 26.0f, 64.0f, 18.0f, 5.0f, 1.15f);
        paint_fluid_blob(&g->fluid, 1, 72.0f, 64.0f, 18.0f, 5.0f, 1.05f);
        paint_fluid_blob(&g->fluid, 2, 118.0f, 64.0f, 18.0f, 5.0f, 1.05f);
    }
    fluid_sanitize_channels(&g->fluid);
}

static void populate_reward_page(GameState *g, int subpage)
{
    populate_entities_and_projectiles(g, NULL, false);
    g->phase = RUN_PHASE_REWARD;
    g->run.rerolls_left = 2;
    g->run.reward.serial = (uint32_t)(subpage + 1);
    g->run.upgrades[0] = (OwnedUpgrade){.id = UPG_CHARGED_SEAL, .level = 1};
    g->run.upgrades[1] = (OwnedUpgrade){.id = UPG_GREAT_VORTEX, .level = 2};
    g->run.upgrade_count = 2;

    if (subpage == 0) {
        g->run.reward.kind = REWARD_KIND_UPGRADE;
        g->run.reward.choice_count = 3;
        g->run.reward.choice_ids[0] = UPG_LINGERING_EMBERS;
        g->run.reward.choice_next_level[0] = 1;
        g->run.reward.choice_ids[1] = UPG_STORM_BRUSH;
        g->run.reward.choice_next_level[1] = 2;
        g->run.reward.choice_ids[2] = UPG_DOUBLE_GALE;
        g->run.reward.choice_next_level[2] = 3;
        g->run.reward.repair_available = true;
        g->run.reward.repair_amount = 1;
        g->run.reward.crystal_reward = 0;
    } else if (subpage == 1) {
        g->run.reward.kind = REWARD_KIND_RELIC;
        g->run.reward.choice_count = 3;
        g->run.reward.choice_ids[0] = RELIC_VERMILION_INKSTONE;
        g->run.reward.choice_ids[1] = RELIC_THUNDER_DRUM;
        g->run.reward.choice_ids[2] = RELIC_PACT_OF_FIRE;
        g->run.reward.repair_available = false;
        g->run.reward.repair_amount = 0;
        g->run.reward.crystal_reward = 0;
    } else if (subpage == 2) {
        g->run.reward.kind = REWARD_KIND_RELIC;
        g->run.reward.choice_count = 3;
        g->run.reward.choice_ids[0] = RELIC_SEAL_OF_TRINITY;
        g->run.reward.choice_ids[1] = RELIC_INKSTORM_SECRET;
        g->run.reward.choice_ids[2] = RELIC_NIGHT_PARADE_SCROLL;
        g->run.reward.repair_available = false;
        g->run.reward.repair_amount = 0;
        g->run.reward.crystal_reward = 0;
    } else if (subpage == 3) {
        g->run.reward.kind = REWARD_KIND_RELIC;
        g->run.reward.choice_count = 3;
        g->run.reward.choice_ids[0] = RELIC_THUNDERFIRE_GEM;
        g->run.reward.choice_ids[1] = RELIC_ONI_TILE;
        g->run.reward.choice_ids[2] = RELIC_ENDLESS_BRUSH;
        g->run.reward.repair_available = false;
        g->run.reward.repair_amount = 0;
        g->run.reward.crystal_reward = 0;
    } else {
        g->run.reward.kind = REWARD_KIND_FALLBACK;
        g->run.reward.choice_count = 0;
        g->run.reward.repair_available = true;
        g->run.reward.repair_amount = 3;
        g->run.reward.crystal_reward = 30;
    }
}

static void populate_lobby_or_brush_page(GameState *g, UiView *ui, int subpage)
{
    const RunConfig cfg = meta_run_config_preset(BRUSH_STYLE_HEAVY, UINT64_C(2026));
    game_init(g, &cfg);
    fluid_reset(&g->fluid);
    paint_fluid_blob(&g->fluid, 0, 108.0f, 44.0f, 26.0f, 18.0f, 0.95f);
    paint_fluid_blob(&g->fluid, 0, 128.0f, 56.0f, 20.0f, 14.0f, 0.85f);
    fluid_sanitize_channels(&g->fluid);
    populate_demo_meta(&ui->meta_view);
    ui->showcase_subpage = subpage % 2;
    ui->lobby_cursor = 0;
    ui->selected_brush = BRUSH_STYLE_EMBER;
}

static void populate_shop_pause_or_settings_page(GameState *g, UiView *ui, int subpage)
{
    populate_entities_and_projectiles(g, NULL, false);
    populate_demo_meta(&ui->meta_view);
    ui->showcase_subpage = subpage % 3;
    ui->shop_cursor = 0;
    g->run.upgrades[0] = (OwnedUpgrade){.id = UPG_EMBER_TIP, .level = 2};
    g->run.upgrades[1] = (OwnedUpgrade){.id = UPG_FLAME_GALE, .level = 3};
    g->run.upgrades[2] = (OwnedUpgrade){.id = UPG_CHAIN_LIGHTNING, .level = 2};
    g->run.upgrades[3] = (OwnedUpgrade){.id = UPG_DOUBLE_GALE, .level = 3};
    g->run.upgrade_count = 4;
    g->run.relics[0] = RELIC_VERMILION_INKSTONE;
    g->run.relics[1] = RELIC_SEAL_OF_TRINITY;
    g->run.relic_count = 2;
    ui->paused = (ui->showcase_subpage == 1);
    ui->pause_cursor = 0;
    ui->pause_confirm = PAUSE_CONFIRM_NONE;
    ui->settings_cursor = 6;
    ui->settings_notice_kind = ui->showcase_subpage == 2 ? 1 : 0;
    ui->settings_notice_t = ui->showcase_subpage == 2 ? 1.5f : 0.0f;
}

static void populate_result_page(GameState *g, UiView *ui, int subpage)
{
    const RunConfig cfg = meta_run_config_preset(BRUSH_STYLE_HEAVY, UINT64_C(0x1a2b3c4d5e6f7788));
    game_init(g, &cfg);
    ui->showcase_subpage = subpage % 2;
    g->run.upgrades[0] = (OwnedUpgrade){.id = UPG_EMBER_TIP, .level = 3};
    g->run.upgrades[1] = (OwnedUpgrade){.id = UPG_WILDFIRE, .level = 2};
    g->run.upgrades[2] = (OwnedUpgrade){.id = UPG_FLAME_GALE, .level = 2};
    g->run.upgrades[3] = (OwnedUpgrade){.id = UPG_CHAIN_LIGHTNING, .level = 3};
    g->run.upgrades[4] = (OwnedUpgrade){.id = UPG_STORM_BRUSH, .level = 2};
    g->run.upgrades[5] = (OwnedUpgrade){.id = UPG_DOUBLE_GALE, .level = 3};
    g->run.upgrades[6] = (OwnedUpgrade){.id = UPG_LASTING_SURGE, .level = 2};
    g->run.upgrades[7] = (OwnedUpgrade){.id = UPG_INKSTONE_BLESSING, .level = 2};
    g->run.upgrade_count = 8;

    if (ui->showcase_subpage == 0) {
        /* Main campaign Victory: deferred settlement preview + 3 buttons ("Enter Endless") */
        g->phase = RUN_PHASE_VICTORY;
        g->run.act_index = 2;
        g->run.wave_index = GAME_BOSS_WAVE_INDEX;
        g->run.acts_cleared = 3;
        g->run.endless_loop = 0;
        g->run.run_settled = false;
        g->run.score = 64820;
        g->run.kills = 118;
        g->run.max_chain = 14;
        g->run.ignitions = 15;
        g->run.storm_zaps = 24;
        g->run.thunderbursts = 6;
        g->run.cataclysms = 3;
        g->run.relics[0] = RELIC_VERMILION_INKSTONE;
        g->run.relics[1] = RELIC_SEAL_OF_TRINITY;
        g->run.relic_count = 2;
        const MetaRunResult res = game_run_result(g);
        ui->settled = false;
        ui->settle_preview = true;
        ui->settle = meta_calc_crystals(&res);
        ui->crystals_after = 240 + ui->settle.total_crystals;
    } else {
        /* Endless run Defeat: Loop 1 Act 5 + Endless & Night Parade Scroll bonus rows */
        g->phase = RUN_PHASE_GAME_OVER;
        g->run.act_index = 4;
        g->run.wave_index = 1;
        g->run.acts_cleared = 3;
        g->run.endless_loop = 1;
        g->run.endless_acts_cleared = 2;
        g->run.bonus_crystals = 40;
        g->run.run_settled = true;
        g->run.score = 128450;
        g->run.kills = 246;
        g->run.max_chain = 18;
        g->run.ignitions = 31;
        g->run.storm_zaps = 48;
        g->run.thunderbursts = 14;
        g->run.cataclysms = 7;
        g->run.relics[0] = RELIC_VERMILION_INKSTONE;
        g->run.relics[1] = RELIC_SEAL_OF_TRINITY;
        g->run.relics[2] = RELIC_NIGHT_PARADE_SCROLL;
        g->run.relic_count = 3;
        const MetaRunResult res = game_run_result(g);
        ui->settled = true;
        ui->settle_preview = false;
        ui->settle = meta_calc_crystals(&res);
        ui->crystals_after = 240 + ui->settle.total_crystals;
    }
}

static void setup_page(GameState *g, RenderState *r, UiView *ui, int page, int subpage)
{
    ui->showcase_page = page;
    ui->showcase_subpage = subpage;
    ui->settled = false;
    ui->settle_preview = false;
    ui->paused = false;
    ui->cursor_scale = 100;
    ui->settings_notice_t = 0.0f;
    ui->settings_notice_kind = 0;
    ui->hover_choice = UI_HOVER_NONE;
    render_reset_vfx(r, UINT64_C(2026));

    if (page == 1) {
        ui->showcase_subpage = subpage % 2;
        if (ui->showcase_subpage == 0) {
            populate_entities_and_projectiles(g, r, false);
        } else {
            populate_p2_entities_showcase(g, r, ui);
        }
    } else if (page == 2) {
        ui->showcase_subpage = 0;
        populate_entities_and_projectiles(g, r, true);
    } else if (page == 3) {
        ui->showcase_subpage = subpage % 5;
        populate_reward_page(g, ui->showcase_subpage);
        r->reward_anim_t = 1.0f;
    } else if (page == 4) {
        populate_lobby_or_brush_page(g, ui, subpage);
    } else if (page == 5) {
        populate_shop_pause_or_settings_page(g, ui, subpage);
    } else {
        populate_result_page(g, ui, subpage);
        r->victory_scroll_t = 1.0f;
    }
}

static void save_showcase_screenshot(int page, int subpage)
{
    if (mkdir("build", 0755) != 0 && errno != EEXIST) {
        TraceLog(LOG_WARNING, "INKSTORM: failed to create build/ for screenshot");
        return;
    }
    if (mkdir("build/snaps", 0755) != 0 && errno != EEXIST) {
        TraceLog(LOG_WARNING, "INKSTORM: failed to create build/snaps/ for screenshot");
        return;
    }
    char path[128];
    if (subpage == 0) {
        snprintf(path, sizeof path, "build/snaps/showcase_p%d.png", page);
    } else {
        snprintf(path, sizeof path, "build/snaps/showcase_p%d_s%d.png", page, subpage);
    }
    Image img = LoadImageFromScreen();
    if (img.data != NULL) {
        if (ExportImage(img, path)) {
            TraceLog(LOG_INFO, "INKSTORM: saved showcase screenshot %s", path);
        } else {
            TraceLog(LOG_WARNING, "INKSTORM: failed to save screenshot %s", path);
        }
        UnloadImage(img);
    }
}

int app_run_showcase(GameState *game, RenderState *render, const CliOptions *opts,
                     uint32_t fault_mask)
{
    if (opts->smoke_frames > 0) {
        SetConfigFlags(FLAG_WINDOW_HIDDEN);
    }
    InitWindow(INK_SCREEN_W, INK_SCREEN_H, "INKSTORM - Showcase");
    if (!IsWindowReady()) {
        return EXIT_FAILURE;
    }
    SetTargetFPS(INK_TICK_HZ);

    char font_path[PATH_MAX];
    char title_font_path[PATH_MAX];
    const bool has_font =
        app_resolve_asset_path("fonts/ui_mincho.ttf", font_path, sizeof font_path);
    const bool has_title_font =
        app_resolve_asset_path("fonts/title_brush.ttf", title_font_path, sizeof title_font_path);
    const RenderOptions render_opts = {
        .use_post = opts->use_post,
        .fault_mask = fault_mask,
        .font_path = has_font ? font_path : NULL,
        .title_font_path = has_title_font ? title_font_path : NULL,
    };
    render_init(render, &render_opts);

    UiView ui = {
        .scene = UI_SCENE_SHOWCASE,
        .hover_choice = UI_HOVER_NONE,
        .shake_scale = 100,
        .cursor_scale = 100,
        .showcase_page = 1,
        .showcase_subpage = 0,
    };

    int page = 1;
    int subpage = 0;
    setup_page(game, render, &ui, page, subpage);

    int frame_idx = 0;
    int remaining = opts->smoke_frames;
    while (remaining > 0 || !WindowShouldClose()) {
        int next_page = page;
        int next_sub = subpage;

        if (remaining > 0) {
            /* Automatically sweep all 6 pages & key P2 subpages during --smoke-frames 30 */
            static const int k_sweep_page[10] = {1, 1, 2, 3, 3, 3, 4, 5, 6, 6};
            static const int k_sweep_sub[10] = {0, 1, 0, 0, 2, 4, 0, 2, 0, 1};
            const int slot = (frame_idx / 3) % 10;
            next_page = k_sweep_page[slot];
            next_sub = k_sweep_sub[slot];
        } else {
            if (IsKeyPressed(KEY_ONE)) {
                next_page = 1;
                next_sub = 0;
            } else if (IsKeyPressed(KEY_TWO)) {
                next_page = 2;
                next_sub = 0;
            } else if (IsKeyPressed(KEY_THREE)) {
                next_page = 3;
                next_sub = 0;
            } else if (IsKeyPressed(KEY_FOUR)) {
                next_page = 4;
                next_sub = 0;
            } else if (IsKeyPressed(KEY_FIVE)) {
                next_page = 5;
                next_sub = 0;
            } else if (IsKeyPressed(KEY_SIX)) {
                next_page = 6;
                next_sub = 0;
            } else if (IsKeyPressed(KEY_RIGHT)) {
                next_page = page < 6 ? page + 1 : 1;
                next_sub = 0;
            } else if (IsKeyPressed(KEY_LEFT)) {
                next_page = page > 1 ? page - 1 : 6;
                next_sub = 0;
            }
            if (IsKeyPressed(KEY_TAB)) {
                if (page == 1 || page == 4 || page == 6) {
                    next_sub = (subpage + 1) % 2;
                } else if (page == 3) {
                    next_sub = (subpage + 1) % 5;
                } else if (page == 5) {
                    next_sub = (subpage + 1) % 3;
                }
            }
            if (IsKeyPressed(KEY_F1)) {
                ui.xray = !ui.xray;
            }
        }

        if (next_page != page || next_sub != subpage) {
            page = next_page;
            subpage = next_sub;
            setup_page(game, render, &ui, page, subpage);
        }

        game->ticks++;
        render->vfx_time += INK_FIXED_DT;
        render_draw(render, game, &ui);
        frame_idx++;

        if (remaining == 0 && IsKeyPressed(KEY_F12)) {
            save_showcase_screenshot(page, subpage);
        }
        if (remaining > 0 && --remaining == 0) {
            break;
        }
    }

    render_close(render);
    CloseWindow();
    return EXIT_SUCCESS;
}
