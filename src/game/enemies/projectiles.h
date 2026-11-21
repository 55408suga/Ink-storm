#ifndef INK_GAME_PROJECTILES_H
#define INK_GAME_PROJECTILES_H

#include "game.h"

EntityId projectiles_spawn(GameState *g, ProjKind kind, float x, float y, float vx, float vy);
EntityId projectiles_spawn_dir(GameState *g, ProjKind kind, float x, float y, float dx, float dy,
                               float speed);
void projectiles_step(GameState *g);

#endif
