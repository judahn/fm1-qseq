/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Chord mode: the shapes, voicing, strum order and the live strum queue.
 * See chords.h. */
#include "chords.h"
#include <string.h>
#if SONG_UI

const char *const song_chord_names[CHORD_TYPES] = {"IN KEY", "MAJOR", "MINOR", "7", "M7", "MAJ7", "6", "SUS2", "SUS4",
                                                   "DIM", "AUG", "ADD9"};
const uint8_t song_chord_keys_layout[12] = {CHORD_IN_KEY, CHORD_SUS2, CHORD_MAJOR, CHORD_SUS4, CHORD_MINOR, CHORD_DIM,
                                            CHORD_SEVEN, CHORD_MINOR7, CHORD_AUG, CHORD_MAJOR7, CHORD_ADD9, CHORD_SIX};
/* Each shape as the semitones above its root, one bit each. */
static const uint16_t shapes[CHORD_TYPES] = {0x091, 0x091, 0x089, 0x491, 0x489, 0x891, 0x291, 0x085, 0x0a1, 0x049, 0x111, 0x4091};

SONG_SMALL uint8_t song_chord_setting(unsigned on, unsigned voicing, unsigned strum, unsigned direction)
{
    return (uint8_t)((on & 1U) | (voicing < CHORD_VOICINGS ? voicing : 0) << 1 | (strum < CHORD_STRUMS ? strum : 0) << 3 |
                     (direction & 3U) << 6);
}
SONG_SMALL uint32_t song_chord_strum_clocks(unsigned strum) { return strum && strum < CHORD_STRUMS ? 15U << strum : 0; }
SONG_SMALL void song_chord_reset(struct song_chord_play *c, const struct song *s, unsigned track)
{
    memset(c, 0, sizeof *c);
    c->seed = s->seed * 2654435761U + track + 1U;
}
/* Pitches from a set of semitones above `root`, lowest first. */
SONG_SMALL static unsigned pitches(uint32_t set, unsigned root, uint8_t out[CHORD_NOTES])
{
    unsigned n = 0;
    for (unsigned i = 0; i < 32 && n < CHORD_NOTES; ++i)
        if (set >> i & 1U && root + i < 128) out[n++] = (uint8_t)(root + i);
    return n;
}
/* How far a chord is from the last: each note to the nearest of the last's. */
SONG_SMALL static unsigned distance(const uint8_t *a, unsigned n, const struct song_chord_play *c)
{
    unsigned sum = 0;
    for (unsigned i = 0; i < n; ++i) {
        unsigned best = 128;
        for (unsigned j = 0; j < c->last_count; ++j) {
            unsigned d = a[i] > c->last[j] ? a[i] - c->last[j] : c->last[j] - a[i];
            if (d < best) best = d;
        }
        sum += best;
    }
    return sum;
}
SONG_SMALL unsigned song_chord_voice(const struct song *s, unsigned t, unsigned root, unsigned type, struct song_chord_play *c,
                                     uint8_t out[CHORD_NOTES])
{
    uint32_t set = shapes[type < CHORD_TYPES ? type : 0];
    if (type == CHORD_IN_KEY && s->key_scale) /* the second and fourth scale notes above the root */
        set = 1U | 1U << (song_scale_step(s, root, 2) - root) | 1U << (song_scale_step(s, root, 4) - root);
    unsigned n = pitches(set, root, out), voicing = song_chord_voicing(s, t);
    if (voicing == CHORD_SPREAD && n > 1) { /* the second note up an octave */
        uint32_t second = set & (set - 1U);
        n = pitches(set ^ (second & ~(second - 1U)) ^ (second & ~(second - 1U)) << 12, root, out);
    } else if (voicing == CHORD_SMOOTH && c->last_count && n > 1) {
        uint8_t best[CHORD_NOTES], cand[CHORD_NOTES];
        unsigned least = ~0U;
        for (unsigned down = 0; down < 2; ++down)
            for (unsigned k = 0; k < n; ++k) { /* inversion k: the lowest k notes up an octave */
                unsigned ok = 1;
                for (unsigned i = 0; i < n; ++i) {
                    int p = out[(i + k) % n] + (i + k >= n ? 12 : 0) - (down ? 12 : 0);
                    ok &= p >= 0 && p < 128;
                    cand[i] = (uint8_t)p;
                }
                unsigned d = ok ? distance(cand, n, c) : ~0U;
                if (d < least) { least = d; memcpy(best, cand, n); }
            }
        if (least != ~0U) memcpy(out, best, n);
    }
    memcpy(c->last, out, n);
    c->last_count = (uint8_t)n;
    return n;
}
SONG_SMALL void song_chord_strum(const struct song *s, unsigned t, unsigned n, struct song_chord_play *c, uint8_t offset[CHORD_NOTES])
{
    uint32_t span = song_chord_strum_clocks(song_chord_strum_of(s, t));
    unsigned direction = song_chord_direction(s, t), down = direction == CHORD_DOWN;
    uint8_t order[CHORD_NOTES] = {0, 1, 2, 3};
    if (direction == CHORD_ALTERNATE) { down = c->flip; c->flip ^= 1U; }
    if (direction == CHORD_RANDOM)
        for (unsigned i = n; i > 1; --i) { /* a shuffle from the seed */
            c->seed ^= c->seed << 13; c->seed ^= c->seed >> 17; c->seed ^= c->seed << 5;
            unsigned j = c->seed % i, x = order[i - 1U];
            order[i - 1U] = order[j]; order[j] = (uint8_t)x;
        }
    for (unsigned i = 0; i < n; ++i) {
        unsigned place = direction == CHORD_RANDOM ? order[i] : down ? n - 1U - i : i;
        offset[i] = (uint8_t)(n > 1 ? place * span / (n - 1U) : 0);
    }
}

