/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Song storage and editing. Tracks, then the clips' parts, sit back to back
 * in one event pool, each sorted by start clock, so a part is one run. */
#include "song.h"
#include "song_config.h"
#include <string.h>
#if SONG_UI /* edits are journaled for undo on the editing pages */
#include "undo.h"
#include "record.h"
#define JOURNAL(s, op, track, e) do { if ((s)->undo) song_undo_record((s)->undo, op, song_owner(s, track), e, 0); } while (0)
#else
#define JOURNAL(s, op, track, e) ((void)0)
#endif

SONG_SMALL void song_clear(struct song *s, unsigned bars, unsigned bpm100)
{
    memset(s, 0, sizeof *s);
    s->bars = (uint16_t)(bars < 1 ? 1 : bars > SONG_MAX_BARS ? SONG_MAX_BARS : bars);
    s->tempo[0].bpm100 = (uint16_t)(bpm100 < SONG_TEMPO_MIN || bpm100 > SONG_TEMPO_MAX ? 12000 : bpm100);
    s->tempos = 1;
    s->meter[0] = (struct song_meter){0, 4, 4};
    s->meters = 1;
    s->loop_end = song_bar_clock(s, s->bars < 4 ? s->bars : 4);
    s->swing_tracks = 0xff;
#if SONG_UI
    s->launch_q = 2; /* a bar */
    s->rec_mode = SONG_REC_OVERDUB; /* 0.4.0: REC's live mode, never STEP */
#endif
    song_tempo_refresh(s);
}

SONG_SMALL const struct song_event *song_track(const struct song *s, unsigned track, unsigned *count)
{
    if (track >= SONG_PARTS) { *count = 0; return s->event; }
    *count = s->count[track];
    return s->event + s->first[track];
}

SONG_SMALL unsigned song_find(const struct song *s, unsigned track, uint32_t clock)
{
    unsigned count, low = 0, high;
    const struct song_event *e = song_track(s, track, &count);
    high = count;
    while (low < high) {
        unsigned mid = (low + high) / 2;
        if (song_event_start(&e[mid]) < clock) low = mid + 1;
        else high = mid;
    }
    return low;
}

SONG_SMALL int song_add_event(struct song *s, unsigned track, const struct song_event *e)
{
    if (track >= SONG_PARTS || s->events >= SONG_EVENTS) return -1;
    uint32_t clock = song_event_start(e);
    /* After any notes already at this clock, so chords keep their entry order. */
    unsigned at = s->first[track] + song_find(s, track, clock + 1);
    memmove(&s->event[at + 1], &s->event[at], (s->events - at) * sizeof s->event[0]);
    s->event[at] = *e;
    ++s->count[track];
    ++s->events;
    for (unsigned t = track + 1; t < SONG_PARTS; ++t) ++s->first[t];
    ++s->revision;
    JOURNAL(s, SONG_UNDO_ADD, track, e);
    return 0;
}
SONG_SMALL int song_add_note(struct song *s, unsigned track, uint32_t clock, unsigned pitch,
                             unsigned velocity, unsigned length)
{
    if (clock > SONG_MAX_CLOCK || pitch > 127 || !velocity || velocity > 127 || !length || length > 0xffff)
        return -1;
    const struct song_event e = {clock | (uint32_t)SONG_NOTE << 24, (uint8_t)pitch, (uint8_t)velocity,
                                 (uint16_t)length};
    return song_add_event(s, track, &e);
}

SONG_SMALL int song_remove(struct song *s, unsigned track, unsigned index)
{
    if (track >= SONG_PARTS || index >= s->count[track]) return -1;
    unsigned at = s->first[track] + index;
    JOURNAL(s, SONG_UNDO_DEL, track, &s->event[at]);
    memmove(&s->event[at], &s->event[at + 1], (s->events - at - 1U) * sizeof s->event[0]);
    --s->count[track];
    --s->events;
    for (unsigned t = track + 1; t < SONG_PARTS; ++t) --s->first[t];
    ++s->revision;
    return 0;
}

SONG_SMALL int song_set_tempo(struct song *s, uint32_t clock, unsigned bpm100)
{
    if (clock > SONG_MAX_CLOCK || bpm100 < SONG_TEMPO_MIN || bpm100 > SONG_TEMPO_MAX) return -1;
    unsigned at = 0;
    while (at < s->tempos && s->tempo[at].clock < clock) ++at;
    if (at == s->tempos || s->tempo[at].clock != clock) {
        if (s->tempos >= SONG_TEMPOS) return -1;
        memmove(&s->tempo[at + 1], &s->tempo[at], (s->tempos - at) * sizeof s->tempo[0]);
        ++s->tempos;
    }
    s->tempo[at] = (struct song_tempo){clock, (uint16_t)bpm100};
    song_tempo_refresh(s);
    ++s->revision;
    return 0;
}

SONG_SMALL int song_set_meter(struct song *s, unsigned bar, unsigned beats, unsigned unit)
{
    /* The QY70's range: 1/16 to 16/16, 1/8 to 16/8, 1/4 to 8/4. */
    if (bar >= SONG_MAX_BARS || !beats || (unit != 4 && unit != 8 && unit != 16) ||
        beats > (unit == 4 ? 8U : 16U))
        return -1;
    unsigned at = 0;
    while (at < s->meters && s->meter[at].bar < bar) ++at;
    if (at == s->meters || s->meter[at].bar != bar) {
        if (s->meters >= SONG_METERS) return -1;
        memmove(&s->meter[at + 1], &s->meter[at], (s->meters - at) * sizeof s->meter[0]);
        ++s->meters;
    }
    s->meter[at] = (struct song_meter){(uint16_t)bar, (uint8_t)beats, (uint8_t)unit};
    ++s->revision;
    return 0;
}

SONG_SMALL int song_add_marker(struct song *s, uint32_t clock, const char *name)
{
    if (s->markers >= SONG_MARKERS || clock > SONG_MAX_CLOCK) return -1;
    unsigned at = 0;
    while (at < s->markers && s->marker[at].clock <= clock) ++at;
    memmove(&s->marker[at + 1], &s->marker[at], (s->markers - at) * sizeof s->marker[0]);
    struct song_marker *m = &s->marker[at];
    m->clock = clock;
    memset(m->name, 0, sizeof m->name);
    for (unsigned i = 0; name && name[i] && i < SONG_MARKER_NAME; ++i) m->name[i] = name[i];
    ++s->markers;
    ++s->revision;
    return 0;
}

#if SONG_UI
SONG_SMALL unsigned song_remove_marker(struct song *s, uint32_t clock)
{
    unsigned kept = 0;
    for (unsigned i = 0; i < s->markers; ++i)
        if (s->marker[i].clock != clock) s->marker[kept++] = s->marker[i];
    unsigned removed = s->markers - kept;
    if (removed) { s->markers = (uint8_t)kept; ++s->revision; }
    return removed;
}
#endif

SONG_SMALL int song_set_loop(struct song *s, uint32_t start, uint32_t end)
{
    if (start >= end || end > song_end(s)) return -1;
    s->loop_start = start;
    s->loop_end = end;
    return 0;
}
