#include "ink_prims.h"
#include "render_internal.h"

#include "core.h"

#include <stdio.h>
#include <string.h>

void render_bokuan_layout(BokuanLayout *out)
{
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof *out);

    /* Left hanging scroll: 4 permanent upgrades */
    const float lx = 72.0f;
    const float lw = 510.0f;
    const float lh = 94.0f;
    const float lgap = 14.0f;
    const float ly0 = 166.0f;
    for (int i = 0; i < META_UPG_COUNT; ++i) {
        out->upgrades[i] = (Rectangle){lx, ly0 + (float)i * (lh + lgap), lw, lh};
    }

    /* Right hanging scroll: 6 relic unlock slots (2 columns x 3 rows) */
    const float rx0 = 626.0f;
    const float rw = 282.0f;
    const float rh = 128.0f;
    const float rgap_x = 18.0f;
    const float rgap_y = 14.0f;
    const float ry0 = 166.0f;
    for (int s = 0; s < META_RELIC_UNLOCK_COUNT; ++s) {
        const int col = s % 2;
        const int row = s / 2;
        out->relics[s] =
            (Rectangle){rx0 + (float)col * (rw + rgap_x), ry0 + (float)row * (rh + rgap_y), rw, rh};
    }

    out->back_rect = (Rectangle){((float)INK_SCREEN_W - 260.0f) * 0.5f, 622.0f, 260.0f, 54.0f};
}

