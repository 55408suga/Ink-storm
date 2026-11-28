#include "core.h"
#include "game.h"

#include <stddef.h>
#include <string.h>

static uint64_t hash_bool(uint64_t h, bool v)
{
    const uint8_t b = v ? 1u : 0u;
    return ink_hash_bytes(h, &b, 1);
}

static uint64_t hash_u8(uint64_t h, uint8_t v) { return ink_hash_bytes(h, &v, 1); }

static uint64_t hash_i32(uint64_t h, int32_t v) { return ink_hash_u32(h, (uint32_t)v); }

static uint64_t hash_core(const GameState *g)
{
    uint64_t h = INK_HASH_INIT;
    h = ink_hash_u64(h, g->ticks);
    h = ink_hash_u64(h, g->rng);
    h = ink_hash_u32(h, (uint32_t)g->phase);
    h = ink_hash_f32(h, g->phase_t);
    h = ink_hash_f32(h, g->hitstop_t);
    h = ink_hash_u64(h, g->config.seed);
    h = ink_hash_u32(h, (uint32_t)g->config.brush_style);
    for (int i = 0; i < META_UPG_COUNT; ++i) {
        h = hash_u8(h, g->config.meta_upgrade_lv[i]);
    }
    h = ink_hash_u64(h, g->config.unlocked_relic_mask);
    h = hash_i32(h, g->config.endless_loop);
    return h;
}

static uint64_t hash_ecs(const EcsWorld *w)
{
    uint64_t h = INK_HASH_INIT;
    for (uint32_t i = 0; i < ECS_MAX_ENTITIES; ++i) {
        h = ink_hash_u32(h, w->gen[i]);
    }
    const uint32_t hw = w->high_water <= ECS_MAX_ENTITIES ? w->high_water : ECS_MAX_ENTITIES;
    for (uint32_t i = 0; i < hw; ++i) {
        h = ink_hash_u64(h, w->comp_mask[i]);
        h = hash_u8(h, w->alive[i]);
        h = hash_u8(h, w->pending_destroy[i]);
    }
    const uint32_t fc = w->free_count <= ECS_MAX_ENTITIES ? w->free_count : ECS_MAX_ENTITIES;
    for (uint32_t i = 0; i < fc; ++i) {
        h = ink_hash_u32(h, w->free_list[i]);
    }
    h = ink_hash_u32(h, w->free_count);
    h = ink_hash_u32(h, w->alive_count);
    h = ink_hash_u32(h, w->high_water);
    return h;
}

