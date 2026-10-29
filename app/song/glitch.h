/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#ifndef FM1_SONG_GLITCH_H
#define FM1_SONG_GLITCH_H
#include "song.h"

/* Per-note glitch (design section 8e). A note's attributes are one 32-bit
 * word, kept in the song's glitch table, and the note holds its 5-bit
 * index in the spare bits of `at`. Notes with the same attributes share an
 * entry, so 31 distinct settings fit a song, and entries no note uses are
 * reclaimed when the table fills. Fields, each 0 when off:
 *   REPEATS  1, 2, 3, 4, 6 or 8 hits inside the note's length
 *   DRIFT    -8..7 semitones per repeat, or scale steps with SCALE
 *   FADE     the last repeat at 100, 70, 40 or 15% velocity
 *   CHANCE   plays 100, 87, 75, 62, 50, 37, 25 or 12% of the time
 *   OCTAVE   0, 25, 50 or 100% chance of an octave jump, RANGE 1 or 2
 *   ACCENT   a velocity boost, FLAM a grace hit a 64th before
 *   SWAP     0, 25, 50 or 100% chance of another pad, on drum tracks */
enum { GLITCH_REPEATS, GLITCH_DRIFT, GLITCH_SCALE, GLITCH_FADE, GLITCH_CHANCE, GLITCH_OCTAVE, GLITCH_RANGE,
       GLITCH_ACCENT, GLITCH_FLAM, GLITCH_SWAP, GLITCH_FIELDS };
enum { GLITCH_FLAM_CLOCKS = SONG_PPQ / 16, GLITCH_ACCENT_BOOST = 32, GLITCH_ENTRIES = 32 };
unsigned song_glitch_field(uint32_t word, unsigned field);          /* raw value */
uint32_t song_glitch_turn(uint32_t word, unsigned field, int delta); /* clamped */
int song_glitch_drift(uint32_t word);
unsigned song_glitch_repeats(uint32_t word);
unsigned song_glitch_percent(uint32_t word, unsigned field); /* FADE, CHANCE, OCTAVE, SWAP */
uint32_t song_glitch_word(const struct song *s, const struct song_event *e);
/* The table index for a word, adding it when new. -1 when the table is full. */
int song_glitch_intern(struct song *s, uint32_t word);
/* Gives the note at a track index the word. Journaled. 0, or -1 when full. */
int song_glitch_apply(struct song *s, unsigned track, unsigned index, uint32_t word);
#endif
