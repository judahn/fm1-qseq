/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Step entry: notes, chords, rests, ties, repeats, deleting and the pointer.
 * Every length is a whole number of clocks: 64th to whole, triplet on any
 * of them, dot or double dot, except a double-dotted plain 64th (52.5). */
#include "step.h"
#include "glitch.h"
#include "song_config.h"
#include <string.h>
#if SONG_UI

const uint8_t step_gate_percent[STEP_GATES] = {50, 90, 99, 100};
static const uint16_t base_clocks[STEP_LENGTHS] = {30, 60, 120, 240, 480, 960, 1920};

SONG_SMALL void step_init(struct song_step *st, struct song *s, const struct song_output *out)
{
    memset(st, 0, sizeof *st);
    st->song = s;
    st->out = *out;
    st->length = STEP_16TH;
    st->gate = 1;
    st->velocity = 100;
    st->random = 0x9e3779b9U;
}
SONG_SMALL void step_begin(struct song_step *st, unsigned track, uint32_t pointer)
{
    step_preview_stop(st);
    st->track = (uint8_t)(track < SONG_TRACKS ? track : 0);
    st->pointer = pointer;
    st->chord_count = st->last_count = st->tie_ok = st->picked = st->ranged = 0;
}
SONG_SMALL uint32_t step_clocks(const struct song_step *st)
{
    static const uint8_t dotted[3] = {4, 6, 7}; /* plain, dot, double dot, in quarters */
    return base_clocks[st->length] * (st->triplet ? 2U : 3U) * dotted[st->dots % 3U] / 12U;
}
SONG_SMALL uint32_t step_gate_clocks(const struct song_step *st, uint32_t step)
{
    uint32_t length = (step * step_gate_percent[st->gate] + 50U) / 100U;
    return length ? length : 1;
}

/* Events at a clock on the step track: [first, end) as track indices.
 * Step entry works on the notes among them and leaves automation alone. */
SONG_SMALL static unsigned at_clock(const struct song_step *st, uint32_t clock, unsigned *end)
{
    unsigned count, i = song_find(st->song, st->track, clock);
    const struct song_event *e = song_track(st->song, st->track, &count);
    for (*end = i; *end < count && song_event_start(&e[*end]) == clock; ++*end) {}
    return i;
}
SONG_SMALL static int find_note(const struct song_step *st, uint32_t clock, unsigned pitch)
{
    unsigned end, i = at_clock(st, clock, &end);
    const struct song_event *e = st->song->event + st->song->first[st->track];
    for (; i < end; ++i)
        if (song_is_note(&e[i]) && e[i].a == pitch) return (int)(st->song->first[st->track] + i);
    return -1;
}
SONG_SMALL unsigned step_selection(const struct song_step *st, unsigned index[STEP_CHORD])
{
    unsigned end, n = 0, i = at_clock(st, st->pointer, &end), first = st->song->first[st->track];
    for (; i < end && n < STEP_CHORD; ++i) {
        unsigned pitch = st->song->event[first + i].a;
        if (!song_is_note(&st->song->event[first + i])) continue;
        if (!st->picked || (st->pick[pitch / 32U] >> (pitch % 32U) & 1U)) index[n++] = first + i;
    }
    return n;
}

SONG_SMALL SONG_SMALL void step_change(struct song_step *st, unsigned at, const struct song_event *e)
{
    (void)song_set_event(st->song, st->track, at - st->song->first[st->track], e);
}
SONG_SMALL static void set_length(struct song_step *st, unsigned at, uint32_t length)
{
    struct song_event e = st->song->event[at];
    e.length = (uint16_t)(length < 0xffff ? length : 0xffff);
    step_change(st, at, &e);
}
static void advance(struct song_step *st, uint32_t clocks)
{
    struct song *s = st->song;
    st->pointer += clocks;
    st->picked = st->ranged = 0;
    /* Entering past the end makes the song longer. */
    while (st->pointer >= song_end(s) && s->bars < SONG_MAX_BARS) ++s->bars;
    if (st->pointer > song_end(s)) st->pointer = song_end(s);
}
/* All or nothing: a chord that doesn't fit leaves the song as it was. Each
 * note starts its offset after the pointer, and ends with the others. */
