/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Time jobs: stretch, and normalize over the player's capture. See jobs.h. */
#include "jobs.h"
#include "player.h"
#include "song_config.h"
#include <stdlib.h>
#include <string.h>
#if SONG_UI

SONG_SMALL static uint32_t scale(uint32_t from, uint32_t clock, unsigned rate)
{
    return from + (uint32_t)(((uint64_t)(clock - from) * rate + 50U) / 100U);
}
/* Tempo changes and markers: inside the range they scale, after it they
 * move by delta. Two tempo changes that meet keep the later one. */
SONG_SMALL static void conductor(struct song *s, uint32_t from, uint32_t to, unsigned rate, int32_t delta)
{
    unsigned kept = 1;
    for (unsigned k = 1; k < s->tempos; ++k) {
        uint32_t c = s->tempo[k].clock;
        c = c >= to ? c + (uint32_t)delta : c > from ? scale(from, c, rate) : c;
        if (c == s->tempo[kept - 1U].clock) --kept;
        s->tempo[kept] = (struct song_tempo){c, s->tempo[k].bpm100};
        ++kept;
    }
    s->tempos = (uint8_t)kept;
    for (unsigned i = 0; i < s->markers; ++i) {
        uint32_t c = s->marker[i].clock;
        s->marker[i].clock = c >= to ? c + (uint32_t)delta : c > from ? scale(from, c, rate) : c;
    }
    if (s->loop_start >= to) s->loop_start += (uint32_t)delta;
    if (s->loop_end >= to) s->loop_end += (uint32_t)delta;
    song_tempo_refresh(s);
}
/* Whole bars at a bar line with no time signature change inside the range
 * move the later signatures by as many bars. The bar count follows. */
SONG_SMALL static void bars(struct song *s, uint32_t from, uint32_t to, int32_t delta)
{
    uint32_t start, size, magnitude = (uint32_t)(delta < 0 ? -delta : delta);
    unsigned bar = song_bar_at(s, to, &start, &size), inside = 0, whole = start == to && magnitude % size == 0;
    for (unsigned m = 1; m < s->meters; ++m) inside |= song_bar_clock(s, s->meter[m].bar) > from && song_bar_clock(s, s->meter[m].bar) < to;
    unsigned moved = whole ? magnitude / size : 0;
    for (unsigned m = 1; moved && !inside && m < s->meters; ++m)
        if (s->meter[m].bar >= bar) s->meter[m].bar = (uint16_t)(delta > 0 ? s->meter[m].bar + moved : s->meter[m].bar - moved);
    unsigned change = moved ? moved : delta > 0 ? (magnitude + SONG_WHOLE - 1U) / SONG_WHOLE : magnitude / SONG_WHOLE;
    s->bars = (uint16_t)(delta > 0 ? s->bars + change : s->bars > change + 1U ? s->bars - change : 1U);
}
SONG_SMALL int song_job_stretch(struct song *s, const struct song_range *r, unsigned rate)
{
    if (r->from >= r->to || rate < 50 || rate > 200 || !r->tracks) return -1;
    uint32_t length = r->to - r->from, cut = r->from + (uint32_t)(((uint64_t)length * rate + 50U) / 100U);
    int32_t delta = (int32_t)(cut - r->from) - (int32_t)length;
    for (unsigned t = 0; delta < 0 && t < SONG_TRACKS; ++t) /* what closes must be empty on the tracks left out */
        if (!(r->tracks >> t & 1U) && song_find(s, t, cut) < song_find(s, t, r->to)) return -1;
    if (delta > 0 && (s->bars + ((uint32_t)delta + SONG_WHOLE - 1U) / SONG_WHOLE > SONG_MAX_BARS || song_shift(s, r->to, delta)))
        return -1;
    int changed = 0;
    for (unsigned t = 0; t < SONG_TRACKS; ++t) {
        if (!(r->tracks >> t & 1U)) continue;
        for (unsigned i = song_find(s, t, r->from), last = song_find(s, t, r->to); i < last; ++i) {
            struct song_event e = s->event[s->first[t] + i];
            e.at = (e.at & ~(SONG_FRESH | 0xffffffU)) | scale(r->from, song_event_start(&e), rate);
            if (song_is_block(&e)) song_block_resize(&e, (uint32_t)(((uint64_t)song_block_length(&e) * rate + 50U) / 100U));
            else if (e.length) {
                uint32_t n = ((uint32_t)e.length * rate + 50U) / 100U;
                e.length = (uint16_t)(n < 1 ? 1 : n > 0xffff ? 0xffff : n);
            }
            song_retouch(s, t, i, &e); /* the order holds: the mapping only grows */
            ++changed;
        }
    }
    if (delta < 0 && song_shift(s, r->to, delta)) return -1; /* can't happen: [cut, to) is empty now */
    if (delta) { conductor(s, r->from, r->to, rate, delta); bars(s, r->from, r->to, delta); }
    return changed;
}

/* Normalize: the capture replaces the tracks' notes. Automation and the FX
 * lane stay. Its 36 KB come from the heap only while it runs. */
SONG_SMALL static int normalize(struct song *s, unsigned tracks, const struct song_player *live, struct song_event *baked,
                                uint8_t *baked_track)
{
    unsigned notes = 0;
    for (unsigned t = 0; t < SONG_TRACKS; ++t)
        if (tracks >> t & 1U)
            for (unsigned i = 0; i < s->count[t]; ++i) notes += song_is_note(&s->event[s->first[t] + i]);
    int n = tracks & 0xffU ? song_player_bake(live, s, tracks, baked, baked_track, SONG_EVENTS) : -1;
    if (n < 0 || s->events - notes + (unsigned)n > SONG_EVENTS) return -1;
    for (unsigned t = 0; t < SONG_TRACKS; ++t)
        if (tracks >> t & 1U) {
            for (unsigned i = s->count[t]; i-- > 0;)
                if (song_is_note(&s->event[s->first[t] + i])) (void)song_remove(s, t, i);
            if (s->swing) s->swing_tracks &= (uint8_t)~(1U << t);
            s->humanize_time[t] = s->humanize_velocity[t] = 0;
        }
    for (int i = 0; i < n; ++i) (void)song_add_event(s, baked_track[i], &baked[i]);
    return n;
}
SONG_SMALL int song_job_normalize(struct song *s, unsigned tracks, const struct song_player *live)
{
    struct song_event *baked = malloc(SONG_EVENTS * sizeof *baked);
    uint8_t *baked_track = malloc(SONG_EVENTS);
    int n = baked && baked_track ? normalize(s, tracks, live, baked, baked_track) : -1;
    free(baked);
    free(baked_track);
    return n;
}
#endif
