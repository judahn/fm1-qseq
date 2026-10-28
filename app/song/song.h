/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#ifndef FM1_SONG_H
#define FM1_SONG_H
#include <stdint.h>

/* Song code runs on the control task (or once per block), so it is built
 * for size, like the stock firmware's control paths. Flash is tight. */
#define SONG_SMALL __attribute__((minsize))

/* Song model: 480 clocks per quarter, 8 tracks of time-sorted events kept in
 * one shared pool, plus tempo, time signature, marker and loop data. Storage
 * is fixed size. Nothing here allocates. */
enum {
    SONG_PPQ = 480,
    SONG_WHOLE = 4 * SONG_PPQ,
    SONG_TRACKS = 8,
    SONG_EVENTS = 4096,     /* shared by all tracks, 8 bytes each */
    SONG_TEMPOS = 32,
    SONG_METERS = 32,
    SONG_MARKERS = 16,
    SONG_MARKER_NAME = 8,
    SONG_MAX_BARS = 999,
    SONG_MAX_CLOCK = 0xffffff,
    SONG_TEMPO_MIN = 2000,  /* hundredths of a BPM */
    SONG_TEMPO_MAX = 30000,
    SONG_FRAME_RATE = 44100
};
/* Song time unit: one frame is 480 * bpm100 units, one clock is
 * SONG_UNITS_PER_CLOCK units at every tempo (44100 * 60 * 100). */
#define SONG_UNITS_PER_CLOCK 264600000ULL
static inline uint32_t song_rate(unsigned bpm100) { return 480U * bpm100; }

/* Event kinds. A note keeps its pitch in a, velocity in b and its length in
 * clocks (1..65535). A controller keeps its number in a and value in b, a
 * program change its program in a, and both have length 0. Controllers
 * and program changes are automation (lanes.h). A stutter on the FX lane
 * (glitch.h) sits on track 1, slice in a, drift in b, for its length. */
enum { SONG_NOTE, SONG_CONTROL, SONG_PROGRAM, SONG_FX };
#define SONG_FRESH 0x80000000U /* flag in `at`: recorded in the take that is running */
/* Start clock in bits 0..23 of `at`, kind in bits 24..25, a note's glitch
 * (an index into the song's glitch table, 0 for none) in bits 26..30. */
#define SONG_GLITCH_SHIFT 26
#define SONG_GLITCH_BITS (31U << SONG_GLITCH_SHIFT)
struct song_event {
    uint32_t at;
    uint8_t a, b;
    uint16_t length;
};
_Static_assert(sizeof(struct song_event) == 8, "song event layout");
static inline uint32_t song_event_start(const struct song_event *e) { return e->at & 0xffffffU; }
static inline unsigned song_event_kind(const struct song_event *e) { return e->at >> 24 & 3U; }
static inline unsigned song_event_glitch(const struct song_event *e) { return e->at >> SONG_GLITCH_SHIFT & 31U; }
static inline int song_is_note(const struct song_event *e) { return song_event_kind(e) == SONG_NOTE; }

struct song_tempo { uint32_t clock; uint16_t bpm100; };
struct song_meter { uint16_t bar; uint8_t beats, unit; }; /* from bar (0-based), beats/unit */
struct song_marker { uint32_t clock; char name[SONG_MARKER_NAME]; };
struct song_undo;
struct song {
    struct song_event event[SONG_EVENTS];
    uint16_t first[SONG_TRACKS], count[SONG_TRACKS];
    struct song_tempo tempo[SONG_TEMPOS];
    uint64_t tempo_units[SONG_TEMPOS]; /* derived segment starts, see song_time.c */
    struct song_meter meter[SONG_METERS];
    struct song_marker marker[SONG_MARKERS];
    uint32_t loop_start, loop_end, revision;
    uint16_t bars, events;
    uint8_t tempos, meters, markers, loop_on;
    /* Song settings. Swing delays off-beats by swing% (0..50) of the grid
     * step, 8th or 16th, on the tracks in swing_tracks. Key: root 0..11
     * (C = 0) and scale 0..12, 0 being chromatic. */
    uint8_t swing, swing_grid, swing_tracks, key_root, key_scale;
    /* Recording settings: mode, count-in bars (0..2), metronome (0 off,
     * 1 on, 2 while recording), input quantize (0 off, else 1 + 2 * length
     * + triplet, lengths as in step entry) and its strength in percent. */
    uint8_t rec_mode, count_in, metronome, quantize, strength;
    struct song_undo *undo;  /* edit journal, or NULL (undo.c) */
    /* The controller each of K1 to K4 records on each track, 0 for the
     * engine's default (lanes.h). */
    uint8_t knob_cc[SONG_TRACKS][4];
    /* Glitch (glitch.h): the attribute words notes point at (entry 0 is
     * none), humanize per track in clocks and velocity steps, and the seed
     * for every random choice. */
    uint32_t glitch[32];
    uint8_t humanize_time[SONG_TRACKS], humanize_velocity[SONG_TRACKS];
    uint32_t seed;
};