static uint64_t hash_components(const GameState *g)
{
    uint64_t h = INK_HASH_INIT;
    const uint32_t hw =
        g->ecs.high_water <= ECS_MAX_ENTITIES ? g->ecs.high_water : ECS_MAX_ENTITIES;
    for (uint32_t i = 0; i < hw; ++i) {
        const EntityId id = ecs_entity_at(&g->ecs, i);
        if (id.index == ECS_INVALID_INDEX) {
            continue;
        }
        const uint64_t mask = g->ecs.comp_mask[i];
        h = ink_hash_u32(h, i);
        h = ink_hash_u64(h, mask);
        if ((mask & COMP_TRANSFORM) != 0) {
            const TransformComp *c = &g->comps.transform[i];
            h = ink_hash_f32(h, c->x);
            h = ink_hash_f32(h, c->y);
            h = ink_hash_f32(h, c->radius);
            h = ink_hash_f32(h, c->facing);
            h = hash_bool(h, c->dissolving);
            h = ink_hash_f32(h, c->dissolve_t);
        }
        if ((mask & COMP_VELOCITY) != 0) {
            const VelocityComp *c = &g->comps.velocity[i];
            h = ink_hash_f32(h, c->vx);
            h = ink_hash_f32(h, c->vy);
            h = ink_hash_u32(h, (uint32_t)c->mass_tier);
            h = ink_hash_f32(h, c->drag);
            h = ink_hash_f32(h, c->fluid_coupling);
        }
        if ((mask & COMP_HEALTH) != 0) {
            const HealthComp *c = &g->comps.health[i];
            h = hash_i32(h, c->hp);
            h = hash_i32(h, c->max_hp);
            h = ink_hash_f32(h, c->flash_t);
            h = ink_hash_f32(h, c->invuln_t);
        }
        if ((mask & COMP_STATUS) != 0) {
            const StatusComp *c = &g->comps.status[i];
            h = ink_hash_f32(h, c->ink_mark_t);
            h = ink_hash_f32(h, c->burn_acc);
            h = ink_hash_f32(h, c->charge_t);
            h = ink_hash_f32(h, c->knockback_t);
            h = ink_hash_f32(h, c->stun_t);
            h = ink_hash_f32(h, c->barrier_t);
        }
        if ((mask & COMP_ENEMY) != 0) {
            const EnemyComp *c = &g->comps.enemy[i];
            h = ink_hash_u32(h, (uint32_t)c->def_id);
            h = hash_bool(h, c->is_elite);
            h = ink_hash_u32(h, c->elite_flags);
            h = hash_i32(h, c->phase);
            h = ink_hash_f32(h, c->fire_cd);
            h = hash_i32(h, c->ai_state);
            h = ink_hash_f32(h, c->absorbed_ink);
            h = ink_hash_f32(h, c->windup);
        }
        if ((mask & COMP_BOSS) != 0) {
            const BossComp *c = &g->comps.boss[i];
            h = hash_i32(h, c->pattern);
            h = ink_hash_f32(h, c->pattern_timer);
            h = hash_i32(h, c->phase_idx);
            h = ink_hash_f32(h, c->gale_dx);
            h = ink_hash_f32(h, c->gale_dy);
            h = ink_hash_f32(h, c->gale_t);
            h = hash_i32(h, c->part_id);
            h = ink_hash_u32(h, c->parent_id.index);
            h = ink_hash_u32(h, c->parent_id.gen);
            h = hash_i32(h, c->telegraph_pattern);
            h = ink_hash_f32(h, c->telegraph);
            h = ink_hash_f32(h, c->stagger);
            h = ink_hash_f32(h, c->stagger_idle_t);
            h = ink_hash_f32(h, c->break_t);
            h = ink_hash_f32(h, c->break_guard_t);
            h = hash_i32(h, c->break_kind);
            h = ink_hash_f32(h, c->volley_t);
            h = hash_i32(h, c->volley_pattern);
            h = ink_hash_f32(h, c->window_total);
            h = ink_hash_f32(h, c->window_bonus);
        }
        if ((mask & COMP_PROJ) != 0) {
            const ProjComp *c = &g->comps.proj[i];
            h = ink_hash_u32(h, (uint32_t)c->kind);
            h = hash_bool(h, c->deflected);
            h = hash_bool(h, c->burning);
            h = hash_bool(h, c->electrified);
            h = hash_bool(h, c->accelerated);
            h = hash_bool(h, c->fluid_pushed);
            h = hash_i32(h, c->pierce);
            h = hash_i32(h, c->bounces_left);
            h = ink_hash_f32(h, c->age);
            h = hash_i32(h, c->trail_n);
            const int32_t tn = c->trail_n > PROJ_TRAIL_LEN ? PROJ_TRAIL_LEN : c->trail_n;
            for (int32_t k = 0; k < tn; ++k) {
                h = ink_hash_f32(h, c->trail[k].x);
                h = ink_hash_f32(h, c->trail[k].y);
            }
            h = hash_i32(h, c->hit_n);
            const int32_t hn =
                c->hit_n > PROJ_MAX_HIT_IDS ? PROJ_MAX_HIT_IDS : (c->hit_n < 0 ? 0 : c->hit_n);
            for (int32_t k = 0; k < hn; ++k) {
                h = ink_hash_u32(h, c->hit_ids[k].index);
                h = ink_hash_u32(h, c->hit_ids[k].gen);
            }
        }
        if ((mask & COMP_INK_ORB) != 0) {
            const InkOrbComp *c = &g->comps.orb[i];
            h = hash_i32(h, c->value);
            h = ink_hash_f32(h, c->age);
        }
        if ((mask & COMP_GIMMICK) != 0) {
            const GimmickComp *c = &g->comps.gimmick[i];
            h = hash_i32(h, c->kind);
            h = hash_i32(h, c->elem_kind);
            h = ink_hash_f32(h, c->timer);
            h = ink_hash_f32(h, c->param_a);
            h = ink_hash_f32(h, c->param_b);
        }
    }
    return h;
}

