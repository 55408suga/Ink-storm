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
    m->best_act = 3;
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

    /* All 5 Projectile kinds */
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
        g->run.reward.choice_ids[0] = UPG_FLAME_GALE;
        g->run.reward.choice_next_level[0] = 1;
        g->run.reward.choice_ids[1] = UPG_THUNDERBURST_FUSE;
        g->run.reward.choice_next_level[1] = 2;
        g->run.reward.choice_ids[2] = UPG_BARRIER;
        g->run.reward.choice_next_level[2] = 3;
        g->run.reward.repair_available = true;
    } else if (subpage == 1) {
        g->run.reward.kind = REWARD_KIND_RELIC;
        g->run.reward.choice_count = 3;
        g->run.reward.choice_ids[0] = RELIC_VERMILION_INKSTONE;
        g->run.reward.choice_ids[1] = RELIC_THUNDER_DRUM;
        g->run.reward.choice_ids[2] = RELIC_PACT_OF_FIRE;
        g->run.reward.repair_available = false;
    } else if (subpage == 2) {
        g->run.reward.kind = REWARD_KIND_UPGRADE;
        g->run.reward.choice_count = 1;
        g->run.reward.choice_ids[0] = UPG_RICH_INK;
        g->run.reward.choice_next_level[0] = 1;
        g->run.reward.repair_available = false;
    } else {
        g->run.reward.kind = REWARD_KIND_FALLBACK;
        g->run.reward.choice_count = 0;
        g->run.reward.repair_available = true;
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

static void populate_shop_or_pause_page(GameState *g, UiView *ui, int subpage)
{
    populate_entities_and_projectiles(g, NULL, false);
    populate_demo_meta(&ui->meta_view);
    ui->showcase_subpage = subpage % 2;
    ui->shop_cursor = 0;
    g->run.upgrades[0] = (OwnedUpgrade){.id = UPG_EMBER_TIP, .level = 2};
    g->run.upgrades[1] = (OwnedUpgrade){.id = UPG_FLAME_GALE, .level = 3};
    g->run.upgrades[2] = (OwnedUpgrade){.id = UPG_CHAIN_LIGHTNING, .level = 2};
    g->run.upgrades[3] = (OwnedUpgrade){.id = UPG_MAELSTROM_BIND, .level = 1};
    g->run.upgrade_count = 4;
    g->run.relics[0] = RELIC_VERMILION_INKSTONE;
    g->run.relics[1] = RELIC_TENGU_FAN;
    g->run.relic_count = 2;
    ui->paused = (ui->showcase_subpage == 1);
    ui->pause_cursor = 0;
    ui->pause_confirm = PAUSE_CONFIRM_NONE;
}

static void populate_result_page(GameState *g, UiView *ui)
{
    const RunConfig cfg = meta_run_config_preset(BRUSH_STYLE_HEAVY, UINT64_C(0x1a2b3c4d5e6f7788));
    game_init(g, &cfg);
    g->phase = RUN_PHASE_VICTORY;
    g->run.act_index = 2;
    g->run.wave_index = GAME_BOSS_WAVE_INDEX;
    g->run.acts_cleared = 3;
    g->run.score = 64820;
    g->run.kills = 118;
    g->run.max_chain = 14;
    g->run.ignitions = 15;
    g->run.storm_zaps = 24;
    g->run.thunderbursts = 6;
    g->run.cataclysms = 3;
    g->run.upgrades[0] = (OwnedUpgrade){.id = UPG_EMBER_TIP, .level = 3};
    g->run.upgrades[1] = (OwnedUpgrade){.id = UPG_WILDFIRE, .level = 2};
    g->run.upgrades[2] = (OwnedUpgrade){.id = UPG_FLAME_GALE, .level = 2};
    g->run.upgrades[3] = (OwnedUpgrade){.id = UPG_CHAIN_LIGHTNING, .level = 3};
    g->run.upgrades[4] = (OwnedUpgrade){.id = UPG_THUNDERBURST_FUSE, .level = 2};
    g->run.upgrades[5] = (OwnedUpgrade){.id = UPG_GREAT_VORTEX, .level = 2};
    g->run.upgrades[6] = (OwnedUpgrade){.id = UPG_LASTING_SURGE, .level = 2};
    g->run.upgrades[7] = (OwnedUpgrade){.id = UPG_SHRINE_WARD, .level = 1};
    g->run.upgrade_count = 8;
    g->run.relics[0] = RELIC_VERMILION_INKSTONE;
    g->run.relics[1] = RELIC_THUNDER_DRUM;
    g->run.relic_count = 2;

    const MetaRunResult res = game_run_result(g);
    ui->settled = true;
    ui->settle = meta_calc_crystals(&res);
    ui->crystals_after = 240 + ui->settle.total_crystals;
}

static void setup_page(GameState *g, RenderState *r, UiView *ui, int page, int subpage)
{
    ui->showcase_page = page;
    ui->showcase_subpage = subpage;
    ui->settled = false;
    ui->paused = false;
    ui->hover_choice = UI_HOVER_NONE;
    render_reset_vfx(r, UINT64_C(2026));

    if (page == 1) {
        populate_entities_and_projectiles(g, r, false);
    } else if (page == 2) {
        populate_entities_and_projectiles(g, r, true);
    } else if (page == 3) {
        populate_reward_page(g, subpage % 4);
        r->reward_anim_t = 1.0f;
    } else if (page == 4) {
        populate_lobby_or_brush_page(g, ui, subpage);
    } else if (page == 5) {
        populate_shop_or_pause_page(g, ui, subpage);
    } else {
        populate_result_page(g, ui);
        r->victory_scroll_t = 1.0f;
    }
}

static void save_showcase_screenshot(int page)
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
    snprintf(path, sizeof path, "build/snaps/showcase_p%d.png", page);
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
            /* Automatically sweep all 6 pages during --showcase --smoke-frames 30 */
            next_page = ((frame_idx / 5) % 6) + 1;
            next_sub = (frame_idx % 5 >= 3 && (next_page == 3 || next_page == 4 || next_page == 5))
                           ? 1
                           : 0;
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
                if (page == 3) {
                    next_sub = (subpage + 1) % 4;
                } else if (page == 4 || page == 5) {
                    next_sub = (subpage + 1) % 2;
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
            save_showcase_screenshot(page);
        }
        if (remaining > 0 && --remaining == 0) {
            break;
        }
    }

    render_close(render);
    CloseWindow();
    return EXIT_SUCCESS;
}