SONG_SMALL void song_strum_add(struct song_strum *q, uint64_t from, unsigned bpm100, unsigned track, const uint8_t *pitch,
                               const uint8_t *offset, unsigned n, unsigned velocity)
{
    for (unsigned i = 0; i < n && q->count < STRUM_QUEUE; ++i, ++q->count) {
        q->frame[q->count] = from + offset[i] * SONG_UNITS_PER_CLOCK / song_rate(bpm100);
        q->track[q->count] = (uint8_t)track;
        q->pitch[q->count] = pitch[i];
        q->velocity[q->count] = (uint8_t)velocity;
    }
}
SONG_SMALL static void remove_at(struct song_strum *q, unsigned i)
{
    --q->count;
    memmove(&q->frame[i], &q->frame[i + 1U], (q->count - i) * sizeof q->frame[0]);
    memmove(&q->track[i], &q->track[i + 1U], q->count - i);
    memmove(&q->pitch[i], &q->pitch[i + 1U], q->count - i);
    memmove(&q->velocity[i], &q->velocity[i + 1U], q->count - i);
}
SONG_SMALL void song_strum_due(struct song_strum *q, uint64_t before, void (*send)(unsigned track, unsigned pitch, unsigned velocity))
{
    for (;;) {
        unsigned first = STRUM_QUEUE;
        for (unsigned i = 0; i < q->count; ++i)
            if (q->frame[i] < before && (first == STRUM_QUEUE || q->frame[i] < q->frame[first])) first = i;
        if (first == STRUM_QUEUE) return;
        send(q->track[first], q->pitch[first], q->velocity[first]);
        remove_at(q, first);
    }
}
SONG_SMALL int song_strum_drop(struct song_strum *q, unsigned track, unsigned pitch)
{
    for (unsigned i = 0; i < q->count; ++i)
        if (q->track[i] == track && q->pitch[i] == pitch) { remove_at(q, i); return 1; }
    return 0;
}
#endif
