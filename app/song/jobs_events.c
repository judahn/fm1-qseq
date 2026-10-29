/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Event jobs: erase, extract, ramps and thinning, whole-track copy, merge
 * and clear, and pasting N times. See jobs.h. */
#include "jobs.h"
#include "lanes.h"
#include "song_config.h"
#if SONG_UI

SONG_SMALL static unsigned type_of(const struct song_event *e)
{
    unsigned k = song_event_kind(e);
    return k == SONG_NOTE ? SONG_JOB_NOTES : k == SONG_CONTROL ? SONG_JOB_CONTROLS : k == SONG_PROGRAM ? SONG_JOB_PROGRAMS : 0;
}
/* Events of `types` in [from, to) on the tracks, counted or removed. */
SONG_SMALL static unsigned sweep(struct song *s, unsigned tracks, uint32_t from, uint32_t to, unsigned types, unsigned remove)
{
    unsigned n = 0;
    for (unsigned t = 0; t < SONG_TRACKS; ++t)
        if (tracks >> t & 1U)
            for (unsigned i = song_find(s, t, to), first = song_find(s, t, from); i-- > first;)
                if (type_of(&s->event[s->first[t] + i]) & types) {
                    ++n;
                    if (remove) (void)song_remove(s, t, i);
                }
    return n;
}
enum { ALL = SONG_JOB_NOTES | SONG_JOB_CONTROLS | SONG_JOB_PROGRAMS };
SONG_SMALL int song_job_erase(struct song *s, const struct song_range *r, unsigned types)
{
    return r->from < r->to && types ? (int)sweep(s, r->tracks, r->from, r->to, types, 1) : -1;
}
/* Moved, so nothing can overflow. */
SONG_SMALL int song_job_extract(struct song *s, const struct song_range *r, unsigned type, unsigned to_track)
{
    int moved = 0;
    if (r->from >= r->to || to_track >= SONG_TRACKS || !type) return -1;
    for (unsigned t = 0; t < SONG_TRACKS; ++t)
        if (t != to_track && (r->tracks >> t & 1U))
            for (unsigned i = song_find(s, t, r->to), first = song_find(s, t, r->from); i-- > first;) {
                struct song_event e = s->event[s->first[t] + i];
                if (!(type_of(&e) & type) || song_remove(s, t, i)) continue;
                e.at &= ~SONG_FRESH;
                (void)song_add_event(s, to_track, &e);
                ++moved;
            }
    return moved;
}

/* Ramp point k of n: f(x) = x + c/16 (x^2 - x), so -16 starts fast, 16 slow. */
SONG_SMALL static int ramp_value(int start, int end, unsigned k, unsigned n, int curve)
{
    if (!n) return start;
    int64_t num = (int64_t)(end - start) * (16 * (int64_t)k * n + curve * ((int64_t)k * k - (int64_t)k * n)), den = 16 * (int64_t)n * n;
    return start + (int)(num >= 0 ? (num + den / 2) / den : -((-num + den / 2) / den));
}
SONG_SMALL int song_job_ramp(struct song *s, const struct song_range *r, unsigned lane, int start, int end, unsigned spacing,
                             int curve)
{
    if (r->from >= r->to || !spacing || curve < -16 || curve > 16 || (lane > 127 && lane != SONG_JOB_TEMPO)) return -1;
    unsigned n = (r->to - r->from - 1U) / spacing, points = 0, tracks = 0, gone = 0;
    int low = lane == SONG_JOB_TEMPO ? 20 : 0, high = lane == SONG_JOB_TEMPO ? 300 : 127, last = -1;
    for (unsigned k = 0; k <= n; ++k) { /* only changes are stored */
        int v = ramp_value(start, end, k, n, curve);
        v = v < low ? low : v > high ? high : v;
        points += v != last;
        last = v;
    }
    if (lane == SONG_JOB_TEMPO) {
        unsigned kept = 0;
        for (unsigned k = 0; k < s->tempos; ++k) kept += !k || s->tempo[k].clock < r->from || s->tempo[k].clock >= r->to;
        if (kept + points - (r->from == 0) > SONG_TEMPOS) return -1;
        for (unsigned k = s->tempos; k-- > 1;)
            if (s->tempo[k].clock >= r->from && s->tempo[k].clock < r->to) (void)song_remove_tempo(s, s->tempo[k].clock);
    } else {
        for (unsigned t = 0; t < SONG_TRACKS; ++t) tracks += r->tracks >> t & 1U;
        for (unsigned t = 0; t < SONG_TRACKS; ++t)
            if (r->tracks >> t & 1U)
                for (unsigned i = song_find(s, t, r->from), end_i = song_find(s, t, r->to); i < end_i; ++i)
                    gone += song_is_lane(&s->event[s->first[t] + i]) && song_event_lane(&s->event[s->first[t] + i]) == lane;
        if (!tracks || s->events - gone + tracks * points > SONG_EVENTS) return -1;
        (void)song_lane_clear(s, r->tracks, lane, r->from, r->to);
    }
    last = -1;
    for (unsigned k = 0; k <= n; ++k) {
        int v = ramp_value(start, end, k, n, curve);
        v = v < low ? low : v > high ? high : v;
        if (v == last) continue;
        last = v;
        uint32_t at = r->from + k * spacing;
        if (lane == SONG_JOB_TEMPO) { (void)song_set_tempo(s, at, (unsigned)v * 100U); continue; }
        const struct song_event e = song_lane_event(at, lane, (unsigned)v);
        for (unsigned t = 0; t < SONG_TRACKS; ++t)
            if (r->tracks >> t & 1U) (void)song_add_event(s, t, &e);
    }
    return (int)(lane == SONG_JOB_TEMPO ? points : points * tracks);
}
/* Every other change in the range goes: the 2nd, the 4th and so on. */
SONG_SMALL int song_job_thin(struct song *s, const struct song_range *r, unsigned lane)
{
    int removed = 0;
    if (r->from >= r->to) return -1;
    if (lane == SONG_JOB_TEMPO) {
        unsigned seen = 0;
        for (unsigned k = 1; k < s->tempos; ++k) {
            if (s->tempo[k].clock < r->from || s->tempo[k].clock >= r->to || !(seen++ & 1U)) continue;
            (void)song_remove_tempo(s, s->tempo[k].clock);
            --k, ++removed;
        }
        return removed;
    }
    for (unsigned t = 0; t < SONG_TRACKS; ++t) {
        if (!(r->tracks >> t & 1U)) continue;
        unsigned first = song_find(s, t, r->from), n = 0;
        for (unsigned i = first, end = song_find(s, t, r->to); i < end; ++i) {
            const struct song_event *e = &s->event[s->first[t] + i];
            if (!song_is_lane(e) || song_event_lane(e) != lane || !(n++ & 1U)) continue;
            (void)song_remove(s, t, i);
            --i, --end, ++removed;
        }
    }
    return removed;
}