void render_draw_bokuan_ui(RenderState *r, const GameState *g, const UiView *ui)
{
    if (r == NULL || g == NULL || ui == NULL) {
        return;
    }

    const Vector2 full[4] = {{0.0f, 0.0f},
                             {(float)INK_SCREEN_W, 0.0f},
                             {(float)INK_SCREEN_W, (float)INK_SCREEN_H},
                             {0.0f, (float)INK_SCREEN_H}};
    ink_fill_poly(r, full, 4, pal_color_alpha(PAL_PAPER_ACT1, 0.72f));

    const MetaState *meta = &ui->meta_view;
    const float cx = (float)INK_SCREEN_W * 0.5f;

    /* Top header & Ink Crystals purse */
    render_draw_text_centered(r, text_get(TEXT_BOKUAN_TITLE, r->use_ja), (Vector2){cx, 60.0f},
                              46.0f, PAL_SUMI_SCORCH, 0.98f, true);
    render_draw_text_centered(r, text_get(TEXT_BOKUAN_HINT, r->use_ja), (Vector2){cx, 102.0f},
                              18.0f, PAL_SUMI_HEAVY, 0.90f, false);

    const Rectangle purse = {(float)INK_SCREEN_W - 278.0f, 38.0f, 218.0f, 48.0f};
    ink_fill_panel(r, purse, PAL_GOFUN, 0.95f, PAL_KIN, 0.92f, 1010u);
    render_draw_text_id(r, TEXT_LOBBY_CRYSTALS, (Vector2){purse.x + 14.0f, purse.y + 14.0f}, 18.0f,
                        PAL_SUMI_HEAVY, 0.95f, false);
    char cbuf[32];
    snprintf(cbuf, sizeof cbuf, "%d", meta->ink_crystals);
    render_draw_number_fixed(r, cbuf, (Vector2){purse.x + 112.0f, purse.y + 13.0f}, 20.0f, PAL_KIN,
                             0.98f);

    /* Scroll backdrop panels */
    ink_fill_panel(r, (Rectangle){56.0f, 126.0f, 542.0f, 476.0f}, PAL_PAPER_ACT2, 0.78f,
                   PAL_SUMI_DARK, 0.85f, 1011u);
    ink_fill_panel(r, (Rectangle){610.0f, 126.0f, 614.0f, 476.0f}, PAL_PAPER_ACT2, 0.78f,
                   PAL_SUMI_DARK, 0.85f, 1012u);

    render_draw_text_id(r, TEXT_BOKUAN_UPGRADES_HDR, (Vector2){76.0f, 134.0f}, 22.0f,
                        PAL_SUMI_SCORCH, 0.96f, false);
    render_draw_text_id(r, TEXT_BOKUAN_RELICS_HDR, (Vector2){630.0f, 134.0f}, 22.0f,
                        PAL_SUMI_SCORCH, 0.96f, false);

    BokuanLayout layout;
    render_bokuan_layout(&layout);
    const int32_t total_items = META_UPG_COUNT + META_RELIC_UNLOCK_COUNT;
    const int32_t active = (ui->hover_choice >= 0 && ui->hover_choice <= total_items)
                               ? ui->hover_choice
                               : ui->shop_cursor;

    /* 4 Permanent Upgrades */
    for (int i = 0; i < META_UPG_COUNT; ++i) {
        const MetaUpgradeKind kind = (MetaUpgradeKind)i;
        const Rectangle rect = layout.upgrades[i];
        const bool sel = (active == i);
        const uint8_t cur_lv = meta->upgrade_lv[kind];
        const uint8_t max_lv = meta_upgrade_max_level(kind);
        const int32_t cost = meta_upgrade_next_cost(meta, kind);
        const bool maxed = (cost < 0);
        const bool afford = !maxed && meta->ink_crystals >= cost;

        const PaletteId border = sel ? PAL_SHU : (maxed ? PAL_KIN : PAL_SUMI_DARK);
        ink_fill_panel(r, rect, PAL_GOFUN, maxed ? 0.88f : 0.96f, border, 0.92f,
                       1020u + (uint32_t)i);

        render_draw_text_wrapped(
            r, text_get(text_shop_upgrade_name(kind), r->use_ja),
            (Rectangle){rect.x + 18.0f, rect.y + 8.0f, rect.width - 170.0f, 30.0f}, 22.0f, 16.0f,
            PAL_SUMI_SCORCH, 0.98f, false, false);
        render_draw_text_wrapped(
            r, text_get(text_shop_upgrade_desc(kind), r->use_ja),
            (Rectangle){rect.x + 18.0f, rect.y + 42.0f, rect.width - 170.0f, 44.0f}, 18.0f, 13.0f,
            PAL_SUMI_HEAVY, 0.92f, false, false);

        char lv_buf[32];
        snprintf(lv_buf, sizeof lv_buf, "%s %u/%u", text_get(TEXT_REWARD_LV, r->use_ja),
                 (unsigned)cur_lv, (unsigned)max_lv);
        render_draw_text(r, lv_buf, (Vector2){rect.x + rect.width - 134.0f, rect.y + 14.0f}, 18.0f,
                         PAL_SUMI_DARK, 0.95f, false);

        if (maxed) {
            render_draw_text_id(r, TEXT_BOKUAN_MAXED,
                                (Vector2){rect.x + rect.width - 118.0f, rect.y + 52.0f}, 19.0f,
                                PAL_KIN, 0.98f, false);
        } else {
            char pbuf[48];
            snprintf(pbuf, sizeof pbuf, "%d %s", cost, text_get(TEXT_BOKUAN_COST_UNIT, r->use_ja));
            render_draw_text(r, pbuf, (Vector2){rect.x + rect.width - 134.0f, rect.y + 52.0f},
                             19.0f, afford ? PAL_SHU : PAL_SUMI_LIGHT, 0.96f, false);
        }
    }

    /* 6 Relic Unlocks */
    for (uint32_t s = 0; s < (uint32_t)META_RELIC_UNLOCK_COUNT; ++s) {
        const Rectangle rect = layout.relics[s];
        const int32_t idx = META_UPG_COUNT + (int32_t)s;
        const bool sel = (active == idx);
        const bool unlocked = meta_is_relic_unlocked(meta, s);
        const int32_t cost = meta_relic_unlock_cost(s);
        const bool afford = !unlocked && cost >= 0 && meta->ink_crystals >= cost;

        const PaletteId border = sel ? PAL_SHU : (unlocked ? PAL_KIN : PAL_SUMI_DARK);
        ink_fill_panel(r, rect, PAL_GOFUN, unlocked ? 0.88f : 0.96f, border, 0.92f, 1040u + s);

        render_draw_text_wrapped(
            r, text_get(text_shop_relic_name(s), r->use_ja),
            (Rectangle){rect.x + 14.0f, rect.y + 8.0f, rect.width - 28.0f, 28.0f}, 20.0f, 14.0f,
            PAL_SUMI_SCORCH, 0.98f, false, false);
        render_draw_text_wrapped(
            r, text_get(text_shop_relic_desc(s), r->use_ja),
            (Rectangle){rect.x + 14.0f, rect.y + 40.0f, rect.width - 28.0f, 46.0f}, 18.0f, 13.0f,
            PAL_SUMI_HEAVY, 0.90f, false, false);

        if (unlocked) {
            render_draw_text_id(r, TEXT_BOKUAN_UNLOCKED, (Vector2){rect.x + 14.0f, rect.y + 92.0f},
                                18.0f, PAL_KIN, 0.98f, false);
            if (sel) {
                seal_stamp(r, TEXT_SEAL_DONE,
                           (Vector2){rect.x + rect.width - 28.0f, rect.y + rect.height - 26.0f},
                           30.0f, SEAL_SHUBUN, 0.7f);
            }
        } else {
            char pbuf[48];
            snprintf(pbuf, sizeof pbuf, "%d %s", cost, text_get(TEXT_BOKUAN_COST_UNIT, r->use_ja));
            render_draw_text(r, pbuf, (Vector2){rect.x + 14.0f, rect.y + 92.0f}, 18.0f,
                             afford ? PAL_SHU : PAL_SUMI_LIGHT, 0.96f, false);
        }
    }

    const bool back_sel = (active == total_items);
    ink_fill_panel(r, layout.back_rect, PAL_GOFUN, 0.95f, back_sel ? PAL_SHU : PAL_SUMI_DARK, 0.92f,
                   1060u);
    render_draw_text_centered(r, text_get(TEXT_BOKUAN_BACK, r->use_ja),
                              (Vector2){layout.back_rect.x + layout.back_rect.width * 0.5f,
                                        layout.back_rect.y + layout.back_rect.height * 0.5f},
                              20.0f, back_sel ? PAL_SHU : PAL_SUMI_SCORCH, 0.96f, false);
}