static uint64_t hash_run(const GameState *g)
{
    uint64_t h = INK_HASH_INIT;
    const PlayerState *p = &g->player;
    h = ink_hash_f32(h, p->ink);
    h = ink_hash_f32(h, p->ink_max);
    h = hash_bool(h, p->dry);
    h = hash_bool(h, p->painting);
    h = ink_hash_f32(h, p->stroke_speed);
    h = ink_hash_f32(h, p->idle_t);
    h = ink_hash_f32(h, p->cyclone_cd);
    h = ink_hash_f32(h, p->cyclone_ccw_cd);
    h = ink_hash_f32(h, p->cyclone_cd_total);
    h = ink_hash_f32(h, p->gale_cd);
    h = ink_hash_f32(h, p->gale_cd_total);
    h = hash_i32(h, p->gale_charges);
    h = hash_i32(h, p->gale_max_charges);
    h = ink_hash_f32(h, p->surge_t);
    h = ink_hash_f32(h, p->cursor_x);
    h = ink_hash_f32(h, p->cursor_y);
    h = ink_hash_f32(h, p->aim_x);
    h = ink_hash_f32(h, p->aim_y);

    h = hash_i32(h, g->shrine_count);
    const int32_t sc = g->shrine_count > GAME_MAX_SHRINES ? GAME_MAX_SHRINES : g->shrine_count;
    for (int32_t i = 0; i < sc; ++i) {
        const ShrineState *s = &g->shrines[i];
        h = ink_hash_f32(h, s->x);
        h = ink_hash_f32(h, s->y);
        h = ink_hash_f32(h, s->hit_radius_proj);
        h = ink_hash_f32(h, s->hit_radius_contact);
        h = hash_i32(h, s->hp);
        h = hash_i32(h, s->max_hp);
        h = ink_hash_f32(h, s->invuln_t);
    }

    const RunProgress *r = &g->run;
    h = hash_i32(h, r->act_index);
    h = hash_i32(h, r->act_count);
    h = hash_i32(h, r->wave_index);
    h = hash_i32(h, r->wave_kills);
    h = hash_i32(h, r->wave_quota);
    h = hash_i32(h, r->acts_cleared);
    h = hash_i32(h, r->endless_loop);
    h = hash_i32(h, r->endless_acts_cleared);
    h = hash_i32(h, r->bonus_crystals);
    h = hash_i32(h, r->kills);
    h = hash_i32(h, r->score);
    h = hash_i32(h, r->chain);
    h = hash_i32(h, r->max_chain);
    h = ink_hash_f32(h, r->chain_t);
    h = hash_i32(h, r->ignitions);
    h = hash_i32(h, r->storm_zaps);
    h = hash_i32(h, r->thunderbursts);
    h = hash_i32(h, r->cataclysms);
    h = hash_i32(h, r->rerolls_left);

    h = ink_hash_u32(h, (uint32_t)r->reward.kind);
    h = hash_i32(h, r->reward.choice_count);
    for (int i = 0; i < GAME_MAX_REWARD_CHOICES; ++i) {
        h = hash_i32(h, r->reward.choice_ids[i]);
        h = hash_u8(h, r->reward.choice_next_level[i]);
    }
    h = hash_bool(h, r->reward.repair_available);
    h = hash_i32(h, r->reward.repair_amount);
    h = hash_i32(h, r->reward.crystal_reward);
    h = ink_hash_u32(h, r->reward.serial);

    h = hash_i32(h, r->upgrade_count);
    const int32_t uc =
        r->upgrade_count > GAME_MAX_OWNED_UPGRADES ? GAME_MAX_OWNED_UPGRADES : r->upgrade_count;
    for (int32_t i = 0; i < uc; ++i) {
        h = ink_hash_u32(h, (uint32_t)r->upgrades[i].id);
        h = hash_u8(h, r->upgrades[i].level);
    }
    h = hash_i32(h, r->relic_count);
    const int32_t rc =
        r->relic_count > GAME_MAX_OWNED_RELICS ? GAME_MAX_OWNED_RELICS : r->relic_count;
    for (int32_t i = 0; i < rc; ++i) {
        h = ink_hash_u32(h, (uint32_t)r->relics[i]);
    }
    h = ink_hash_u32(h, r->boss_id.index);
    h = ink_hash_u32(h, r->boss_id.gen);
    h = hash_bool(h, r->run_settled);

    const RyuBodyState *rb = &g->ryu_body;
    h = ink_hash_u32(h, rb->head_id.index);
    h = ink_hash_u32(h, rb->head_id.gen);
    for (int i = 0; i < GAME_RYU_ORB_COUNT; ++i) {
        h = ink_hash_u32(h, rb->orb_ids[i].index);
        h = ink_hash_u32(h, rb->orb_ids[i].gen);
    }
    h = hash_u8(h, rb->orb_broken_mask);
    h = hash_i32(h, rb->active_orb_slot);
    h = hash_i32(h, rb->trail_head);
    h = hash_i32(h, rb->trail_count);
    const int32_t tc = rb->trail_count > GAME_RYU_TRAIL_CAP
                           ? GAME_RYU_TRAIL_CAP
                           : (rb->trail_count < 0 ? 0 : rb->trail_count);
    for (int32_t i = 0; i < tc; ++i) {
        h = ink_hash_f32(h, rb->trail[i].x);
        h = ink_hash_f32(h, rb->trail[i].y);
    }
    return h;
}

