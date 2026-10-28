/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#ifndef FM1_SONG_UNDO_H
#define FM1_SONG_UNDO_H
#include "song.h"
#include "song_config.h"

/* Multi-level undo as diffs (design section 9). An edit is a transaction of
 * records: notes added or removed (by part, song.h), a clip's length set, a
 * time shift, and when the small maps
 * (tempo, meters, markers, loop, feel, key, glitch table) changed, their earlier
 * state. Moving the loop alone is not a step, and recording settings are
 * preferences, not edits.
 * Undo applies the inverses backwards and redo replays them. The oldest
 * transactions go when the buffer is full. */
/* 32 KB since phase 6, so a job can touch about 1,300 events. 16 KB on the
 * device, where RAM is shorter: about 650. */
#if SONG_DEVICE_FULL && !defined(FM1_OPEN_WEB)
enum { SONG_UNDO_BYTES = 16384, SONG_UNDO_LEVELS = 64 };
#else
enum { SONG_UNDO_BYTES = 32768, SONG_UNDO_LEVELS = 64 };
#endif
enum { SONG_UNDO_ADD = 1, SONG_UNDO_DEL, SONG_UNDO_SHIFT, SONG_UNDO_MAPS, SONG_UNDO_SOUND, SONG_UNDO_LENGTH };
enum { SONG_SOUND_BYTES = 223 }; /* a track's sound setup, opaque here (phase 6, Copy Track) */
struct song_maps {
    struct song_tempo tempo[SONG_TEMPOS];
    struct song_meter meter[SONG_METERS];
    struct song_marker marker[SONG_MARKERS];
    uint32_t loop_start, loop_end;
    uint16_t bars;
    uint8_t tempos, meters, markers, loop_on, swing, swing_grid, swing_tracks, key_root, key_scale;
    uint8_t humanize_time[SONG_TRACKS], humanize_velocity[SONG_TRACKS];
    uint32_t glitch[32];
};
struct song_undo {
    uint8_t data[SONG_UNDO_BYTES];
    struct song_maps before;  /* the maps when the open transaction began */
    uint32_t used, done, open_at;
    uint8_t depth, paused, lost, levels, pending; /* pending: nothing written yet, redo still kept */
    uint8_t step; /* this transaction stands alone, never joined to the one before (song_undo_step) */
    void (*sound)(unsigned track, const uint8_t *setup); /* puts a sound setup back, set by the port */
};
void song_undo_init(struct song_undo *u);
void song_undo_begin(struct song_undo *u, const struct song *s); /* nests */
void song_undo_record(struct song_undo *u, unsigned op, unsigned track, const struct song_event *e,
                      uint32_t extra);
/* Closes the transaction. Returns 0, or -1 when it was too big to keep
 * (the history is then cleared, since older steps no longer line up). */
int song_undo_end(struct song_undo *u, const struct song *s);
/* The open transaction is a step of its own, even if it only changes the
 * maps like the one before (a job, not a knob turned over many polls). */
void song_undo_step(struct song_undo *u);
/* A track's sound setup before and after a change, so undo can swap it. */
void song_undo_sound(struct song_undo *u, unsigned track, const uint8_t *before, const uint8_t *after);
int song_undo(struct song_undo *u, struct song *s);  /* 0, or -1 when there is nothing */
int song_redo(struct song_undo *u, struct song *s);
unsigned song_undo_count(const struct song_undo *u, unsigned redo);
void song_maps_read(const struct song *s, struct song_maps *m);
#endif
