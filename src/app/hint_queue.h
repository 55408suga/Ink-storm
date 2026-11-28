#ifndef INK_APP_HINT_QUEUE_H
#define INK_APP_HINT_QUEUE_H

#include "game.h"

#include <stdbool.h>
#include <stdint.h>

#define HINT_SHOW_SECONDS 4.0f

typedef struct {
    HintId items[HINT_COUNT];
    int32_t head;
    int32_t count;
} HintQueue;

void hint_queue_clear(HintQueue *q);
bool hint_queue_contains(const HintQueue *q, HintId id);
bool hint_queue_push(HintQueue *q, HintId id);
int32_t hint_queue_step(HintQueue *q, HintId *active, float *active_t, float dt, bool visible);
int32_t hint_queue_step_enabled(HintQueue *q, HintId *active, float *active_t, float dt,
                                bool visible, bool enabled);

#endif
