#ifndef INK_RENDER_SPRITES_H
#define INK_RENDER_SPRITES_H

#include "render_internal.h"
#include "sprite_rules.h"

/* Sprite bank (sprites.c). render_sprites_init は GL コンテキストの作成後に呼ぶ */
void render_sprites_init(RenderState *r, const RenderOptions *opt);
void render_sprites_close(RenderState *r);
/* 描ける ID を返す。状態差分が欠ければ基本の絵、群が揃わなければ SPR_NONE */
SpriteId render_sprite_resolve(const RenderState *r, SpriteId id);
void render_sprite_draw(RenderState *r, SpriteId id, const SpriteXform *xf, Color tint);
void render_sprite_draw_halo(RenderState *r, SpriteId id, const SpriteXform *xf, Color tint);
void render_sprite_draw_ryu_band(RenderState *r, SpriteId id, bool halo, const GameVec2 *spine,
                                 int32_t spine_n, bool awakened, float time_s, bool flip_v,
                                 float reveal, Color tint);

/* Per-character composition (draw_entities_sprite.c)。false を返したら手続き描画へ戻す */
void render_sprite_on_ryu_kill(RenderState *r);
void render_sprite_tick(RenderState *r, const GameState *g, float dt);
void render_sprite_draw_halos(RenderState *r, const GameState *g, int act_index);
bool render_sprite_draw_shrine(RenderState *r, const GameState *g, int32_t shrine_index,
                               int act_index);
bool render_sprite_draw_enemy(RenderState *r, const GameState *g, EntityId id,
                              const ShrineState *shrine, int act_index);
void render_sprite_draw_ryu_death(RenderState *r, int act_index);
void render_sprite_draw_tints(RenderState *r, const GameState *g);
void render_sprite_draw_cores(RenderState *r, const GameState *g);

#endif
