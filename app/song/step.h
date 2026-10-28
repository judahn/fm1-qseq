/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#ifndef FM1_SONG_STEP_H
#define FM1_SONG_STEP_H
#include "player.h"

/* Step entry (design section 4). The pointer is the selection: the notes on
 * the track that start exactly at the pointer, all of them unless narrowed
 * by picking pitches. Knob edits change the entry settings and, when the
 * pointer sits on notes, rewrite the selected notes too. Model code only:
 * the panel maps keys to pitches, and previews go out through `out`. */
enum { STEP_64TH, STEP_32ND, STEP_16TH, STEP_8TH, STEP_QUARTER, STEP_HALF, STEP_WHOLE, STEP_LENGTHS };
enum { STEP_GATES = 4, STEP_RANDOMS = 4, STEP_CHORD = 16 };
struct song_step {
    struct song *song;
    struct song_output out;       /* previews of the notes the pointer lands on */
    uint32_t pointer, last_at, random;
    uint32_t pick[4];             /* narrowed selection, one bit per pitch */
    uint16_t serial;              /* counts previews, so the panel can time them out */
    uint8_t track, length, triplet, dots, gate, velocity, random_level, picked;
    uint8_t chord[STEP_CHORD], chord_count;   /* keys held for the next entry */
    uint8_t strum[STEP_CHORD], last_strum[STEP_CHORD]; /* each note's start after the pointer (chord mode) */
    uint8_t last[STEP_CHORD], last_count, tie_ok;
    uint8_t preview[STEP_CHORD], preview_count, preview_track;
    uint32_t glitch;              /* the entry glitch word (glitch.h) */
};

/* step.c: entering, rests, ties, deleting and moving. Entering calls return
 * 0, or -1 when the song has no room (nothing changes) or nothing applies. */
void step_init(struct song_step *st, struct song *s, const struct song_output *out);
void step_begin(struct song_step *st, unsigned track, uint32_t pointer);
uint32_t step_clocks(const struct song_step *st);  /* current step length */
uint32_t step_gate_clocks(const struct song_step *st, uint32_t step);
void step_key(struct song_step *st, unsigned pitch); /* an entry key went down */
/* Chord mode's root key (design 8g): the whole chord is the next entry,
 * each note `offset` clocks after the pointer, its end where the others'
 * is, so a strum enters as played. */
void step_chord(struct song_step *st, const uint8_t *pitch, const uint8_t *offset, unsigned count);
int step_commit(struct song_step *st);              /* every entry key is up */
int step_rest(struct song_step *st);
int step_repeat(struct song_step *st);
int step_tie(struct song_step *st);
int step_delete(struct song_step *st);
int step_back_delete(struct song_step *st);
void step_move(struct song_step *st, int steps);    /* by the step length */
void step_jump(struct song_step *st, int direction); /* note to note */
unsigned step_selection(const struct song_step *st, unsigned index[STEP_CHORD]);
/* Change the note at pool index `at` on the step track, journaled. */
void step_change(struct song_step *st, unsigned at, const struct song_event *e);
void step_preview_stop(struct song_step *st);

/* step_edit.c: knobs, length keys, picking and the pitch knob. */
void step_land(struct song_step *st);               /* select and preview at the pointer */
void step_set_length(struct song_step *st, int delta);
void step_pick_length(struct song_step *st, unsigned length);
void step_toggle_triplet(struct song_step *st);
void step_cycle_dots(struct song_step *st);
void step_set_velocity(struct song_step *st, int delta);
void step_set_gate(struct song_step *st, int delta);
void step_pick(struct song_step *st, unsigned pitch);
/* One glitch field on the selected notes, or the entry setting. -1 when
 * the song's glitch table is full. */
int step_set_glitch(struct song_step *st, unsigned field, int delta);
/* By scale steps (semitones when chromatic), or octaves. Drum tracks move
 * along their pad notes instead. Returns how many notes moved. */
unsigned step_transpose(struct song_step *st, int steps, unsigned octaves);
unsigned step_transpose_pads(struct song_step *st, const uint8_t *pads, unsigned count, int steps);
unsigned step_entry_velocity(struct song_step *st); /* draws a random one for R1..R4 */
const char *step_velocity_name(unsigned velocity);
void step_length_name(const struct song_step *st, char out[8]);
extern const uint8_t step_gate_percent[STEP_GATES];
#endif