/* Editing (song.c). Calls return 0, or -1 when invalid or full. */
void song_clear(struct song *s, unsigned bars, unsigned bpm100);
int song_add_note(struct song *s, unsigned track, uint32_t clock, unsigned pitch,
                  unsigned velocity, unsigned length);
int song_set_tempo(struct song *s, uint32_t clock, unsigned bpm100);
int song_set_meter(struct song *s, unsigned bar, unsigned beats, unsigned unit);
int song_add_marker(struct song *s, uint32_t clock, const char *name);
int song_set_loop(struct song *s, uint32_t start, uint32_t end);
const struct song_event *song_track(const struct song *s, unsigned track, unsigned *count);
unsigned song_find(const struct song *s, unsigned track, uint32_t clock); /* first start >= clock */
int song_remove(struct song *s, unsigned track, unsigned index);
/* Journaled edits for the editing pages (song_edit.c). Indexes here are
 * into the whole pool. A shift moves every event at or after `from`. */
int song_add_event(struct song *s, unsigned track, const struct song_event *e);
int song_set_event(struct song *s, unsigned track, unsigned index, const struct song_event *e);
int song_shift(struct song *s, uint32_t from, int32_t delta);
/* Conductor changes: removing the tempo change at a clock or the time
 * signature change at a bar. The first of each stays. 0, or -1 if none. */
int song_remove_tempo(struct song *s, uint32_t clock);
int song_remove_meter(struct song *s, unsigned bar);

/* Bars, beats and snapping (song_time.c). Bars are 0-based here and 1-based
 * on screen. Zoom 0..3 snaps to a bar, half bar, beat or 8th. */
enum { SONG_ZOOMS = 4 };
uint32_t song_bar_clock(const struct song *s, unsigned bar);
unsigned song_bar_at(const struct song *s, uint32_t clock, uint32_t *start, uint32_t *length);
const struct song_meter *song_meter_at(const struct song *s, unsigned bar);
uint32_t song_end(const struct song *s);
void song_bbc(const struct song *s, uint32_t clock, unsigned out[3]); /* bar, beat, clock */
uint32_t song_snap_move(const struct song *s, uint32_t clock, unsigned zoom, int steps);
int song_marker_step(const struct song *s, uint32_t clock, int direction); /* index or -1 */
/* Swing delay in clocks for a note on this track starting at clock. Only
 * notes exactly on an off-beat of the grid move. Applied at playback. */
uint32_t song_swing(const struct song *s, unsigned track, uint32_t clock);

/* Tempo segments (song_time.c). Segment k starts at tempo[k].clock, at song
 * time tempo_units[k] in that segment's units. */
void song_tempo_refresh(struct song *s);
unsigned song_tempo_at(const struct song *s, uint32_t clock);
uint64_t song_units(const struct song *s, unsigned segment, uint32_t clock);
uint64_t song_rescale(uint64_t value, uint32_t to, uint32_t from); /* ceil(value * to / from) */

/* Key and scale (song_key.c). Chromatic means no snapping. */
enum { SONG_SCALES = 13 };
extern const char *const song_scale_names[SONG_SCALES];
extern const char *const song_key_names[12];
unsigned song_scale_mask(const struct song *s); /* pitch classes in the key, bit 0 = C */
unsigned song_scale_snap(const struct song *s, unsigned pitch); /* up to the next scale note */
unsigned song_scale_step(const struct song *s, unsigned pitch, int steps);

/* The built-in phase 1 test song (test_song.c). */
void song_build_test(struct song *s);
#endif
