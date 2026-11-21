#ifndef INK_APP_FRAME_CLOCK_H
#define INK_APP_FRAME_CLOCK_H

typedef struct {
    double accumulator;
} FrameClock;

void frame_clock_reset(FrameClock *c);
int frame_clock_advance(FrameClock *c, double frame_dt);

#endif
