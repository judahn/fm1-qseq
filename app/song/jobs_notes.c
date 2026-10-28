/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Note jobs: quantize, velocity, crescendo, gate, transpose, fit to scale,
 * shift, chord sort and strum. See jobs.h. */
#include "jobs.h"
#include "lanes.h"
#include "song_config.h"
#include <string.h>
#if SONG_UI

/* Each note of the range through fn, changed in place, the tracks sorted
 * again after. With lanes set, controllers and program changes too. */
typedef void (*job_fn)(const struct song *s, unsigned t, struct song_event *e, const void *ctx);
SONG_SMALL static int each(struct song *s, const struct song_range *r, unsigned skip_drums, unsigned lanes, job_fn fn,
                           const void *ctx)
{
    int changed = 0;
    if (r->from >= r->to) return -1;
    for (unsigned t = 0; t < SONG_TRACKS; ++t) {
        if (!(r->tracks >> t & 1U) || (skip_drums && (r->drums >> t & 1U))) continue;
        unsigned moved = 0;
        for (unsigned i = song_find(s, t, r->from), last = song_find(s, t, r->to); i < last; ++i) {
            const struct song_event *old = &s->event[s->first[t] + i];
            if (!song_is_note(old) && !(lanes && song_is_lane(old))) continue;
            struct song_event e = *old;
            e.at &= ~SONG_FRESH;
            fn(s, t, &e, ctx);
            if (!memcmp(&e, old, sizeof e)) continue;
            moved |= song_event_start(&e) != song_event_start(old);
            song_retouch(s, t, i, &e);
            ++changed;
        }
        if (moved) song_sort_track(s, t);
    }
    return changed;
}
static int clampi(int v, int low, int high) { return v < low ? low : v > high ? high : v; }
static void start_at(struct song_event *e, uint32_t clock) { e->at = (e->at & ~0xffffffU) | clock; }

/* Quantize. Grid line k of a bar sits at k * grid, every odd one later by
 * the swing. The nearest line wins (the earlier on a tie), and the note
 * moves `strength` percent of the way, rounded. */
struct quantize { unsigned grid, strength, swing, gate; };
SONG_SMALL static uint32_t snap(const struct song *s, uint32_t clock, const struct quantize *q)
{
    uint32_t start, length, best = 0, gap = ~0U;
    song_bar_at(s, clock, &start, &length);
    uint32_t k0 = (clock - start) / q->grid;
    for (uint32_t k = k0 ? k0 - 1U : 0; k <= k0 + 1U; ++k) {
        uint32_t at = k * q->grid + (k & 1U ? q->grid * (2U * q->swing - 100U) / 100U : 0U);
        if (at > length) at = length; /* the next bar's first line */
        uint32_t d = start + at > clock ? start + at - clock : clock - start - at;
        if (d < gap) { gap = d; best = start + at; }
    }
    int64_t move = ((int64_t)best - (int64_t)clock) * (int64_t)q->strength;
    return (uint32_t)((int64_t)clock + (move >= 0 ? (move + 50) / 100 : -((-move + 50) / 100)));
}
SONG_SMALL static void quantized(const struct song *s, unsigned t, struct song_event *e, const void *ctx)
{
    const struct quantize *q = ctx;
    (void)t;
    uint32_t start = song_event_start(e), at = snap(s, start, q);
    if (q->gate) {
        uint32_t end = snap(s, start + e->length, q);
        e->length = (uint16_t)(end > at ? (end - at > 0xffffU ? 0xffffU : end - at) : q->grid);
    }
    start_at(e, at > SONG_MAX_CLOCK ? SONG_MAX_CLOCK : at);
}
SONG_SMALL int song_job_quantize(struct song *s, const struct song_range *r, unsigned grid, unsigned strength, unsigned swing,
                                 unsigned gate)
{
    const struct quantize q = {grid, strength > 100 ? 100 : strength, swing < 50 ? 50 : swing > 83 ? 83 : swing, gate};
    return grid ? each(s, r, 0, 0, quantized, &q) : -1;
}

/* Velocity and gate: a value, or a rate and an offset. Crescendo by time. */
struct amounts { unsigned mode, value; int offset; uint32_t from, to; };
SONG_SMALL static void velocity(const struct song *s, unsigned t, struct song_event *e, const void *ctx)
{
    const struct amounts *a = ctx;
    (void)s; (void)t;
    e->b = (uint8_t)clampi(a->mode == SONG_JOB_SET ? (int)a->value : (int)(e->b * a->value / 100U) + a->offset, 1, 127);
}
SONG_SMALL int song_job_velocity(struct song *s, const struct song_range *r, unsigned mode, unsigned value, int offset)
{
    const struct amounts a = {mode, value, offset, 0, 0};
    return mode > SONG_JOB_RATE ? -1 : each(s, r, 0, 0, velocity, &a);
}
SONG_SMALL static void crescendo(const struct song *s, unsigned t, struct song_event *e, const void *ctx)
{
    const struct amounts *a = ctx;
    (void)s; (void)t;
    int64_t step = (int64_t)a->offset * (song_event_start(e) - a->from);
    int delta = (int)(step >= 0 ? (step + (a->to - a->from) / 2) / (a->to - a->from) : -((-step + (a->to - a->from) / 2) / (a->to - a->from)));
    e->b = (uint8_t)clampi(e->b + delta, 1, 127);
}
SONG_SMALL int song_job_crescendo(struct song *s, const struct song_range *r, int amount)
{
    const struct amounts a = {0, 0, amount, r->from, r->to};
    return each(s, r, 0, 0, crescendo, &a);
}
SONG_SMALL static void gate(const struct song *s, unsigned t, struct song_event *e, const void *ctx)
{
    const struct amounts *a = ctx;
    if (a->mode == SONG_JOB_LEGATO) { /* to the next note's start on the track */
        unsigned count, i = song_find(s, t, song_event_start(e) + 1U);
        const struct song_event *list = song_track(s, t, &count);
        while (i < count && !song_is_note(&list[i])) ++i;
        if (i < count) e->length = (uint16_t)clampi((int)(song_event_start(&list[i]) - song_event_start(e)), 1, 0xffff);
        return;
    }
    e->length = (uint16_t)clampi(a->mode == SONG_JOB_SET ? (int)a->value : (int)(e->length * a->value / 100U) + a->offset,
                                 1, 0xffff);
}
SONG_SMALL int song_job_gate(struct song *s, const struct song_range *r, unsigned mode, unsigned value, int offset)
{
    const struct amounts a = {mode, value, offset, 0, 0};
    return mode > SONG_JOB_LEGATO ? -1 : each(s, r, 0, 0, gate, &a);
}