SONG_SMALL static int enter(struct song_step *st, const uint8_t *pitches, const uint8_t *offsets, unsigned count)
{
    struct song *s = st->song;
    uint32_t step = step_clocks(st), gate = step_gate_clocks(st, step);
    unsigned fresh = 0;
    for (unsigned i = 0; i < count; ++i) fresh += find_note(st, st->pointer + offsets[i], pitches[i]) < 0;
    if (s->events + fresh > SONG_EVENTS) return -1;
    int glitch = song_glitch_intern(s, st->glitch); /* entered notes get the entry glitch */
    uint32_t flags = (uint32_t)(glitch > 0 ? glitch : 0) << SONG_GLITCH_SHIFT;
    for (unsigned i = 0; i < count; ++i) {
        unsigned velocity = step_entry_velocity(st);
        uint32_t start = st->pointer + offsets[i], length = gate > offsets[i] ? gate - offsets[i] : 1;
        int at = find_note(st, start, pitches[i]);
        if (at < 0) {
            const struct song_event e = {start | flags, pitches[i], (uint8_t)velocity, (uint16_t)length};
            (void)song_add_event(s, st->track, &e);
        } else {
            struct song_event e = s->event[at]; /* the same pitch again replaces it */
            e.b = (uint8_t)velocity;
            e.length = (uint16_t)length;
            e.at = (e.at & ~SONG_GLITCH_BITS) | flags;
            step_change(st, (unsigned)at, &e);
        }
    }
    if (pitches != st->last) { memcpy(st->last, pitches, count); memcpy(st->last_strum, offsets, count); }
    st->last_count = (uint8_t)count;
    st->last_at = st->pointer;
    st->tie_ok = 1;
    advance(st, step);
    return 0;
}
SONG_SMALL void step_key(struct song_step *st, unsigned pitch)
{
    if (pitch > 127 || st->chord_count >= STEP_CHORD) return;
    for (unsigned i = 0; i < st->chord_count; ++i)
        if (st->chord[i] == pitch) return;
    st->strum[st->chord_count] = 0;
    st->chord[st->chord_count++] = (uint8_t)pitch;
}
SONG_SMALL void step_chord(struct song_step *st, const uint8_t *pitch, const uint8_t *offset, unsigned count)
{
    st->chord_count = 0;
    for (unsigned i = 0; i < count; ++i) {
        step_key(st, pitch[i]);
        st->strum[i] = offset[i];
    }
}
SONG_SMALL int step_commit(struct song_step *st)
{
    if (!st->chord_count) return -1;
    int result = enter(st, st->chord, st->strum, st->chord_count);
    st->chord_count = 0;
    return result;
}
SONG_SMALL int step_repeat(struct song_step *st)
{
    return st->last_count ? enter(st, st->last, st->last_strum, st->last_count) : -1;
}
SONG_SMALL int step_rest(struct song_step *st)
{
    st->tie_ok = 0;
    advance(st, step_clocks(st));
    return 0;
}
/* Lengthen the last entry by one more step, which may be a different
 * length: it sounds through to the new step and ends with its gate. */
SONG_SMALL int step_tie(struct song_step *st)
{
    if (!st->tie_ok || !st->last_count) return -1;
    uint32_t step = step_clocks(st), span = st->pointer - st->last_at + step_gate_clocks(st, step);
    for (unsigned i = 0; i < st->last_count; ++i) {
        int at = find_note(st, st->last_at + st->last_strum[i], st->last[i]);
        if (at >= 0) set_length(st, (unsigned)at, span > st->last_strum[i] ? span - st->last_strum[i] : 1U);
    }
    ++st->song->revision;
    advance(st, step);
    return 0;
}
SONG_SMALL int step_delete(struct song_step *st)
{
    uint32_t from, to;
    if (step_range(st, &from, &to)) return step_range_delete(st);
    unsigned index[STEP_CHORD], n = step_selection(st, index);
    while (n--) (void)song_remove(st->song, st->track, index[n] - st->song->first[st->track]);
    st->tie_ok = st->picked = 0;
    return 0;
}
/* Back one step, deleting the notes that start in the step backed over. */
SONG_SMALL int step_back_delete(struct song_step *st)
{
    uint32_t step = step_clocks(st), to = st->pointer > step ? st->pointer - step : 0;
    unsigned from = song_find(st->song, st->track, to), end = song_find(st->song, st->track, st->pointer);
    while (end-- > from)
        if (song_is_note(&st->song->event[st->song->first[st->track] + end])) (void)song_remove(st->song, st->track, end);
    st->pointer = to;
    st->tie_ok = st->picked = st->ranged = 0;
    return 0;
}
/* The pointer line to line on the step grid (0.6.7, step_move.c), or note
 * to note (`jump`, by the direction). */
SONG_SMALL static void slide(struct song_step *st, int steps, unsigned jump)
{
    unsigned count, i;
    const struct song_event *e = song_track(st->song, st->track, &count);
    if (!jump) st->pointer = step_grid_move(st, st->pointer, steps);
    else if (steps > 0)
        for (i = song_find(st->song, st->track, st->pointer + 1); i < count; ++i) {
            if (song_is_note(&e[i])) { st->pointer = song_event_start(&e[i]); break; }
        }
    else
        for (i = song_find(st->song, st->track, st->pointer); i-- > 0;)
            if (song_is_note(&e[i])) { st->pointer = song_event_start(&e[i]); break; }
}
SONG_SMALL void step_move(struct song_step *st, int steps)
{
    st->ranged = 0;
    slide(st, steps, 0);
    step_land(st);
}
SONG_SMALL void step_jump(struct song_step *st, int direction)
{
    st->ranged = 0;
    slide(st, direction, 1);
    step_land(st);
}
/* A sweep starts at the pointer with every pitch in it, and the pointer
 * moves on without landing, so the knobs keep what they show. */
SONG_SMALL void step_sweep(struct song_step *st, int steps, unsigned jump)
{
    if (!st->ranged) { st->ranged = 1; st->anchor = st->pointer; st->picked = 0; }
    step_preview_stop(st);
    slide(st, steps, jump);
    st->tie_ok = 0;
}
SONG_SMALL void step_preview_stop(struct song_step *st)
{
    for (unsigned i = 0; i < st->preview_count; ++i)
        st->out.note(st->out.context, st->preview_track, st->preview[i], 0);
    st->preview_count = 0;
}
#endif
