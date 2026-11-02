/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Step entry's pitch knob: the selected notes by scale step, by octave, or
 * along a drum track's pads. A chord moves without its notes bumping into
 * each other, and a note that would land on another note's pitch stays
 * put. A sweep's notes (step_range.c) move clock by clock, the same way,
 * and the pitches picked in it follow them. */
#include "step.h"
#include "song_config.h"
#include <string.h>
#if SONG_UI

struct shift { int steps; unsigned octaves, count; const uint8_t *pads; };

SONG_SMALL static int find_pitch(const struct song_step *st, uint32_t clock, unsigned pitch)
{
    unsigned count, i = song_find(st->song, st->track, clock);
    const struct song_event *e = song_track(st->song, st->track, &count);
    for (; i < count && song_event_start(&e[i]) == clock; ++i)
        if (song_is_note(&e[i]) && e[i].a == pitch) return 1;
    return 0;
}
SONG_SMALL static int target(const struct song_step *st, unsigned pitch, const struct shift *m)
{
    if (m->pads) {
        unsigned pad = 0;
        while (pad < m->count && m->pads[pad] != pitch) ++pad;
        int next = (int)pad + m->steps;
        return pad == m->count || next < 0 || next >= (int)m->count ? -1 : m->pads[next];
    }
    return m->octaves ? (int)pitch + m->steps * 12 : (int)song_scale_step(st->song, pitch, m->steps);
}
/* The notes at pool indices index[0..n), all starting at `clock`: highest
 * first going up and lowest first going down. The picked pitches follow
 * each note that moves, or with `kept` collect where every note ends up. */
SONG_SMALL static unsigned move(struct song_step *st, unsigned *index, unsigned n, uint32_t clock, const struct shift *m,
                                uint32_t *kept)
{
    unsigned moved = 0;
    for (unsigned a = 0; a < n; ++a) /* by pitch */
        for (unsigned b = a + 1; b < n; ++b)
            if (st->song->event[index[b]].a < st->song->event[index[a]].a) {
                unsigned t = index[a]; index[a] = index[b]; index[b] = t;
            }
    for (unsigned k = 0; k < n; ++k) {
        unsigned at = index[m->steps > 0 ? n - 1 - k : k];
        struct song_event e = st->song->event[at];
        int to = target(st, e.a, m);
        if (to >= 0 && to <= 127 && to != e.a && !find_pitch(st, clock, (unsigned)to)) {
            if (st->picked && !kept) {
                st->pick[e.a / 32U] &= ~(1U << (e.a % 32U));
                st->pick[to / 32] |= 1U << (to % 32);
            }
            if (st->ids) step_repitch(st, clock, e.a, (unsigned)to); /* a moved note stays selected (0.6.8) */
            e.a = (uint8_t)to;
            step_change(st, at, &e);
            ++moved;
        }
        if (kept) kept[e.a / 32U] |= 1U << (e.a % 32U);
    }
    return moved;
}
/* A sweep, one clock's notes at a time (up to a chord's worth each). */
SONG_SMALL static int move_swept(struct song_step *st, const struct shift *m)
{
    uint32_t from, to, kept[4] = {0};
    unsigned base = st->song->first[st->track], i, end, moved = 0;
    if (step_too_big(st)) return STEP_TOO_BIG;
    step_range(st, &from, &to);
    i = song_find(st->song, st->track, from);
    end = song_find(st->song, st->track, to);
    while (i < end) {
        uint32_t clock = song_event_start(&st->song->event[base + i]);
        unsigned index[STEP_CHORD], n = 0;
        for (; i < end && song_event_start(&st->song->event[base + i]) == clock; ++i)
            if (n < STEP_CHORD && step_chosen(st, &st->song->event[base + i])) index[n++] = base + i;
        moved += move(st, index, n, clock, m, kept);
    }
    if (st->picked) memcpy(st->pick, kept, sizeof kept);
    return (int)moved;
}
SONG_SMALL static int shift_pitch(struct song_step *st, const struct shift *m)
{
    uint32_t from, to;
    if (step_range(st, &from, &to)) return move_swept(st, m);
    unsigned index[STEP_CHORD], n = step_selection(st, index), moved = move(st, index, n, st->pointer, m, NULL);
    if (moved) step_preview(st);
    return (int)moved;
}
SONG_SMALL int step_transpose(struct song_step *st, int steps, unsigned octaves)
{
    const struct shift m = {steps, octaves, 0, NULL};
    return shift_pitch(st, &m);
}
SONG_SMALL int step_transpose_pads(struct song_step *st, const uint8_t *pads, unsigned count, int steps)
{
    const struct shift m = {steps, 0, count, pads};
    return shift_pitch(st, &m);
}
#endif