/* Pitch: semitones or scale steps inside a note range, or into the key. */
struct pitch { int amount; unsigned steps, low, high; };
SONG_SMALL static void transposed(const struct song *s, unsigned t, struct song_event *e, const void *ctx)
{
    const struct pitch *p = ctx;
    (void)t;
    if (e->a < p->low || e->a > p->high) return;
    e->a = (uint8_t)(p->steps ? song_scale_step(s, e->a, p->amount) : (unsigned)clampi(e->a + p->amount, 0, 127));
}
SONG_SMALL int song_job_transpose(struct song *s, const struct song_range *r, int amount, unsigned scale_steps, unsigned low,
                                  unsigned high)
{
    const struct pitch p = {amount, scale_steps, low, high};
    return each(s, r, 1, 0, transposed, &p);
}
SONG_SMALL static void fitted(const struct song *s, unsigned t, struct song_event *e, const void *ctx)
{
    (void)t; (void)ctx;
    e->a = (uint8_t)song_scale_snap(s, e->a);
}
SONG_SMALL int song_job_fit_scale(struct song *s, const struct song_range *r) { return each(s, r, 1, 0, fitted, NULL); }

/* Shift: notes and automation, never out of the range. */
SONG_SMALL static void shifted(const struct song *s, unsigned t, struct song_event *e, const void *ctx)
{
    const struct amounts *a = ctx;
    (void)s; (void)t;
    int64_t at = (int64_t)song_event_start(e) + a->offset;
    start_at(e, at < a->from ? a->from : at >= a->to ? a->to - 1U : (uint32_t)at);
}
SONG_SMALL int song_job_shift(struct song *s, const struct song_range *r, int clocks)
{
    const struct amounts a = {0, 0, clocks, r->from, r->to};
    return each(s, r, 0, 1, shifted, &a);
}

/* Chords: the notes sharing a start, ordered by pitch. Sorting takes them
 * out and puts them back in order, last first, so undo restores the old
 * order. Strum then starts each one `clocks` after the one before. Chords
 * go from the last to the first, so strummed notes are never met again. */
SONG_SMALL static int chords(struct song *s, const struct song_range *r, unsigned clocks, unsigned down)
{
    int changed = 0;
    if (r->from >= r->to) return -1;
    for (unsigned t = 0; t < SONG_TRACKS; ++t) {
        if (!(r->tracks >> t & 1U)) continue;
        for (uint32_t limit = r->to;;) {
            unsigned end = song_find(s, t, limit), n = 0, all = 0, order = 1;
            if (end <= song_find(s, t, r->from)) break;
            const struct song_event *e = s->event + s->first[t];
            uint32_t at = song_event_start(&e[end - 1U]);
            unsigned i = song_find(s, t, at);
            struct song_event chord[16];
            for (unsigned k = i; k < end; ++k) if (song_is_note(&e[k]) && all++ < 16) chord[n++] = e[k];
            for (unsigned a = 1; a < n; ++a) /* by pitch, stable */
                for (unsigned b = a; b > 0 && (down ? chord[b].a > chord[b - 1U].a : chord[b].a < chord[b - 1U].a); --b) {
                    struct song_event x = chord[b]; chord[b] = chord[b - 1U]; chord[b - 1U] = x;
                }
            for (unsigned k = i, m = 0; k < end; ++k) if (song_is_note(&e[k])) order &= m < n && !memcmp(&e[k], &chord[m++], 8);
            limit = at;
            if (n < 2 || all > 16 || (order && !clocks)) continue;
            for (unsigned k = end; k-- > i;) if (song_is_note(&s->event[s->first[t] + k])) (void)song_remove(s, t, k);
            for (unsigned m = 0; m < n; ++m) {
                struct song_event c = chord[m];
                uint32_t offset = m * clocks, stop = at + c.length;
                c.at &= ~SONG_FRESH;
                if (at + offset <= SONG_MAX_CLOCK) start_at(&c, at + offset);
                c.length = (uint16_t)(stop > song_event_start(&c) ? stop - song_event_start(&c) : 1U);
                (void)song_add_event(s, t, &c);
            }
            changed += (int)n;
        }
    }
    return changed;
}
SONG_SMALL int song_job_chord_sort(struct song *s, const struct song_range *r, unsigned down) { return chords(s, r, 0, down); }
SONG_SMALL int song_job_strum(struct song *s, const struct song_range *r, unsigned clocks, unsigned down)
{
    return clocks ? chords(s, r, clocks, down) : -1;
}
#endif
