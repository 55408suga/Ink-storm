#include "frame_clock.h"

#include "core.h"

#include <math.h>
#include <stddef.h>

void frame_clock_reset(FrameClock *c)
{
    if (c != NULL) {
        c->accumulator = 0.0;
    }
}

int frame_clock_advance(FrameClock *c, double frame_dt)
{
    if (c == NULL || !isfinite(frame_dt) || frame_dt < 0.0) {
        return 0;
    }
    const double max_dt = 0.25;
    const double step_dt = 1.0 / (double)INK_TICK_HZ;
    const double dt = frame_dt > max_dt ? max_dt : frame_dt;

    c->accumulator += dt;
    int ticks = 0;
    while (c->accumulator >= step_dt && ticks < 15) {
        c->accumulator -= step_dt;
        ++ticks;
    }
    return ticks;
}
