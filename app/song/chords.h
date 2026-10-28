/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#ifndef FM1_SONG_CHORDS_H
#define FM1_SONG_CHORDS_H
#include "song.h"
#if SONG_UI

/* Chord mode (design 8g): a play setting on each instrument track. A root
 * key plays a chord of the type picked last, voiced and strummed as the
 * track's settings say, and it records as plain notes. The settings live in
 * the song: chord[track] holds on (bit 0), the voicing (bits 1 and 2), the
 * strum (bits 3 to 5: off, then a 64th, 32nd, 16th or 8th from the first
 * note to the last) and the direction (bits 6 and 7), and chord_type[track]
 * the type picked. Model code only: the port plays the keys (song_chord.c). */
enum { CHORD_IN_KEY, CHORD_MAJOR, CHORD_MINOR, CHORD_SEVEN, CHORD_MINOR7, CHORD_MAJOR7, CHORD_SIX, CHORD_SUS2,
       CHORD_SUS4, CHORD_DIM, CHORD_AUG, CHORD_ADD9, CHORD_TYPES };
enum { CHORD_CLOSE, CHORD_SPREAD, CHORD_SMOOTH, CHORD_VOICINGS };
enum { CHORD_UP, CHORD_DOWN, CHORD_ALTERNATE, CHORD_RANDOM, CHORD_DIRECTIONS };
enum { CHORD_STRUMS = 5, CHORD_NOTES = 4 };
static inline unsigned song_chord_on(const struct song *s, unsigned t) { return s->chord[t] & 1U; }
static inline unsigned song_chord_voicing(const struct song *s, unsigned t) { return s->chord[t] >> 1 & 3U; }
static inline unsigned song_chord_strum_of(const struct song *s, unsigned t) { return s->chord[t] >> 3 & 7U; }
static inline unsigned song_chord_direction(const struct song *s, unsigned t) { return s->chord[t] >> 6 & 3U; }
/* Settings, each clamped to its range. */
uint8_t song_chord_setting(unsigned on, unsigned voicing, unsigned strum, unsigned direction);
extern const char *const song_chord_names[CHORD_TYPES];
/* The type on each of the 12 type keys, F4 to E5: the common types on the
 * white keys, the color types on the black ones. */
extern const uint8_t song_chord_keys_layout[12];
uint32_t song_chord_strum_clocks(unsigned strum); /* first note to last: 0, 30, 60, 120 or 240 */

/* Each track's playing: the last chord, for smooth voicing, the alternating
 * strum's next stroke, and the random strum's seed. */
struct song_chord_play { uint8_t last[CHORD_NOTES], last_count, flip; uint32_t seed; };
void song_chord_reset(struct song_chord_play *c, const struct song *s, unsigned track);
/* The chord of `type` on `root`, voiced for track t, lowest note first.
 * Close is the shape on the root, spread lifts its second note an octave,
 * and smooth takes the inversion of the close chord, or that an octave down,
 * nearest the last chord. In key stacks the second and fourth notes of the
 * song's scale above the root (major with no key set). Returns how many,
 * notes past 127 left out. */
unsigned song_chord_voice(const struct song *s, unsigned t, unsigned root, unsigned type, struct song_chord_play *c,
                          uint8_t out[CHORD_NOTES]);
/* The strum: each note's start in clocks after the chord's first, for the
 * notes as song_chord_voice gave them, by the track's direction. Up plays
 * the lowest first, down the highest, alternating flips each chord (up
 * first), and random draws an order from the seed. */
void song_chord_strum(const struct song *s, unsigned t, unsigned n, struct song_chord_play *c, uint8_t offset[CHORD_NOTES]);

/* Live strums, out through the same per-block delivery as playback: each
 * note at its exact audio frame, sent before the block holding it renders. */
enum { STRUM_QUEUE = 16 };
struct song_strum {
    uint64_t frame[STRUM_QUEUE];
    uint8_t track[STRUM_QUEUE], pitch[STRUM_QUEUE], velocity[STRUM_QUEUE], count;
};
/* A chord from audio frame `from`, each note its offset in clocks later at
 * the tempo. Notes past a full queue are left out. */
void song_strum_add(struct song_strum *q, uint64_t from, unsigned bpm100, unsigned track, const uint8_t *pitch,
                    const uint8_t *offset, unsigned n, unsigned velocity);
/* Every note due before audio frame `before`, in time order, through `send`. */
void song_strum_due(struct song_strum *q, uint64_t before, void (*send)(unsigned track, unsigned pitch, unsigned velocity));
/* A note still waiting goes: 1 if it was waiting, 0 if it went out already. */
int song_strum_drop(struct song_strum *q, unsigned track, unsigned pitch);
#endif
#endif
