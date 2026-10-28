/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Edits beyond adding and removing notes: changing a note in place and
 * shifting everything after a point in time (both journaled), and removing
 * conductor changes (undo sees those through the maps). */
#include "undo.h"
#include "song_config.h"
#include <string.h>
#if SONG_UI

/* Index is within the track. A new start clock re-sorts the note. */
SONG_SMALL int song_set_event(struct song *s, unsigned track, unsigned index, const struct song_event *e)
{
    unsigned count;
    const struct song_event *t = song_track(s, track, &count);
    if (index >= count) return -1;
    if (song_event_start(&t[index]) != song_event_start(e)) {
        if (song_remove(s, track, index)) return -1;
        return song_add_event(s, track, e);
    }
    if (s->undo) song_undo_record(s->undo, SONG_UNDO_DEL, track, &t[index], 0);
    s->event[s->first[track] + index] = *e;
    if (s->undo) song_undo_record(s->undo, SONG_UNDO_ADD, track, e, 0);
    ++s->revision;
    return 0;
}

SONG_SMALL int song_remove_tempo(struct song *s, uint32_t clock)
{
    for (unsigned k = 1; k < s->tempos; ++k)
        if (s->tempo[k].clock == clock) {
            memmove(&s->tempo[k], &s->tempo[k + 1], (s->tempos - k - 1U) * sizeof s->tempo[0]);
            --s->tempos;
            song_tempo_refresh(s);
            ++s->revision;
            return 0;
        }
    return -1;
}
SONG_SMALL int song_remove_meter(struct song *s, unsigned bar)
{
    for (unsigned m = 1; m < s->meters; ++m)
        if (s->meter[m].bar == bar) {
            memmove(&s->meter[m], &s->meter[m + 1], (s->meters - m - 1U) * sizeof s->meter[0]);
            --s->meters;
            ++s->revision;
            return 0;
        }
    return -1;
}

/* Every event at or after `from` moves by delta, on all tracks. Moving left
 * needs the span it moves into to be empty, so order is kept. */
SONG_SMALL int song_shift(struct song *s, uint32_t from, int32_t delta)
{
    if (!delta) return 0;
    if (delta < 0 && (uint32_t)-delta > from) return -1;
    for (unsigned t = 0; t < SONG_TRACKS; ++t) {
        unsigned count, i = song_find(s, t, delta < 0 ? from + (uint32_t)delta : from);
        const struct song_event *e = song_track(s, t, &count);
        if (delta < 0 && i < count && song_event_start(&e[i]) < from) return -1;
        if (delta > 0 && count && song_event_start(&e[count - 1]) >= from &&
            song_event_start(&e[count - 1]) + (uint32_t)delta > SONG_MAX_CLOCK) return -1;
    }
    for (unsigned t = 0; t < SONG_TRACKS; ++t) {
        unsigned count, i = song_find(s, t, from);
        song_track(s, t, &count);
        for (struct song_event *e = s->event + s->first[t] + i; i < count; ++i, ++e)
            e->at = (e->at & 0xff000000U) | (song_event_start(e) + (uint32_t)delta);
    }
    ++s->revision;
    if (s->undo) {
        const struct song_event at = {from, 0, 0, 0};
        song_undo_record(s->undo, SONG_UNDO_SHIFT, 0, &at, (uint32_t)delta);
    }
    return 0;
}
#endif
