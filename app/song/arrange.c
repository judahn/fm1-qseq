/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Arranging: copy, cut, paste, duplicate, and inserting or deleting time.
 * Note edits go through the journaled calls, and the caller's undo
 * transaction picks up marker, tempo and meter changes on its own.
 * Automation travels with the notes, starting from the values in effect
 * where the copy starts, so it sounds the same wherever it lands. */
#include "arrange.h"
#include "lanes.h"
#include "glitch.h"
#include "song_config.h"
#include <string.h>
#if SONG_UI

SONG_SMALL static unsigned in_range(const struct song *s, unsigned tracks, uint32_t from, uint32_t to)
{
    unsigned n = 0;
    for (unsigned t = 0; t < SONG_TRACKS; ++t)
        if (tracks >> t & 1U) n += song_find(s, t, to) - song_find(s, t, from);
    return n;
}
/* Stutters on the FX lane (track 1) when track 1 itself isn't in the mask. */
SONG_SMALL static unsigned fx_only(const struct song *s, unsigned tracks, uint32_t from, uint32_t to)
{
    unsigned n = 0, count, end = song_find(s, 0, to);
    const struct song_event *e = song_track(s, 0, &count);
    for (unsigned i = song_find(s, 0, from); !(tracks & 1U) && i < end; ++i) n += song_event_kind(&e[i]) == SONG_FX;
    return n;
}
SONG_SMALL static void clear(struct song *s, unsigned tracks, uint32_t from, uint32_t to)
{
    for (unsigned t = 0; t < SONG_TRACKS; ++t)
        if (tracks >> t & 1U)
            for (unsigned i = song_find(s, t, to), first = song_find(s, t, from); i-- > first;)
                (void)song_remove(s, t, i);
}
/* The copy of e moved to `at`, cut off at `limit`, without the take flag. */
SONG_SMALL static struct song_event moved(const struct song_event *e, uint32_t at, uint32_t limit)
{
    struct song_event c = *e;
    uint32_t start = song_event_start(e);
    c.at = (e->at & ~(SONG_FRESH | 0xffffffU)) | at;
    if (limit > start && start + c.length > limit) c.length = (uint16_t)(limit - start);
    return c;
}

