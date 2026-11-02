/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#ifndef FM1_SONG_STEP_H
#define FM1_SONG_STEP_H
#include "player.h"
#include "undo.h"

/* Step entry (design section 4). The pointer is the selection: the notes on
 * the track that start exactly at the pointer, all of them unless narrowed
 * by picking pitches, or since 0.5.0 a sweep's notes (step_range.c). Knob
 * edits change the entry settings and, when the selection holds notes,
 * rewrite them too. Model code only: the panel maps keys to pitches, and
 * previews go out through `out`. */
enum { STEP_64TH, STEP_32ND, STEP_16TH, STEP_8TH, STEP_QUARTER, STEP_HALF, STEP_WHOLE, STEP_LENGTHS };
enum { STEP_GATES = 4, STEP_RANDOMS = 4, STEP_CHORD = 16 };
/* The notes MOVE and NUDGE took (0.6.8, step_ids.c): as many as undo could
 * take back in one step, the edit jobs' reckoning, and the notes a run of
 * them covers, put back as it leaves. */
enum { STEP_IDS = (SONG_UNDO_BYTES - 512 - (int)sizeof(struct song_maps)) / 24, STEP_COVERS = 64 };
struct song_step {
    struct song *song;
    struct song_output out;       /* previews of the notes the pointer lands on */
    uint32_t pointer, last_at, random;
    uint32_t pick[4];             /* narrowed selection, one bit per pitch */
    uint16_t serial;              /* counts previews, so the panel can time them out */
    uint8_t track, length, triplet, dots, gate, velocity, random_level, picked, ranged;
    uint8_t chord[STEP_CHORD], chord_count;   /* keys held for the next entry */
    uint8_t strum[STEP_CHORD], last_strum[STEP_CHORD]; /* each note's start after the pointer (chord mode) */
    uint8_t last[STEP_CHORD], last_count, tie_ok;
    uint8_t preview[STEP_CHORD], preview_count, preview_track;
    uint32_t glitch;              /* the entry glitch word (glitch.h) */
    uint32_t anchor;              /* where a sweep began, while ranged (step_range.c) */
    /* Since a turn of MOVE or NUDGE (0.6.8): the notes it moved, by start
     * and pitch (start << 8 | pitch), sorted, are the selection until it
     * changes, and the notes its run has covered wait to be put back. */
    uint16_t ids, covers;
    uint32_t id[STEP_IDS];
    struct song_event cover[STEP_COVERS];
};

/* step.c: entering, rests, ties, deleting and moving. Entering calls return
 * 0, or -1 when the song has no room (nothing changes) or nothing applies.
 * Each of them, and step_begin, ends a selection by identity (step_ids.c). */
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
void step_move(struct song_step *st, int steps);    /* line to line on the step grid (0.6.7) */
void step_jump(struct song_step *st, int direction); /* note to note */
/* PRESETS with SEL held (0.5.0, K4 until 0.5.2, LFO until 0.5.6): the pointer moves, by
 * steps or note to note, and the selection runs from where the sweep began
 * (step_range.c). */
void step_sweep(struct song_step *st, int steps, unsigned jump);
unsigned step_selection(const struct song_step *st, unsigned index[STEP_CHORD]);
/* Change the note at pool index `at` on the step track, journaled. */
void step_change(struct song_step *st, unsigned at, const struct song_event *e);
void step_preview_stop(struct song_step *st);

/* step_edit.c: knobs, length keys and picking. The edits return 0, or
 * STEP_TOO_BIG when a swept selection is more than undo could take back,
 * and then nothing changed, the entry settings included. */
enum { STEP_TOO_BIG = -2 };
void step_land(struct song_step *st);               /* select and preview at the pointer */
void step_preview(struct song_step *st);            /* sound the notes selected at the pointer */
int step_set_length(struct song_step *st, int delta);
int step_pick_length(struct song_step *st, unsigned length);
int step_toggle_triplet(struct song_step *st);
int step_cycle_dots(struct song_step *st);
int step_set_velocity(struct song_step *st, int delta);
int step_set_gate(struct song_step *st, int delta);
/* At the pointer a key adds its pitch to the picked notes. In a sweep the
 * first key picks only its pitch, and later ones toggle theirs. */
