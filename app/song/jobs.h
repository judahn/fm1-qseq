/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#ifndef FM1_SONG_JOBS_H
#define FM1_SONG_JOBS_H
#include "song.h"
#include "arrange.h"

/* Edit jobs (phase 6), after the QY70's song jobs, on the 6a selection: the
 * clocks [from, to) on the tracks in `tracks`. An event belongs to the range
 * by its start. Drum tracks (`drums`: 808 and sampler, whose keys are pads)
 * are left alone by the pitch jobs. Every job goes through the journal, so
 * the caller's transaction makes it one undo step, and every job is all or
 * nothing: it returns how many events it changed, 0 when there was nothing
 * to do, or -1 when the result would not fit (events, tempo changes, bars)
 * or the settings don't apply, and then nothing has changed. */
struct song_range { uint32_t from, to; uint8_t tracks, drums; };
enum { SONG_JOB_SET, SONG_JOB_RATE, SONG_JOB_LEGATO };            /* velocity and gate modes */
enum { SONG_JOB_NOTES = 1, SONG_JOB_CONTROLS = 2, SONG_JOB_PROGRAMS = 4 }; /* event types */
enum { SONG_JOB_TEMPO = 200 };                                    /* a ramp's lane: tempo, not a controller */

/* Notes (jobs_notes.c). Quantize: `grid` in clocks (a 64th triplet, 20, to a
 * whole note, 1920), toward the nearest grid line of the note's bar by
 * `strength` percent. `swing` 50 to 83 puts every second grid line that far
 * through its pair (50 is straight). `gate` quantizes the note ends too. */
int song_job_quantize(struct song *s, const struct song_range *r, unsigned grid, unsigned strength, unsigned swing,
                      unsigned gate);
int song_job_velocity(struct song *s, const struct song_range *r, unsigned mode, unsigned value, int offset); /* SET or RATE % */
int song_job_crescendo(struct song *s, const struct song_range *r, int amount); /* 0 at `from`, `amount` at `to` */
int song_job_gate(struct song *s, const struct song_range *r, unsigned mode, unsigned value, int offset);
int song_job_transpose(struct song *s, const struct song_range *r, int amount, unsigned scale_steps, unsigned low,
                       unsigned high);
int song_job_fit_scale(struct song *s, const struct song_range *r); /* up to the next note of the key */
int song_job_shift(struct song *s, const struct song_range *r, int clocks); /* kept inside the range */
int song_job_chord_sort(struct song *s, const struct song_range *r, unsigned down);
/* Strum: the notes of a chord, in sort order, start `clocks` apart. Their
 * ends stay where they were. */
int song_job_strum(struct song *s, const struct song_range *r, unsigned clocks, unsigned down);

/* Events (jobs_events.c). Ramp: a lane (a controller, or SONG_JOB_TEMPO in
 * BPM) from `start` to `end`, a point every `spacing` clocks, the curve -16
 * (fast start) to 16 (slow start). It replaces that lane in the range. */
int song_job_erase(struct song *s, const struct song_range *r, unsigned types);
int song_job_extract(struct song *s, const struct song_range *r, unsigned type, unsigned to_track);
int song_job_ramp(struct song *s, const struct song_range *r, unsigned lane, int start, int end, unsigned spacing,
                  int curve);
int song_job_thin(struct song *s, const struct song_range *r, unsigned lane); /* every other change goes */
/* Whole tracks: copy (replacing), merge (`from` ends up empty), clear. Copy
 * leaves the clip blocks where they are. */
int song_job_copy_track(struct song *s, unsigned from, unsigned to);
int song_job_merge(struct song *s, unsigned from, unsigned into);
int song_job_clear(struct song *s, unsigned tracks);
/* The clipboard `times` times from `at`, over what is there, copy k
 * transposed k * `transpose` semitones (drum tracks never). The song grows
 * to fit. */
int song_job_paste(struct song *s, const struct song_clip *clip, uint32_t at, unsigned track, unsigned times,
                   int transpose, unsigned drums);

/* Time (jobs_time.c). Stretch: the range's events 50 to 200 percent, gates
 * too. Later events, markers and tempo changes move by the difference, as
 * inserting or deleting time does, and the selection follows. Shrinking
 * needs the tracks left out to have nothing in the part that closes. When
 * every track is in, markers and tempo changes inside scale too. */
int song_job_stretch(struct song *s, const struct song_range *r, unsigned rate);
struct song_player;
/* Normalize: swing, humanize and the glitch attributes of whole tracks
 * become plain notes, exactly as the next play from the top would roll
 * them (player_bake.inc), and the tracks' swing and humanize go off. */
int song_job_normalize(struct song *s, unsigned tracks, const struct song_player *live);
#endif
