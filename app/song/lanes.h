/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#ifndef FM1_SONG_LANES_H
#define FM1_SONG_LANES_H
#include "song.h"

/* Automation (design section 7). A lane is one controller on a track,
 * numbered by its controller (0 to 127), or the track's program changes.
 * Lane events sit among the notes, 8 bytes each, and only changes are
 * stored. Each engine takes the controllers its MIDI handler knows, and
 * those are the parameters the knobs offer. */
enum { SONG_DX7, SONG_303, SONG_808, SONG_PSG, SONG_PLUCK, SONG_SAMPLER, SONG_ENGINES };
enum { SONG_LANE_PROGRAM = 128, SONG_LANES = 129 };
struct song_param { uint8_t cc, initial; char name[11]; };
/* An engine's parameters, in the order the knob page offers them. */
unsigned song_params(unsigned engine, const uint8_t **index);
const struct song_param *song_param(unsigned index);
const struct song_param *song_param_for(unsigned engine, unsigned cc); /* NULL if not taken */
/* The controller on knob 0..3 of a track: the song's choice, or the
 * engine's default when unset or not taken by this engine. */
unsigned song_knob_cc(const struct song *s, unsigned engine, unsigned track, unsigned knob);
unsigned song_programs(unsigned engine); /* program count, 0 when it has none */

static inline int song_is_lane(const struct song_event *e)
{
    return song_event_kind(e) == SONG_CONTROL || song_event_kind(e) == SONG_PROGRAM;
}
static inline unsigned song_event_lane(const struct song_event *e)
{
    return song_event_kind(e) == SONG_PROGRAM ? SONG_LANE_PROGRAM : e->a;
}
static inline unsigned song_event_value(const struct song_event *e)
{
    return song_event_kind(e) == SONG_PROGRAM ? e->a : e->b;
}
struct song_event song_lane_event(uint32_t clock, unsigned lane, unsigned value);
/* The value as of `clock`: the last change at or before it, else, for a
 * controller, the first one after it. -1 when there is none. */
int song_lane_at(const struct song *s, unsigned track, unsigned lane, uint32_t clock);
/* The track index of the lane's change at exactly `clock`, or -1. */
int song_lane_find(const struct song *s, unsigned track, unsigned lane, uint32_t clock);
/* Sets the lane's value at `clock`, replacing a change there. Journaled. */
int song_lane_set(struct song *s, unsigned track, unsigned lane, uint32_t clock, unsigned value, unsigned fresh);
/* Chase: every lane's value as of `clock`, skipping changes exactly at
 * `clock`, which play anyway. A controller with nothing before uses its
 * first value. A program change only counts from where it is, and before
 * a track's first one the song's saved program (song.h) is sent, when known. */
typedef void (*song_lane_fn)(void *context, unsigned track, unsigned lane, unsigned value);
void song_chase(const struct song *s, unsigned track, uint32_t clock, song_lane_fn fn, void *context);
/* Removes a lane's changes in [from, to) on the tracks in the mask. */
unsigned song_lane_clear(struct song *s, unsigned tracks, unsigned lane, uint32_t from, uint32_t to);
/* The next (direction > 0) or previous change on a lane, or -1. */
int64_t song_lane_next(const struct song *s, unsigned track, unsigned lane, uint32_t clock, int direction);
#endif