void step_pick(struct song_step *st, unsigned pitch);
/* One glitch field on the selected notes, or the entry setting. -1 when
 * the song's glitch table is full. */
int step_set_glitch(struct song_step *st, unsigned field, int delta);
/* step_pitch.c: by scale steps (semitones when chromatic), or octaves.
 * Drum tracks move along their pad notes instead. Returns how many notes
 * moved, or STEP_TOO_BIG. */
int step_transpose(struct song_step *st, int steps, unsigned octaves);
int step_transpose_pads(struct song_step *st, const uint8_t *pads, unsigned count, int steps);
/* step_range.c (0.5.0): the swept selection, every note that starts from
 * the anchor to the pointer, both included, at the pitches picked in it if
 * any. The clocks [from, to) and 1 while the pointer is off the anchor, and
 * how many notes are selected, swept or at the pointer. Too big: undo couldn't hold a
 * change to every one (the edit jobs' reckoning, with room for the maps). */
int step_range(const struct song_step *st, uint32_t *from, uint32_t *to);
unsigned step_count(const struct song_step *st);
int step_too_big(const struct song_step *st);
/* A note the selection holds: the moved notes while there are some
 * (0.6.8), else every one or the picked pitches. */
int step_chosen(const struct song_step *st, const struct song_event *e);
/* The swept selection's edits, for step.c and step_edit.c. */
int step_range_delete(struct song_step *st);
void step_range_velocity(struct song_step *st, int delta);
void step_range_scale(struct song_step *st, uint32_t before, uint32_t after); /* lengths times after / before */
int step_range_glitch(struct song_step *st, unsigned field, int delta);
/* step_move.c (0.6.7): the step grid, MOVE and NUDGE. The grid is the
 * multiples of the step length counted from each bar line, and the bar
 * lines themselves (a clip's bars count from its start). step_grid_move goes
 * from line to line, `steps` of them, and step_offset is how far the
 * pointer sits from its nearest line, in clocks, after it positive.
 * step_nudge moves the selected notes whole by `clocks`, the pointer with
 * them and a sweep's anchor too, and from then on they are the selection
 * by identity (0.6.8), passing over other notes without taking them in. A
 * note landed on, of a moved note's pitch at its clock, is replaced, and
 * within a run (`more`, the turn before being one of it) put back once the
 * moved notes leave its place. Playing, the player hears of each moved
 * note (song_player_moved). It returns 0, -1 with nothing selected, or a
 * refusal, and then nothing moved: STEP_TOO_BIG (undo's reckoning, the
 * notes replaced counted in), STEP_START before clock 0, or STEP_END past
 * a clip's end or past where entering a note could reach. */
enum { STEP_NUDGE = 5, STEP_START = -4, STEP_END = -5 };
uint32_t step_grid_move(const struct song_step *st, uint32_t clock, int steps);
int step_offset(const struct song_step *st);
int step_nudge(struct song_step *st, int32_t clocks, unsigned more, struct song_player *p);
/* step_ids.c (0.6.8): the selection by identity. step_forget ends it and
 * the run's covered notes with it, and step_repitch keeps it in step when
 * a moved note's pitch changes at `clock`. */
uint32_t step_id_of(uint32_t start, unsigned pitch);
int step_has_id(const struct song_step *st, uint32_t id);
void step_forget(struct song_step *st);
void step_repitch(struct song_step *st, uint32_t clock, unsigned from, unsigned to);
unsigned step_take(struct song_step *st, uint32_t from, uint32_t to, int32_t clocks);
void step_cover(struct song_step *st, unsigned index);
void step_uncover(struct song_step *st);
unsigned step_entry_velocity(struct song_step *st); /* draws a random one for R1..R4 */
const char *step_velocity_name(unsigned velocity);
void step_length_name(const struct song_step *st, char out[8]);
extern const uint8_t step_gate_percent[STEP_GATES];
#endif
