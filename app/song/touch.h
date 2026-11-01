/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#ifndef FM1_SONG_TOUCH_H
#define FM1_SONG_TOUCH_H
#include "lanes.h"

/* Knob recording in latch style (design section 7, touch style before
 * 0.6.2). From its first turn in a take a knob writes its lane on the take
 * track, at most one change per 32nd, flagged as part of the take, and the
 * older changes it passes over are replaced, turning or not, until the take
 * ends. Then the lane goes back to the older values. In a loop take each
 * pass replaces the one before. */
enum { SONG_TOUCH_GRID = SONG_PPQ / 8 };
struct song_touch {
    uint32_t from, last;   /* the first and latest 32nd written */
    int16_t under;         /* on a loop pass after the first, what the pass before had at the loop's start, else -1 */
    uint8_t lane, value, active;
};
/* A knob set the lane to `value` at `clock`. */
void song_touch_move(struct song *s, unsigned track, struct song_touch *t, unsigned lane, unsigned value,
                     uint32_t clock);
/* The take ended at `clock`. Older changes in the touched span go, and the
 * lane returns to its older value there, which is returned (-1 when the
 * lane had nothing before, so the knob's value stays). */
int song_touch_release(struct song *s, unsigned track, struct song_touch *t, uint32_t clock);
/* A loop pass ends at `end`, the loop's end: the older changes the pass went
 * over go, and on the knob's `first` pass the lane's older value at `end`
 * is kept there, for what follows the loop. */
void song_touch_wrap(struct song *s, unsigned track, struct song_touch *t, uint32_t end, int first);
/* After a pass, its changes on the track count as older for the next. */
void song_touch_settle(struct song *s, unsigned track);
/* The next pass starts at `start`, the knob still latched at `value`. */
void song_touch_again(struct song *s, unsigned track, struct song_touch *t, unsigned lane, unsigned value,
                      uint32_t start);
#endif
