/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Step entry's selection by identity (0.6.8). After a turn of MOVE or
 * NUDGE the notes it moved are the selection, wherever they go, so they
 * pass over and among other notes without taking them in. They are known
 * by start and pitch: a moved note landing where another of its pitch
 * starts replaces it, so no other note ever shares one, and moved notes
 * that share one between them (a sampler pad at two TUNEs, or the same
 * note twice) are all selected anyway. The list is kept sorted, so a turn
 * moves it whole in place and a lookup is a binary search. Anything that
 * changes the selection ends it (step_forget, step.c). The notes a run
 * lands on wait in `cover`, up to STEP_COVERS, and go back as the moved
 * notes leave their place. Past that a note landed on is replaced for good,
 * which undo still takes back. See step.h. */
#include "step.h"
#include "song_config.h"
#if SONG_UI

SONG_SMALL uint32_t step_id_of(uint32_t start, unsigned pitch) { return start << 8 | pitch; }
SONG_SMALL static uint32_t id_of(const struct song_event *e) { return step_id_of(song_event_start(e), e->a); }
/* The first entry not below `id`. */
SONG_SMALL static unsigned lower(const struct song_step *st, uint32_t id)
{
    unsigned low = 0, high = st->ids;
    while (low < high) {
        unsigned mid = (low + high) / 2U;
        if (st->id[mid] < id) low = mid + 1U;
        else high = mid;
    }
    return low;
}
SONG_SMALL int step_has_id(const struct song_step *st, uint32_t id)
{
    unsigned at = lower(st, id);
    return at < st->ids && st->id[at] == id;
}
SONG_SMALL void step_forget(struct song_step *st) { st->ids = st->covers = 0; }
/* A moved note's new pitch at `clock` (the pitch knob): its entry goes to
 * its sorted place. */
SONG_SMALL void step_repitch(struct song_step *st, uint32_t clock, unsigned from, unsigned to)
{
    uint32_t now = step_id_of(clock, to);
    unsigned i = lower(st, step_id_of(clock, from));
    if (!step_has_id(st, step_id_of(clock, from))) return;
    for (; i > 0 && st->id[i - 1U] > now; --i) st->id[i] = st->id[i - 1U];
    for (; i + 1U < st->ids && st->id[i + 1U] < now; ++i) st->id[i] = st->id[i + 1U];
    st->id[i] = now;
}
/* A turn's notes as they will be, `clocks` on: the list held already moves
 * whole, staying sorted, or else the notes selected in [from, to) make it.
 * How many. The caller knows they fit. */
SONG_SMALL unsigned step_take(struct song_step *st, uint32_t from, uint32_t to, int32_t clocks)
{
    unsigned count, i, n = 0;
    const struct song_event *e = song_track(st->song, st->track, &count);
    if (st->ids) {
        for (i = 0; i < st->ids; ++i) st->id[i] += (uint32_t)clocks << 8;
        return st->ids;
    }
    for (i = song_find(st->song, st->track, from); i < count && song_event_start(&e[i]) < to; ++i) {
        if (!step_chosen(st, &e[i]) || n == STEP_IDS) continue;
        uint32_t id = step_id_of(song_event_start(&e[i]) + (uint32_t)clocks, e[i].a);
        unsigned k = n++;
        for (; k > 0 && st->id[k - 1U] > id; --k) st->id[k] = st->id[k - 1U];
        st->id[k] = id;
    }
    st->ids = (uint16_t)n;
    return n;
}
/* The note at track index `index` is landed on: it goes, kept to put back
 * while there is room. */
SONG_SMALL void step_cover(struct song_step *st, unsigned index)
{
    if (st->covers < STEP_COVERS) st->cover[st->covers++] = st->song->event[st->song->first[st->track] + index];
    (void)song_remove(st->song, st->track, index);
}
/* The notes covered whose place no moved note holds now go back. */
SONG_SMALL void step_uncover(struct song_step *st)
{
    for (unsigned i = st->covers; i-- > 0;) {
        if (step_has_id(st, id_of(&st->cover[i]))) continue;
        (void)song_add_event(st->song, st->track, &st->cover[i]);
        st->cover[i] = st->cover[--st->covers];
    }
}
#endif