static uint64_t hash_events(const GameState *g)
{
    uint64_t h = INK_HASH_INIT;
    h = hash_i32(h, g->event_count);
    const int32_t ec = g->event_count > GAME_MAX_EVENTS ? GAME_MAX_EVENTS : g->event_count;
    for (int32_t i = 0; i < ec; ++i) {
        const GameEvent *ev = &g->events[i];
        h = ink_hash_u32(h, (uint32_t)ev->type);
        h = ink_hash_f32(h, ev->x);
        h = ink_hash_f32(h, ev->y);
        h = ink_hash_f32(h, ev->x2);
        h = ink_hash_f32(h, ev->y2);
        h = hash_i32(h, ev->value);
        h = ink_hash_u32(h, ev->flags);
    }
    h = ink_hash_u32(h, g->dropped_events);
    h = hash_i32(h, g->pending_impulse_count);
    const int32_t pc = g->pending_impulse_count > GAME_MAX_PENDING_IMPULSE
                           ? GAME_MAX_PENDING_IMPULSE
                           : g->pending_impulse_count;
    for (int32_t i = 0; i < pc; ++i) {
        const PendingImpulse *imp = &g->pending_impulses[i];
        h = ink_hash_f32(h, imp->x);
        h = ink_hash_f32(h, imp->y);
        h = ink_hash_f32(h, imp->rad_cells);
        h = ink_hash_f32(h, imp->peak_speed);
    }
    return h;
}

