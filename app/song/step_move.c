/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Step entry's grid, MOVE and NUDGE (0.6.7). PRESETS steps from line to
 * line of the grid of the current length, so a walk at 1/64 and then a
 * half note lands on the half bar, where it used to keep the 64ths' offset.
 * Entering, rests, ties and repeats still go on by the length from wherever
 * the pointer is. MOVE (SHIFT+K1) and NUDGE (SHIFT+K2) take the selected
 * notes, the pointer's or a sweep's, by the length or by 5 clocks. The
 * pointer is the selection, so it goes with them, a sweep's anchor too,
 * and the pick narrows to their pitches so a note that stays where they
 * land doesn't join them. A turn that would let one in is refused instead,
 * as is one past the start, a clip's end, or where entering could reach.
 * See step.h. */
#include "step.h"
#include "song_config.h"
#include <string.h>
#if SONG_UI

/* The grid line at or before `clock`, and the one after it in *next: the
 * step length counted from its bar's line, never past the bar's end. */
SONG_SMALL static uint32_t line(const struct song_step *st, uint32_t clock, uint32_t *next)
{
    uint32_t start, length, step = step_clocks(st), at;
    song_bar_at(st->song, clock, &start, &length);
    if (step > length) step = length;
    at = start + (clock - start) / step * step;
    *next = at + step < start + length ? at + step : start + length;
    return at;
}
SONG_SMALL uint32_t step_grid_move(const struct song_step *st, uint32_t clock, int steps)
{
    uint32_t end = song_end(st->song), next;
    if (clock > end) clock = end;
    for (; steps > 0 && clock < end; --steps) {
        line(st, clock, &next);
        clock = next < end ? next : end;
    }
    for (; steps < 0 && clock; ++steps) clock = line(st, clock - 1U, &next);
    return clock;
}
SONG_SMALL int step_offset(const struct song_step *st)
{
    uint32_t next, at = line(st, st->pointer, &next), before = st->pointer - at, after = next - st->pointer;
    return before <= after ? (int)before : -(int)after;
}

static uint32_t moved_by(uint32_t clock, int32_t clocks) { return clock + (uint32_t)clocks; }
static uint32_t inside(int64_t clock, uint32_t end) { return clock < 0 ? 0 : clock > end ? end : (uint32_t)clock; }
SONG_SMALL static int has(const uint32_t *pitches, unsigned pitch) { return pitches[pitch / 32U] >> (pitch % 32U) & 1U; }

/* What a turn moves: the chosen notes in [from, to), how many, the first
 * and last of their starts, and their pitches. */
struct turn { uint32_t from, to, first, last, pitches[4]; unsigned n; };
SONG_SMALL static void selected(const struct song_step *st, struct turn *t)
{
    unsigned count, i;
    const struct song_event *e = song_track(st->song, st->track, &count);
    if (!step_range(st, &t->from, &t->to)) { t->from = st->pointer; t->to = st->pointer + 1U; }
    memset(t->pitches, 0, sizeof t->pitches);
    t->n = t->last = 0;
    t->first = ~0U;
    for (i = song_find(st->song, st->track, t->from); i < count && song_event_start(&e[i]) < t->to; ++i) {
        uint32_t at = song_event_start(&e[i]);
        if (!step_chosen(st, &e[i])) continue;
        ++t->n;
        if (at < t->first) t->first = at;
        t->last = at;
        t->pitches[e[i].a / 32U] |= 1U << (e[i].a % 32U);
    }
}
/* A note of a pitch in `pick` that stays, in [lo, hi] outside where the
 * notes were: it would be selected with them after the turn. */
SONG_SMALL static int in_the_way(const struct song_step *st, const struct turn *t, const uint32_t *pick, uint32_t lo, uint32_t hi)
{
    unsigned count, i;
    const struct song_event *e = song_track(st->song, st->track, &count);
    for (i = song_find(st->song, st->track, lo); i < count && song_event_start(&e[i]) <= hi; ++i) {
        uint32_t at = song_event_start(&e[i]);
        if (song_is_note(&e[i]) && (at < t->from || at >= t->to) && has(pick, e[i].a)) return 1;
    }
    return 0;
}
SONG_SMALL int step_nudge(struct song_step *st, int32_t clocks, struct song_player *p)
{
    struct song *s = st->song;
    struct turn t;
    uint32_t pick[4], end = song_end(s), bars = s->bars, pointer, anchor;
    unsigned count, i;
    const struct song_event *e;
    if (step_too_big(st)) return STEP_TOO_BIG;
    selected(st, &t);
    if (!t.n || !clocks) return -1;
    /* A clip keeps its notes inside its length. On the song a note goes as
     * far as the pointer can, the end, and the song grows to hold it there
     * as it does for a note entered at the end. */
    if ((int64_t)t.first + clocks < 0) return STEP_START;
    if (s->open && (int64_t)t.last + clocks >= s->clip_length[s->open - SONG_TRACKS]) return STEP_END;
    if ((int64_t)t.last + clocks > end) return STEP_END;
    if (!s->open)
        while (moved_by(t.last, clocks) >= song_bar_clock(s, bars) && bars < SONG_MAX_BARS) ++bars;
    end = s->open ? end : song_bar_clock(s, bars);
    pointer = inside((int64_t)st->pointer + clocks, end);
    anchor = inside((int64_t)st->anchor + clocks, end);
    memcpy(pick, st->picked ? st->pick : t.pitches, sizeof pick);
    if (st->ranged ? in_the_way(st, &t, pick, anchor < pointer ? anchor : pointer, anchor < pointer ? pointer : anchor)
                   : in_the_way(st, &t, pick, pointer, pointer))
        return STEP_IN_THE_WAY;
    /* Every chosen note in place, the track sorted again after. */
    e = song_track(s, st->track, &count);
    for (i = song_find(s, st->track, t.from); i < count && song_event_start(&e[i]) < t.to; ++i) {
        struct song_event x = e[i];
        if (!step_chosen(st, &x)) continue;
        if (p) song_player_moved(p, st->track, x.a, song_event_start(&x), moved_by(song_event_start(&x), clocks));
        x.at = (x.at & ~0xffffffU) | moved_by(song_event_start(&x), clocks);
        song_retouch(s, st->track, i, &x);
    }
    song_sort_track(s, st->track);
    s->bars = (uint16_t)bars;
    st->pointer = pointer;
    if (st->ranged) st->anchor = anchor;
    memcpy(st->pick, pick, sizeof pick);
    st->picked = 1;
    st->tie_ok = 0;
    return 0;
}
#endif
