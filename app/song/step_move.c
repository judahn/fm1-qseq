/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Step entry's grid, MOVE and NUDGE (0.6.7). PRESETS steps from line to
 * line of the grid of the current length, so a walk at 1/64 and then a
 * half note lands on the half bar, where it used to keep the 64ths' offset.
 * Entering, rests, ties and repeats still go on by the length from wherever
 * the pointer is. MOVE (SHIFT+K1) and NUDGE (SHIFT+K2) take the selected
 * notes, the pointer's or a sweep's, by the length or by 5 clocks, and the
 * pointer goes with them, a sweep's anchor too. From the first turn the
 * moved notes are the selection by identity (0.6.8, step_ids.c), so other
 * notes they pass stay out of it. One they land on, of their pitch at their
 * clock, is replaced, and put back if the run moves on. In 0.6.7 a turn
 * that would have brought another note into the selection was refused. A
 * turn past the start, a clip's end, or where entering could reach still
 * is. See step.h. */
#include "step.h"
#include "song_config.h"
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

/* What a turn moves: the selected notes in [from, to), how many, and the
 * first and last of their starts. */
struct turn { uint32_t from, to, first, last; unsigned n; };
SONG_SMALL static void selected(const struct song_step *st, struct turn *t)
{
    unsigned count, i;
    const struct song_event *e = song_track(st->song, st->track, &count);
    if (!step_range(st, &t->from, &t->to)) { t->from = st->pointer; t->to = st->pointer + 1U; }
    t->n = t->last = 0;
    t->first = ~0U;
    for (i = song_find(st->song, st->track, t->from); i < count && song_event_start(&e[i]) < t->to; ++i) {
        uint32_t at = song_event_start(&e[i]);
        if (!step_chosen(st, &e[i])) continue;
        ++t->n;
        if (at < t->first) t->first = at;
        t->last = at;
    }
}
/* Before a turn: how many notes it lands on, at a moving note's new start
 * and of its pitch, not moving themselves. Two moving notes that share a
 * place count its notes twice, which only errs large. */
SONG_SMALL static unsigned landed(const struct song_step *st, const struct turn *t, int32_t clocks)
{
    unsigned count, i, j, n = 0;
    const struct song_event *e = song_track(st->song, st->track, &count);
    for (i = song_find(st->song, st->track, t->from); i < count && song_event_start(&e[i]) < t->to; ++i) {
        uint32_t at = moved_by(song_event_start(&e[i]), clocks);
        if (!step_chosen(st, &e[i])) continue;
        for (j = song_find(st->song, st->track, at); j < count && song_event_start(&e[j]) == at; ++j)
            n += song_is_note(&e[j]) && e[j].a == e[i].a && !(at >= t->from && at < t->to && step_chosen(st, &e[j]));
    }
    return n;
}
/* Once the list holds where the turn's notes go: a note is one of them if
 * `clocks` on it is one of those places. */
SONG_SMALL static int moving(const struct song_step *st, const struct song_event *e, int32_t clocks)
{
    int64_t at = (int64_t)song_event_start(e) + clocks;
    return song_is_note(e) && at >= 0 && at <= 0xffffff && step_has_id(st, step_id_of((uint32_t)at, e->a));
}
SONG_SMALL int step_nudge(struct song_step *st, int32_t clocks, unsigned more, struct song_player *p)
{
    struct song *s = st->song;
    struct turn t;
    uint32_t end = song_end(s), bars = s->bars, pointer, anchor;
    unsigned count, i, k;
    const struct song_event *e;
    if (!more) st->covers = 0; /* a new run: what the last one landed on stays replaced */
    selected(st, &t);
    if (!t.n || !clocks) return -1;
    /* A clip keeps its notes inside its length. On the song a note goes as
     * far as the pointer can, the end, and the song grows to hold it there
     * as it does for a note entered at the end. */
    if ((int64_t)t.first + clocks < 0) return STEP_START;
    if (s->open && (int64_t)t.last + clocks >= s->clip_length[s->open - SONG_TRACKS]) return STEP_END;
    if ((int64_t)t.last + clocks > end) return STEP_END;
    if (t.n > STEP_IDS || (t.n + landed(st, &t, clocks)) * 24U + 512U + sizeof(struct song_maps) > SONG_UNDO_BYTES)
        return STEP_TOO_BIG;
    if (!s->open)
        while (moved_by(t.last, clocks) >= song_bar_clock(s, bars) && bars < SONG_MAX_BARS) ++bars;
    end = s->open ? end : song_bar_clock(s, bars);
    pointer = inside((int64_t)st->pointer + clocks, end);
    anchor = inside((int64_t)st->anchor + clocks, end);
    step_take(st, t.from, t.to, clocks);
    /* Landed on: each place once, every note there of its pitch that stays. */
    for (k = 0; k < st->ids; ++k) {
        uint32_t at = st->id[k] >> 8;
        if (k && st->id[k] == st->id[k - 1U]) continue;
        e = song_track(s, st->track, &count);
        for (i = song_find(s, st->track, at); i < count && song_event_start(&e[i]) == at;)
            if (song_is_note(&e[i]) && e[i].a == (st->id[k] & 255U) && !moving(st, &e[i], clocks)) {
                step_cover(st, i);
                e = song_track(s, st->track, &count);
            } else ++i;
    }
    /* The moving notes in place, the track sorted again after, then what
     * the run covered and has left put back. */
    e = song_track(s, st->track, &count);
    for (i = song_find(s, st->track, t.from); i < count && song_event_start(&e[i]) < t.to; ++i) {
        struct song_event x = e[i];
        if (!moving(st, &x, clocks)) continue;
        if (p) song_player_moved(p, st->track, x.a, song_event_start(&x), moved_by(song_event_start(&x), clocks));
        x.at = (x.at & ~0xffffffU) | moved_by(song_event_start(&x), clocks);
        song_retouch(s, st->track, i, &x);
    }
    song_sort_track(s, st->track);
    step_uncover(st);
    s->bars = (uint16_t)bars;
    st->pointer = pointer;
    if (st->ranged) st->anchor = anchor;
    st->tie_ok = 0;
    return 0;
}
#endif