static uint64_t hash_internal(const GameInternal *in)
{
    uint64_t h = INK_HASH_INIT;
    const RunModifiers *m = &in->mods;
    h = ink_hash_f32(h, m->brush_radius_mul);
    h = ink_hash_f32(h, m->brush_force_mul);
    h = ink_hash_f32(h, m->stroke_cost_mul);
    h = ink_hash_f32(h, m->gale_impulse_mul);
    h = ink_hash_f32(h, m->gale_cost_add);
    h = ink_hash_f32(h, m->gale_fire_add);
    h = ink_hash_f32(h, m->gale_end_blast_dmg);
    h = ink_hash_f32(h, m->cyclone_storm_add);
    h = ink_hash_f32(h, m->cyclone_storm_sustain);
    h = ink_hash_f32(h, m->ink_max_add);
    h = ink_hash_f32(h, m->ink_regen_mul);
    h = hash_bool(h, m->ink_regen_no_delay);
    h = ink_hash_f32(h, m->burn_rate_mul);
    h = ink_hash_f32(h, m->burn_base_dmg);
    h = hash_bool(h, m->inferno_ember_on_kill);
    h = ink_hash_f32(h, m->fire_dmg_mul);
    h = hash_i32(h, m->chain_hops_add);
    h = ink_hash_f32(h, m->chain_range_mul);
    h = ink_hash_f32(h, m->cyclone_radius_mul);
    h = ink_hash_f32(h, m->cyclone_duration_add);
    h = hash_i32(h, m->maelstrom_bind_level);
    h = ink_hash_f32(h, m->cyclone_cd);
    h = ink_hash_f32(h, m->gale_cd);
    h = ink_hash_f32(h, m->surge_duration);
    h = ink_hash_f32(h, m->chain_grace);
    h = ink_hash_f32(h, m->orb_pull_radius);
    h = hash_bool(h, m->cyclone_pulls_all_orbs);
    h = ink_hash_f32(h, m->dry_force_override);
    h = hash_bool(h, m->dry_zero_cost);
    h = ink_hash_f32(h, m->dry_deflect_dmg_mul);
    h = hash_i32(h, m->burning_proj_pierce);
    h = hash_bool(h, m->burning_proj_hit_splat);
    h = hash_i32(h, m->thunder_proj_targets);
    h = hash_bool(h, m->deflect_auto_electrify);
    h = ink_hash_f32(h, m->storm_zap_cd);
    h = ink_hash_f32(h, m->thunderburst_thresh);
    h = ink_hash_f32(h, m->thunderburst_radius_mul);
    h = ink_hash_f32(h, m->homing_steer_mul);
    h = ink_hash_f32(h, m->deflected_max_speed_mul);
    h = ink_hash_f32(h, m->ink_mark_mul);
    h = ink_hash_f32(h, m->ink_mark_duration);
    h = hash_i32(h, m->shrine_max_hp_delta);
    h = ink_hash_f32(h, m->shrine_invuln_add);
    h = hash_bool(h, m->barrier_shield_enabled);
    h = ink_hash_f32(h, m->stroke_ember_interval);
    h = ink_hash_f32(h, m->stroke_ember_mul);
    h = ink_hash_f32(h, m->stroke_ember_rad);
    h = ink_hash_f32(h, m->stroke_ember_amt);
    h = ink_hash_f32(h, m->relic_ember_interval);
    h = ink_hash_f32(h, m->ink_damp_override);
    h = ink_hash_f32(h, m->fire_damp_override);
    h = hash_bool(h, m->twin_brush);
    h = hash_bool(h, m->tengu_fan);
    h = hash_bool(h, m->thunder_drum);
    h = hash_bool(h, m->ink_dyed_shrine);
    h = hash_bool(h, m->rosary_swirl);
    h = hash_bool(h, m->wind_return);
    h = hash_i32(h, m->chain_storm_interval);
    h = ink_hash_f32(h, m->chain_storm_mul);
    h = hash_bool(h, m->surge_stroke_storm);
    h = hash_i32(h, m->gale_max_charges);
    h = ink_hash_f32(h, m->cataclysm_thresh);
    h = ink_hash_f32(h, m->cataclysm_cd_base);
    h = ink_hash_f32(h, m->surge_reaction_dmg_mul);
    h = hash_i32(h, m->surge_min_chain);
    h = hash_bool(h, m->thunderfire_gem);
    h = hash_bool(h, m->oni_tile);
    h = hash_bool(h, m->night_parade_scroll);
    h = hash_i32(h, m->bonus_kill_orbs);
    h = ink_hash_f32(h, m->spawn_interval_mul);
    h = hash_i32(h, m->max_alive_add);

    for (int i = 0; i < GAME_MAX_ACTIVE_CYCLONES; ++i) {
        const ActiveCyclone *c = &in->cyclones[i];
        h = hash_bool(h, c->active);
        h = ink_hash_f32(h, c->x);
        h = ink_hash_f32(h, c->y);
        h = ink_hash_f32(h, c->rad_cells);
        h = ink_hash_f32(h, c->life);
        h = ink_hash_f32(h, c->max_life);
        h = ink_hash_f32(h, c->storm_sustain);
        h = ink_hash_f32(h, c->spin);
    }

    h = ink_hash_f32(h, in->ignite_callout_cd);
    h = hash_bool(h, in->blazing_active);
    h = ink_hash_f32(h, in->storm_zap_cd);
    h = ink_hash_f32(h, in->ember_tip_dist);
    h = ink_hash_f32(h, in->spawn_cd);
    h = ink_hash_u32(h, in->seen_enemies_mask);
    h = hash_bool(h, in->act_prepared);
    h = hash_bool(h, in->first_reward_offered);
    h = hash_bool(h, in->wave_clear_is_boss);
    h = ink_hash_f32(h, in->last_boss_x);
    h = ink_hash_f32(h, in->last_boss_y);
    h = ink_hash_f32(h, in->boss_vortex_t);
    h = ink_hash_f32(h, in->boss_vortex_x);
    h = ink_hash_f32(h, in->boss_vortex_y);

    h = ink_hash_f32(h, in->relic_ember_dist);
    for (int i = 0; i < GAME_THUNDERBURST_BLOCKS; ++i) {
        h = ink_hash_f32(h, in->thunderburst_block_cd[i]);
    }
    h = ink_hash_f32(h, in->thunderburst_hitstop_cd);
    h = ink_hash_f32(h, in->cataclysm_cd);
    h = ink_hash_f32(h, in->barrier_shield_cd);
    h = hash_bool(h, in->barrier_shield_ready);
    h = ink_hash_f32(h, in->wind_return_cd);
    h = ink_hash_u32(h, in->triggered_hints_mask);
    for (int i = 0; i < ECS_MAX_ENTITIES; ++i) {
        h = ink_hash_f32(h, in->maelstrom_bind_t[i]);
        h = ink_hash_f32(h, in->maelstrom_hit_cd[i]);
    }

    h = ink_hash_f32(h, in->wind_timer);
    h = ink_hash_f32(h, in->wind_telegraph_t);
    h = ink_hash_f32(h, in->wind_active_t);
    h = hash_i32(h, in->wind_dir);
    h = hash_i32(h, in->wave_elites_spawned);
    h = ink_hash_u32(h, in->last_elite_flags);
    for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < 2; ++j) {
            h = ink_hash_f32(h, in->nue_trail_x[i][j]);
        }
    }
    for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < 2; ++j) {
            h = ink_hash_f32(h, in->nue_trail_y[i][j]);
        }
    }
    for (int i = 0; i < 2; ++i) {
        h = ink_hash_f32(h, in->nue_trail_t[i]);
    }
    h = ink_hash_f32(h, in->ryu_coil_t);
    h = ink_hash_f32(h, in->ryu_coil_angle);
    h = hash_bool(h, in->ryu_coil_active);
    h = ink_hash_f32(h, in->ryu_ward_ping_cd);
    h = ink_hash_f32(h, in->thunderfire_gem_cd);
    h = hash_bool(h, in->sandbox_infinite_ink);
    return h;
}

void game_hash_sections(const GameState *g, uint64_t out[GAME_HASH_SECTION_COUNT])
{
    if (out == NULL) {
        return;
    }
    if (g == NULL) {
        memset(out, 0, sizeof(uint64_t) * GAME_HASH_SECTION_COUNT);
        return;
    }
    out[GAME_HASH_CORE] = hash_core(g);
    out[GAME_HASH_FLUID] = fluid_hash(&g->fluid);
    out[GAME_HASH_ECS] = hash_ecs(&g->ecs);
    out[GAME_HASH_COMPONENTS] = hash_components(g);
    out[GAME_HASH_RUN] = hash_run(g);
    out[GAME_HASH_EVENTS] = hash_events(g);
    out[GAME_HASH_INTERNAL] = hash_internal(&g->internal);
}

uint64_t game_hash(const GameState *g)
{
    if (g == NULL) {
        return 0;
    }
    uint64_t sections[GAME_HASH_SECTION_COUNT];
    game_hash_sections(g, sections);
    uint64_t h = INK_HASH_INIT;
    for (int i = 0; i < GAME_HASH_SECTION_COUNT; ++i) {
        h = ink_hash_u64(h, sections[i]);
    }
    return h;
}
