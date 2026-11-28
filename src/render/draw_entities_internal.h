#ifndef INK_RENDER_DRAW_ENTITIES_INTERNAL_H
#define INK_RENDER_DRAW_ENTITIES_INTERNAL_H

#include "render_internal.h"

/* draw_entities.c の手続き描画と draw_entities_sprite.c の絵の描画で共有する部品 */
uint32_t render_entity_seed(EntityId id, uint32_t part, uint64_t boil_step);
void render_draw_eye_brush(RenderState *r, Vector2 at, float size, PaletteId pal, float alpha);
int render_elite_palettes(const EnemyComp *en, PaletteId out[4]);
void render_draw_tengu_telegraph(RenderState *r, const TransformComp *tr, const BossComp *boss,
                                 const ShrineState *shrine, uint32_t seed);
void render_draw_nue_telegraph(RenderState *r, const TransformComp *tr, const BossComp *boss,
                               uint32_t seed);
void render_draw_ryu_telegraph(RenderState *r, const BossComp *boss, const ShrineState *shrine,
                               uint32_t seed);
void render_draw_boss_stagger_arc(RenderState *r, const GameState *g, const TransformComp *tr,
                                  const EnemyComp *en, const BossComp *boss, uint32_t seed);
void render_draw_boss_break_ring(RenderState *r, const TransformComp *tr, const EnemyComp *en,
                                 const BossComp *boss, bool reduce_flash, uint32_t seed);
void render_draw_nue_grounded_ring(RenderState *r, const TransformComp *tr, const EnemyComp *en,
                                   const BossComp *boss, const HealthComp *hp, uint32_t seed);
void render_draw_ryu_ward(RenderState *r, const GameState *g, const TransformComp *tr,
                          const BossComp *boss, uint32_t seed);
void render_draw_ryu_orb_target(RenderState *r, const GameState *g, const TransformComp *tr,
                                const BossComp *boss, bool reduce_flash, uint32_t seed);

#endif
