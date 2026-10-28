/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Mangle jobs. See mangle.h. */
#include "mangle.h"
#include "song_config.h"
#if SONG_UI
#include "undo.h"
enum { SLICE = SONG_PPQ / 4 };

static uint32_t mix(uint32_t x)
{
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU;
    return x ^ x >> 16;
}
/* New starts for the notes of one track inside the selection, then the
 * range sorted again. Every event stays inside [from, to), so the track's
 * range in the pool holds. */
typedef uint32_t (*song_retime)(const struct song_event *e, uint32_t from, uint32_t to);
SONG_SMALL static int retime(struct song *s, unsigned tracks, uint32_t from, uint32_t to, song_retime fn)
{
    int moved = 0;
    for (unsigned t = 0; t < SONG_TRACKS; ++t) {
        if (!(tracks >> t & 1U)) continue;
        struct song_event *e = s->event + s->first[t];
        unsigned first = song_find(s, t, from), last = song_find(s, t, to);
        for (unsigned i = first; i < last; ++i) {
            if (!song_is_note(&e[i])) continue;
            uint32_t at = fn(&e[i], from, to);
            if (at == song_event_start(&e[i])) continue;
            if (s->undo) song_undo_record(s->undo, SONG_UNDO_DEL, t, &e[i], 0);
            e[i].at = (e[i].at & 0xff000000U) | at;
            if (s->undo) song_undo_record(s->undo, SONG_UNDO_ADD, t, &e[i], 0);
            ++moved;
        }
        for (unsigned i = first + 1U; i < last; ++i) { /* insertion sort, so equal starts keep their order */
            struct song_event x = e[i];
            unsigned j = i;
            for (; j > first && song_event_start(&e[j - 1U]) > song_event_start(&x); --j) e[j] = e[j - 1U];
            e[j] = x;
        }
    }
    ++s->revision;
    return moved;
}

static uint16_t order[SONG_MANGLE_SLICES];
static uint32_t scrambled(const struct song_event *e, uint32_t from, uint32_t to)
{
    uint32_t k = (song_event_start(e) - from) / SLICE, at = from + order[k] * SLICE + (song_event_start(e) - from) % SLICE;
    return at < to ? at : to - 1U;
}
SONG_SMALL int song_scramble(struct song *s, unsigned tracks, uint32_t from, uint32_t to, uint32_t seed)
{
    uint32_t n = (to - from + SLICE - 1U) / SLICE;
    if (from >= to || n > SONG_MANGLE_SLICES) return -1;
    for (uint32_t i = 0; i < n; ++i) order[i] = (uint16_t)i;
    for (uint32_t i = n; i > 1; --i) { /* Fisher-Yates */
        uint32_t j = mix(seed ^ i * 0x9e3779b9U) % i;
        uint16_t x = order[i - 1U]; order[i - 1U] = order[j]; order[j] = x;
    }
    return retime(s, tracks, from, to, scrambled);
}
static uint32_t mirrored(const struct song_event *e, uint32_t from, uint32_t to)
{
    uint32_t end = song_event_start(e) + e->length;
    return end >= to ? from : from + to - end;
}
SONG_SMALL int song_reverse(struct song *s, unsigned tracks, uint32_t from, uint32_t to)
{
    return from < to ? retime(s, tracks, from, to, mirrored) : -1;
}

/* Each note goes to the selected track its own hash picks. The hash
 * doesn't depend on the track, so a note visited again stays put. */
SONG_SMALL int song_shuffle_tracks(struct song *s, unsigned tracks, uint32_t from, uint32_t to, uint32_t seed)
{
    uint8_t list[SONG_TRACKS];
    unsigned n = 0;
    int moved = 0;
    for (unsigned t = 0; t < SONG_TRACKS; ++t) if (tracks >> t & 1U) list[n++] = (uint8_t)t;
    if (from >= to || n < 2) return -1;
    for (unsigned k = 0; k < n; ++k) {
        unsigned t = list[k], first = song_find(s, t, from);
        for (unsigned i = song_find(s, t, to); i-- > first;) {
            const struct song_event e = s->event[s->first[t] + i];
            if (!song_is_note(&e)) continue;
            unsigned to_track = list[mix(seed ^ mix(song_event_start(&e) ^ (uint32_t)e.a << 24 ^ (uint32_t)e.length << 8)) % n];
            if (to_track == t || song_remove(s, t, i)) continue;
            (void)song_add_event(s, to_track, &e);
            ++moved;
        }
    }
    return moved;
}
#endif