/* A lane's value in effect at `from`, when no change sits right there. */
struct start_values { struct song *s; struct song_clip *clip; uint32_t from, to; unsigned count, fill; };
SONG_SMALL static void start_value(void *context, unsigned track, unsigned lane, unsigned value)
{
    struct start_values *v = context;
    if (song_lane_find(v->s, track, lane, v->from) >= 0 || song_lane_next(v->s, track, lane, v->from, -1) < 0) return;
    ++v->count;
    if (v->fill == 1) {
        v->clip->event[v->clip->count] = song_lane_event(0, lane, value);
        v->clip->track[v->clip->count++] = (uint8_t)track;
    } else if (v->fill == 2) {
        const struct song_event e = song_lane_event(v->to, lane, value);
        (void)song_add_event(v->s, track, &e);
    }
}
SONG_SMALL int song_copy(const struct song *s, struct song_clip *clip, unsigned tracks, uint32_t from, uint32_t to)
{
    struct start_values v = {(struct song *)s, clip, from, to, 0, 0};
    for (unsigned t = 0; t < SONG_TRACKS; ++t)
        if (tracks >> t & 1U) song_chase(s, t, from, start_value, &v);
    if (from >= to || !(tracks & 0xffU) ||
        in_range(s, tracks, from, to) + v.count + fx_only(s, tracks, from, to) > SONG_CLIP_EVENTS) return -1;
    clip->count = 0;
    clip->length = to - from;
    clip->tracks = (uint8_t)tracks;
    memcpy(clip->glitch, s->glitch, sizeof clip->glitch);
    v.fill = 1;
    for (unsigned t = 0; t < SONG_TRACKS; ++t) {
        unsigned whole = tracks >> t & 1U;
        if (!whole && t) continue;
        if (whole) song_chase(s, t, from, start_value, &v);
        unsigned count, end = song_find(s, t, to);
        const struct song_event *e = song_track(s, t, &count);
        for (unsigned i = song_find(s, t, from); i < end; ++i) {
            if (!whole && song_event_kind(&e[i]) != SONG_FX) continue;
            clip->event[clip->count] = moved(&e[i], song_event_start(&e[i]) - from, to);
            clip->track[clip->count++] = (uint8_t)t;
        }
    }
    return 0;
}
SONG_SMALL int song_cut(struct song *s, struct song_clip *clip, unsigned tracks, uint32_t from, uint32_t to)
{
    if (song_copy(s, clip, tracks, from, to)) return -1;
    clear(s, tracks, from, to);
    return 0;
}
SONG_SMALL int song_paste(struct song *s, const struct song_clip *clip, uint32_t at, unsigned track, unsigned insert)
{
    unsigned single = clip->tracks && !(clip->tracks & (clip->tracks - 1U)), targets = single ? 1U << track : clip->tracks;
    if (!clip->length || track >= SONG_TRACKS || at + clip->length > SONG_MAX_CLOCK) return -1;
    unsigned freed = insert ? 0 : in_range(s, targets, at, at + clip->length);
    if (s->events - freed + clip->count > SONG_EVENTS) return -1;
    unsigned fx = 0;
    for (unsigned i = 0; i < clip->count; ++i) fx += song_event_kind(&clip->event[i]) == SONG_FX;
    if (insert) { if (song_insert_time(s, at, clip->length)) return -1; }
    else {
        clear(s, targets, at, at + clip->length);
        unsigned count, first = song_find(s, 0, at);
        const struct song_event *e = song_track(s, 0, &count);
        for (unsigned i = song_find(s, 0, at + clip->length); fx && i-- > first;) /* the FX lane's stutters there too */
            if (song_event_kind(&e[i]) == SONG_FX) (void)song_remove(s, 0, i);
    }
    for (unsigned i = 0; i < clip->count; ++i) {
        struct song_event e = song_board_event(s, clip, i, at);
        (void)song_add_event(s, song_event_kind(&e) == SONG_FX ? 0 : single ? track : clip->track[i], &e);
    }
    return 0;
}
SONG_SMALL struct song_event song_board_event(struct song *s, const struct song_clip *clip, unsigned i, uint32_t at)
{
    struct song_event e = moved(&clip->event[i], at + song_event_start(&clip->event[i]), 0);
    unsigned glitch = song_event_glitch(&e);
    if (glitch) { /* the same attributes, wherever they sit in this song's table */
        int at_now = song_glitch_intern(s, clip->glitch[glitch]);
        e.at = (e.at & ~SONG_GLITCH_BITS) | (uint32_t)(at_now > 0 ? at_now : 0) << SONG_GLITCH_SHIFT;
    }
    return e;
}
SONG_SMALL int song_duplicate(struct song *s, unsigned tracks, uint32_t *from, uint32_t *to)
{
    uint32_t a = *from, b = *to, length = b - a;
    if (a >= b || s->events + in_range(s, tracks, a, b) + fx_only(s, tracks, a, b) > SONG_EVENTS ||
        song_insert_time(s, b, length)) return -1;
    struct start_values v = {s, NULL, a, b, 0, 2};
    for (unsigned t = 0; t < SONG_TRACKS; ++t) {
        unsigned whole = tracks >> t & 1U;
        if (!whole && t) continue;
        if (whole) song_chase(s, t, a, start_value, &v); /* these land at b, after the originals */
        /* The copies sort after the originals, so these indexes hold. */
        for (unsigned i = song_find(s, t, a), end = song_find(s, t, b); i < end; ++i) {
            if (!whole && song_event_kind(&s->event[s->first[t] + i]) != SONG_FX) continue;
            struct song_event e = moved(&s->event[s->first[t] + i], song_event_start(&s->event[s->first[t] + i]) + length, b);
            (void)song_add_event(s, t, &e);
        }
    }
    *from = b;
    *to = b + length;
    return 0;
}

