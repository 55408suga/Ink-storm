#ifndef INK_GAME_PLAYER_SKILLS_H
#define INK_GAME_PLAYER_SKILLS_H

#include "game.h"

typedef struct {
    float px, py;
    float x, y;
    float vx, vy;
    float rad;
    float fire_amt;
} EmberTrailSegment;

void player_skills_step(GameState *g, const BrushInput *valid_in);
GameVec2 player_skills_stroke_force(double mdx, double mdy, float force_scale, float max_vel);
void player_skills_emit_ember_trail(GameState *g, float *dist_acc, float interval,
                                    EmberTrailSegment seg);
void player_skills_refill_sandbox_ink(GameState *g);
void player_skills_cast_free_gale(GameState *g, float origin_x, float origin_y, float dir_x,
                                  float dir_y);

#endif