/* Whole tracks. A track's clip blocks stay with it (blocks.h). */
SONG_SMALL static unsigned owned(const struct song *s, unsigned t)
{
    unsigned count, n = 0;
    const struct song_event *e = song_track(s, t, &count);
    for (unsigned i = 0; i < count; ++i) n += song_event_kind(&e[i]) != SONG_FX;
    return n;
}
SONG_SMALL int song_job_clear(struct song *s, unsigned tracks)
{
    return (int)sweep(s, tracks, 0, SONG_MAX_CLOCK + 1U, ALL, 1);
}
SONG_SMALL int song_job_copy_track(struct song *s, unsigned from, unsigned to)
{
    unsigned n = owned(s, from);
    if (from >= SONG_TRACKS || to >= SONG_TRACKS || from == to || s->events - owned(s, to) + n > SONG_EVENTS) return -1;
    (void)song_job_clear(s, 1U << to);
    for (unsigned i = 0; i < s->count[from]; ++i) {
        struct song_event e = s->event[s->first[from] + i];
        if (song_event_kind(&e) == SONG_FX) continue;
        e.at &= ~SONG_FRESH;
        (void)song_add_event(s, to, &e);
    }
    return (int)n;
}
SONG_SMALL int song_job_merge(struct song *s, unsigned from, unsigned into)
{
    const struct song_range all = {0, SONG_MAX_CLOCK + 1U, (uint8_t)(1U << from), 0};
    return from >= SONG_TRACKS || into >= SONG_TRACKS || from == into ? -1 : song_job_extract(s, &all, ALL, into);
}

/* Paste N times: the whole span is cleared first, so each paste only adds
 * and the check up front holds to the end. */
SONG_SMALL int song_job_paste(struct song *s, const struct song_clip *clip, uint32_t at, unsigned track, unsigned times,
                              int transpose, unsigned drums)
{
    unsigned single = clip->tracks && !(clip->tracks & (clip->tracks - 1U)), targets = single ? 1U << track : clip->tracks;
    uint64_t end = at + (uint64_t)times * clip->length;
    unsigned bars = s->bars;
    if (!clip->length || !times || track >= SONG_TRACKS || end > SONG_MAX_CLOCK) return -1;
    while (bars < SONG_MAX_BARS && song_bar_clock(s, bars) < end) ++bars;
    if (song_bar_clock(s, bars) < end ||
        s->events - sweep(s, targets, at, (uint32_t)end, ALL, 0) + (uint64_t)times * clip->count > SONG_EVENTS) return -1;
    (void)sweep(s, targets, at, (uint32_t)end, ALL, 1);
    s->bars = (uint16_t)bars;
    for (unsigned k = 0; k < times; ++k) {
        uint32_t from = at + k * clip->length, to = from + clip->length;
        (void)song_paste(s, clip, from, track, 0);
        int shift = transpose * (int)(k + 1U);
        for (unsigned t = 0; shift && t < SONG_TRACKS; ++t) {
            if (!(targets >> t & 1U) || (drums >> t & 1U)) continue;
            for (unsigned i = song_find(s, t, from), last = song_find(s, t, to); i < last; ++i) {
                struct song_event e = s->event[s->first[t] + i];
                int pitch = e.a + shift;
                if (!song_is_note(&e)) continue;
                e.a = (uint8_t)(pitch < 0 ? 0 : pitch > 127 ? 127 : pitch);
                song_retouch(s, t, i, &e);
            }
        }
    }
    return (int)(times * clip->count);
}
#endif
