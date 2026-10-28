/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#ifndef FM1_SONG_TOUCH_H
#define FM1_SONG_TOUCH_H
#include "lanes.h"

/* Knob recording in touch style (design section 7). While a knob moves it
 * writes its lane on the take track, at most one change per 32nd, flagged
 * as part of the take, and the older changes it passes over are replaced.
 * Once it stops, the lane goes back to the older values. */
enum { SONG_TOUCH_GRID = SONG_PPQ / 8 };
struct song_touch {
    uint32_t from, last;   /* the first and latest 32nd written */
    uint8_t lane, value, active;
};
/* A knob set the lane to `value` at `clock`. */
void song_touch_move(struct song *s, unsigned track, struct song_touch *t, unsigned lane, unsigned value,
                     uint32_t clock);
/* The knob stopped at `clock`. Older changes in the touched span go, and
 * the lane returns to its older value there, which is returned (-1 when
 * the lane had nothing before, so the knob's value stays). */
int song_touch_release(struct song *s, unsigned track, struct song_touch *t, uint32_t clock);
#endif
