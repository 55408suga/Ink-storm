#include "showcase.h"

#include "core.h"
#include "paths.h"
#include "raylib.h"

#include <errno.h>
#include <limits.h>
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

static void populate_entities_and_projectiles(GameState *g, bool heavy_ink_60pct)
{
    const RunConfig cfg = meta_run_config_preset(BRUSH_STYLE_HEAVY, UINT64_C(2026));
    game_init(g, &cfg);
    g->phase = RUN_PHASE_PLAYING;
    g->phase_t = 5.0f;
    g->run.act_index = 0;
    g->run.wave_index = 0;
    g->run.wave_kills = 4;
    g->run.wave_quota = 8;
    g->run.score = 12850;
    g->run.chain = 4;
    g->run.chain_t = 2.0f;
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
    g->shrines[0].hp = 6;
    g->shrines[0].max_hp = 8;

    /* P0 enemies (3 regular types x 3 states: idle / windup / hit flash) */
    const EnemyDefId regulars[3] = {ENEMY_ARCHER, ENEMY_LANTERN_FIRE, ENEMY_UMBRELLA};
    const float row_y[3] = {138.0f, 232.0f, 330.0f};
    const float col_x[3] = {244.0f, 342.0f, 440.0f};
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            const EntityId id = game_debug_spawn_enemy(g, regulars[r], col_x[c], row_y[r]);
            TransformComp *tr = game_transform_mut(g, id);
            EnemyComp *en = game_enemy_mut(g, id);
            HealthComp *hp = game_health_mut(g, id);
            StatusComp *st = game_status_mut(g, id);
            if (tr != NULL) {
                tr->facing = 0.25f;
            }
            if (c == 1 && en != NULL) {
                en->windup = 0.80f;
            }
            if (c == 2 && hp != NULL) {
                hp->flash_t = 0.22f;
            }
            if (r == 0 && c == 1 && st != NULL) {
                st->ink_mark_t = 2.0f;
            } else if (r == 1 && c == 2 && st != NULL) {
                st->burn_acc = 0.3f;
            } else if (r == 2 && c == 1 && st != NULL) {
                st->charge_t = 1.5f;
            }
        }
    }

    /* O-Tengu boss x 3 states (idle / telegraph / hit flash) */
    const float boss_x[3] = {635.0f, 845.0f, 1055.0f};
    for (int c = 0; c < 3; ++c) {
        const EntityId bid = game_debug_spawn_enemy(g, ENEMY_BOSS_TENGU, boss_x[c], 178.0f);
        BossComp *bc = game_boss_mut(g, bid);
        HealthComp *hp = game_health_mut(g, bid);
        if (c == 1 && bc != NULL) {
            bc->telegraph_pattern = 1;
            bc->telegraph = 0.78f;
            bc->gale_dx = 0.0f;
            bc->gale_dy = 1.0f;
        }
        if (c == 2 && hp != NULL) {
            hp->flash_t = 0.22f;
        }
    }
    /* Clear boss_id on page 1/2 so normal wave HUD is also visible */
    g->run.boss_id = (EntityId){ECS_INVALID_INDEX, 0};

    /* Projectiles: 3 kinds x 4 states (normal, deflected, burning, electrified) */
    const ProjKind kinds[3] = {PROJ_ARROW, PROJ_FIREBALL, PROJ_STORM_ORB};
    const float py[3] = {322.0f, 382.0f, 442.0f};
    const float px[4] = {610.0f, 755.0f, 900.0f, 1045.0f};
    for (int k = 0; k < 3; ++k) {
        for (int s = 0; s < 4; ++s) {
            const EntityId pid =
                game_debug_spawn_projectile(g, kinds[k], px[s], py[k], 220.0f, 0.0f);
            ProjComp *pr = game_proj_mut(g, pid);
            if (pr != NULL) {
                pr->deflected = (s >= 1);
                pr->burning = (s == 2);
                pr->electrified = (s == 3);
                pr->trail_n = 10;
                for (int t = 0; t < 10; ++t) {
                    pr->trail[t] = (GameVec2){px[s] - (float)(9 - t) * 7.5f,
                                              py[k] + (float)((t % 2) * 2 - 1) * 1.5f};
                }
            }
        }
    }

    /* Ink orbs */
    game_debug_spawn_orb(g, 240.0f, 424.0f);
    game_debug_spawn_orb(g, 286.0f, 424.0f);
    game_debug_spawn_orb(g, 332.0f, 424.0f);

    if (heavy_ink_60pct) {
        /* Page 2: cover ~60% of the canvas with dense ink (1.2) to verify ink_knockout */
        float *ink = fluid_channel_mut(&g->fluid, 0);
        if (ink != NULL) {
            for (int y = 9; y < 74; ++y) {
                for (int x = 8; x < 148; ++x) {
                    ink[y * FLUID_W + x] = 1.20f;
                }
            }
        }
        paint_fluid_blob(&g->fluid, 1, 86.0f, 58.0f, 22.0f, 9.0f, 0.95f);
        paint_fluid_blob(&g->fluid, 2, 122.0f, 58.0f, 22.0f, 9.0f, 0.95f);
    } else {
        /* Page 1: organic fluid wash strip showing ink, fire, storm, and overlaps */
        paint_fluid_blob(&g->fluid, 0, 26.0f, 62.0f, 20.0f, 6.0f, 1.15f);
        paint_fluid_blob(&g->fluid, 0, 62.0f, 62.0f, 18.0f, 6.0f, 1.05f);
        paint_fluid_blob(&g->fluid, 1, 78.0f, 62.0f, 20.0f, 6.0f, 1.05f);
        paint_fluid_blob(&g->fluid, 0, 112.0f, 62.0f, 18.0f, 6.0f, 1.05f);
        paint_fluid_blob(&g->fluid, 2, 128.0f, 62.0f, 20.0f, 6.0f, 1.05f);
    }
    fluid_sanitize_channels(&g->fluid);
}

