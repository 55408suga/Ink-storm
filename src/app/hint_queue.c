#include "hint_queue.h"

#include <stddef.h>

void hint_queue_clear(HintQueue *q)
{
    if (q != NULL) {
        q->head = 0;
        q->count = 0;
    }
}

bool hint_queue_contains(const HintQueue *q, HintId id)
{
    if (q == NULL) {
        return false;
    }
    for (int32_t i = 0; i < q->count; ++i) {
        if (q->items[(q->head + i) % HINT_COUNT] == id) {
            return true;
        }
    }
    return false;
}

bool hint_queue_push(HintQueue *q, HintId id)
{
    if (q == NULL || q->count >= HINT_COUNT || hint_queue_contains(q, id)) {
        return false;
    }
    q->items[(q->head + q->count) % HINT_COUNT] = id;
    q->count++;
    return true;
}

/* Hints only age, and the next one only starts, while the hint strip can actually be seen, so a
 * hint queued during a clear or the reward screen is still shown in full afterwards. Returns the
 * hint that started showing this step, or -1. */
int32_t hint_queue_step(HintQueue *q, HintId *active, float *active_t, float dt, bool visible)
{
    if (q == NULL || active == NULL || active_t == NULL || !visible) {
        return -1;
    }
    if (*active_t > 0.0f) {
        *active_t = *active_t - dt > 0.0f ? *active_t - dt : 0.0f;
    }
    if (*active_t > 0.0f || q->count <= 0) {
        return -1;
    }
    *active = q->items[q->head];
    q->head = (q->head + 1) % HINT_COUNT;
    q->count--;
    *active_t = HINT_SHOW_SECONDS;
    return (int32_t)*active;
}

int32_t hint_queue_step_enabled(HintQueue *q, HintId *active, float *active_t, float dt,
                                bool visible, bool enabled)
{
    if (!enabled) {
        hint_queue_clear(q);
        if (active != NULL) {
            *active = HINT_STROKE;
        }
        if (active_t != NULL) {
            *active_t = 0.0f;
        }
        return -1;
    }
    return hint_queue_step(q, active, active_t, dt, visible);
}