/* Whole bars at `at`, when it is a bar line and the length fits the bar. */
SONG_SMALL static unsigned whole_bars(const struct song *s, uint32_t at, uint32_t length, unsigned *bar)
{
    uint32_t start, size;
    *bar = song_bar_at(s, at, &start, &size);
    return at == start && length % size == 0 ? length / size : 0;
}
SONG_SMALL int song_insert_time(struct song *s, uint32_t at, uint32_t length)
{
    unsigned bar, bars = whole_bars(s, at, length, &bar);
    unsigned grow = bars ? bars : (length + SONG_WHOLE - 1U) / SONG_WHOLE;
    if (!length || s->bars + grow > SONG_MAX_BARS || song_shift(s, at, (int32_t)length)) return -1;
    for (unsigned i = 0; i < s->markers; ++i)
        if (s->marker[i].clock >= at) s->marker[i].clock += length;
    for (unsigned k = 1; k < s->tempos; ++k)
        if (s->tempo[k].clock >= at) s->tempo[k].clock += length;
    for (unsigned m = 1; bars && m < s->meters; ++m) /* the new bars take the meter at `at` */
        if (s->meter[m].bar > bar) s->meter[m].bar = (uint16_t)(s->meter[m].bar + bars);
    if (s->loop_start >= at) s->loop_start += length;
    if (s->loop_end > at) s->loop_end += length;
    s->bars = (uint16_t)(s->bars + grow);
    song_tempo_refresh(s);
    return 0;
}
SONG_SMALL int song_delete_time(struct song *s, uint32_t from, uint32_t to)
{
    if (from >= to) return -1;
    uint32_t length = to - from, start, size;
    unsigned first_bar = song_bar_at(s, from, &start, &size), last_bar = song_bar_at(s, to, NULL, NULL);
    unsigned bars = from == start && song_bar_clock(s, last_bar) == to ? last_bar - first_bar : 0;
    clear(s, 0xffU, from, to);
    for (unsigned t = 0; t < SONG_TRACKS; ++t) { /* notes running into the gap end at it */
        unsigned count, i = song_find(s, t, from);
        const struct song_event *e = song_track(s, t, &count);
        while (i-- > 0 && song_event_start(&e[i]) + SONG_WHOLE * 34U > from)
            if (song_event_start(&e[i]) + e[i].length > from) {
                struct song_event c = e[i];
                c.length = (uint16_t)(from - song_event_start(&e[i]));
                (void)song_set_event(s, t, i, &c);
            }
    }
    if (song_shift(s, to, -(int32_t)length)) return -1;
    unsigned kept = 0;
    for (unsigned i = 0; i < s->markers; ++i) {
        uint32_t c = s->marker[i].clock;
        if (c >= from && c < to) continue;
        s->marker[kept] = s->marker[i];
        if (c >= to) s->marker[kept].clock = c - length;
        ++kept;
    }
    s->markers = (uint8_t)kept;
    /* The last tempo inside the gap applies from the cut on. */
    kept = 0;
    for (unsigned k = 0; k < s->tempos; ++k) {
        uint32_t c = s->tempo[k].clock;
        if (c >= from && c < to && k + 1U < s->tempos && s->tempo[k + 1].clock < to) continue;
        s->tempo[kept] = s->tempo[k];
        s->tempo[kept].clock = c >= to ? c - length : c >= from ? from : c;
        if (!kept || s->tempo[kept].clock != s->tempo[kept - 1].clock) ++kept;
        else s->tempo[kept - 1] = s->tempo[kept];
    }
    s->tempos = (uint8_t)kept;
    kept = 0;
    for (unsigned m = 0; bars && m < s->meters; ++m) {
        unsigned b = s->meter[m].bar;
        if (b >= first_bar && b < last_bar && m + 1U < s->meters && s->meter[m + 1].bar < last_bar) continue;
        s->meter[kept] = s->meter[m];
        s->meter[kept].bar = (uint16_t)(b >= last_bar ? b - bars : b >= first_bar ? first_bar : b);
        if (!kept || s->meter[kept].bar != s->meter[kept - 1].bar) ++kept;
        else s->meter[kept - 1] = s->meter[kept];
    }
    if (bars) s->meters = (uint8_t)kept;
    unsigned shrink = bars ? bars : length / SONG_WHOLE;
    s->bars = (uint16_t)(s->bars > shrink + 1U ? s->bars - shrink : 1U);
    if (s->loop_start >= to) s->loop_start -= length; else if (s->loop_start > from) s->loop_start = from;
    if (s->loop_end >= to) s->loop_end -= length; else if (s->loop_end > from) s->loop_end = from;
    if (s->loop_end <= s->loop_start) s->loop_end = s->loop_start + SONG_WHOLE;
    song_tempo_refresh(s);
    return 0;
}
#endif