static void populate_reward_page(GameState *g, int variant)
{
    populate_entities_and_projectiles(g, false);
    g->phase = RUN_PHASE_REWARD;
    g->run.rerolls_left = 2;
    g->run.reward.serial = (uint32_t)(variant + 1);
    g->run.upgrades[0] = (OwnedUpgrade){.id = UPG_CHARGED_SEAL, .level = 1};
    g->run.upgrades[1] = (OwnedUpgrade){.id = UPG_GREAT_VORTEX, .level = 2};
    g->run.upgrade_count = 2;

    if (variant == 0) {
        g->run.reward.kind = REWARD_KIND_UPGRADE;
        g->run.reward.choice_count = 3;
        g->run.reward.choice_ids[0] = UPG_EMBER_TIP;
        g->run.reward.choice_next_level[0] = 1;
        g->run.reward.choice_ids[1] = UPG_CHARGED_SEAL;
        g->run.reward.choice_next_level[1] = 2;
        g->run.reward.choice_ids[2] = UPG_GREAT_VORTEX;
        g->run.reward.choice_next_level[2] = 3;
        g->run.reward.repair_available = true;
    } else if (variant == 1) {
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

static void populate_result_page(GameState *g, UiView *ui)
{
    const RunConfig cfg = meta_run_config_preset(BRUSH_STYLE_HEAVY, UINT64_C(0x1a2b3c4d5e6f7788));
    game_init(g, &cfg);
    g->phase = RUN_PHASE_VICTORY;
    g->run.act_index = 0;
    g->run.wave_index = GAME_BOSS_WAVE_INDEX;
    g->run.acts_cleared = 1;
    g->run.score = 24680;
    g->run.kills = 42;
    g->run.max_chain = 9;
    g->run.ignitions = 7;
    g->run.storm_zaps = 11;
    g->run.thunderbursts = 2;
    g->run.cataclysms = 1;
    g->run.upgrades[0] = (OwnedUpgrade){.id = UPG_EMBER_TIP, .level = 2};
    g->run.upgrades[1] = (OwnedUpgrade){.id = UPG_WILDFIRE, .level = 1};
    g->run.upgrades[2] = (OwnedUpgrade){.id = UPG_CHAIN_LIGHTNING, .level = 2};
    g->run.upgrades[3] = (OwnedUpgrade){.id = UPG_LASTING_SURGE, .level = 1};
    g->run.upgrade_count = 4;

    const MetaRunResult res = game_run_result(g);
    ui->settled = true;
    ui->settle = meta_calc_crystals(&res);
    ui->crystals_after = 120 + ui->settle.total_crystals;
}

static void setup_page(GameState *g, RenderState *r, UiView *ui, int page, int reward_variant)
{
    ui->showcase_page = page;
    ui->settled = false;
    ui->hover_choice = UI_HOVER_NONE;
    render_reset_vfx(r, UINT64_C(2026));

    if (page == 1) {
        populate_entities_and_projectiles(g, false);
    } else if (page == 2) {
        populate_entities_and_projectiles(g, true);
    } else if (page == 3) {
        populate_reward_page(g, reward_variant);
        r->reward_anim_t = 1.0f;
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
    const bool has_font =
        app_resolve_asset_path("fonts/ui_mincho.ttf", font_path, sizeof font_path);
    const RenderOptions render_opts = {
        .use_post = opts->use_post,
        .fault_mask = fault_mask,
        .font_path = has_font ? font_path : NULL,
    };
    render_init(render, &render_opts);

    UiView ui = {
        .scene = UI_SCENE_SHOWCASE,
        .hover_choice = UI_HOVER_NONE,
        .shake_scale = 100,
        .showcase_page = 1,
    };

    int page = 1;
    int reward_variant = 0;
    setup_page(game, render, &ui, page, reward_variant);

    int remaining = opts->smoke_frames;
    while (remaining > 0 || !WindowShouldClose()) {
        int next_page = page;
        int next_var = reward_variant;

        if (remaining == 0) {
            if (IsKeyPressed(KEY_ONE)) {
                next_page = 1;
            } else if (IsKeyPressed(KEY_TWO)) {
                next_page = 2;
            } else if (IsKeyPressed(KEY_THREE)) {
                next_page = 3;
            } else if (IsKeyPressed(KEY_FOUR)) {
                next_page = 4;
            } else if (IsKeyPressed(KEY_RIGHT)) {
                next_page = page < 4 ? page + 1 : 1;
            } else if (IsKeyPressed(KEY_LEFT)) {
                next_page = page > 1 ? page - 1 : 4;
            }
            if (page == 3 && IsKeyPressed(KEY_TAB)) {
                next_var = (reward_variant + 1) % 3;
            }
            if (IsKeyPressed(KEY_F1)) {
                ui.xray = !ui.xray;
            }
        }

        if (next_page != page || next_var != reward_variant) {
            page = next_page;
            reward_variant = next_var;
            setup_page(game, render, &ui, page, reward_variant);
        }

        /* Showcase exception: increment ticks directly for 6-tick boil inspection */
        game->ticks++;
        render_draw(render, game, &ui);

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
