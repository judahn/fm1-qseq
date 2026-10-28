/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Bars, beats, snapping and the tempo map. All integer: clocks for musical
 * position, and for time a per-segment unit of 1/(480 * bpm100) frame, in
 * which one clock is exactly SONG_UNITS_PER_CLOCK units at any tempo. */
#include "song.h"
#include <stddef.h>

static uint32_t bar_length(const struct song_meter *m) { return m->beats * (uint32_t)(SONG_WHOLE / m->unit); }

const struct song_meter *song_meter_at(const struct song *s, unsigned bar)
{
    unsigned i = 0;
    while (i + 1U < s->meters && s->meter[i + 1].bar <= bar) ++i;
    return &s->meter[i];
}

uint32_t song_bar_clock(const struct song *s, unsigned bar)
{
    uint32_t clock = 0;
    for (unsigned i = 0; i < s->meters; ++i) {
        unsigned from = s->meter[i].bar, to = i + 1U < s->meters ? s->meter[i + 1].bar : ~0U;
        if (bar <= to) return clock + (bar - from) * bar_length(&s->meter[i]);
        clock += (to - from) * bar_length(&s->meter[i]);
    }
    return clock;
}

unsigned song_bar_at(const struct song *s, uint32_t clock, uint32_t *start, uint32_t *length)
{
    uint32_t at = 0;
    for (unsigned i = 0;; ++i) {
        uint32_t size = bar_length(&s->meter[i]);
        if (i + 1U < s->meters) {
            uint32_t span = (s->meter[i + 1].bar - s->meter[i].bar) * size;
            if (clock >= at + span) { at += span; continue; }
        }
        unsigned n = (clock - at) / size;
        if (start) *start = at + n * size;
        if (length) *length = size;
        return s->meter[i].bar + n;
    }
}

uint32_t song_end(const struct song *s) { return song_bar_clock(s, s->bars); }

void song_bbc(const struct song *s, uint32_t clock, unsigned out[3])
{
    uint32_t start;
    unsigned bar = song_bar_at(s, clock, &start, NULL);
    uint32_t beat = SONG_WHOLE / song_meter_at(s, bar)->unit;
    out[0] = bar + 1;
    out[1] = (clock - start) / beat + 1;
    out[2] = (clock - start) % beat;
}

/* Bar, half bar, beat or 8th, never longer than the bar itself. */
static uint32_t snap_step(const struct song *s, unsigned bar, uint32_t length, unsigned zoom)
{
    uint32_t step = zoom == 0 ? length : zoom == 1 ? length / 2
        : zoom == 2 ? SONG_WHOLE / song_meter_at(s, bar)->unit : SONG_PPQ / 2;
    return step && step < length ? step : length;
}

uint32_t song_snap_move(const struct song *s, uint32_t clock, unsigned zoom, int steps)
{
    uint32_t end = song_end(s), start, length;
    if (clock > end) clock = end;
    for (; steps > 0 && clock < end; --steps) {
        unsigned bar = song_bar_at(s, clock, &start, &length);
        uint32_t step = snap_step(s, bar, length, zoom), next = start + ((clock - start) / step + 1) * step;
        clock = next < start + length ? next : start + length;
    }
    for (; steps < 0 && clock > 0; ++steps) {
        unsigned bar = song_bar_at(s, clock - 1, &start, &length);
        uint32_t step = snap_step(s, bar, length, zoom);
        clock = start + (clock - 1 - start) / step * step;
    }
    return clock;
}

int song_marker_step(const struct song *s, uint32_t clock, int direction)
{
    if (direction > 0) {
        for (unsigned i = 0; i < s->markers; ++i)
            if (s->marker[i].clock > clock) return (int)i;
    } else
        for (unsigned i = s->markers; i-- > 0;)
            if (s->marker[i].clock < clock) return (int)i;
    return -1;
}

unsigned song_tempo_at(const struct song *s, uint32_t clock)
{
    unsigned k = 0;
    while (k + 1U < s->tempos && s->tempo[k + 1].clock <= clock) ++k;
    return k;
}

uint64_t song_units(const struct song *s, unsigned segment, uint32_t clock)
{
    return s->tempo_units[segment] + (uint64_t)(clock - s->tempo[segment].clock) * SONG_UNITS_PER_CLOCK;
}

/* Split the product so neither half can overflow 64 bits. */
uint64_t song_rescale(uint64_t value, uint32_t to, uint32_t from)
{
    uint64_t q = value / from, r = value % from;
    return q * to + (r * to + from - 1U) / from;
}

/* Segment k starts at the end of segment k-1, converted to its own units and
 * rounded down. That costs under one unit (1/(480 * bpm100) frame) per tempo
 * change and never grows with song length. */
void song_tempo_refresh(struct song *s)
{
    s->tempo[0].clock = 0;
    s->tempo_units[0] = 0;
    for (unsigned k = 1; k < s->tempos; ++k) {
        uint64_t end = song_units(s, k - 1, s->tempo[k].clock);
        uint32_t to = song_rate(s->tempo[k].bpm100), from = song_rate(s->tempo[k - 1].bpm100);
        s->tempo_units[k] = end / from * to + end % from * to / from;
    }
}
