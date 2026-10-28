/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#ifndef FM1_SONG_UNDO_H
#define FM1_SONG_UNDO_H
#include "song.h"

/* Multi-level undo as diffs (design section 9). An edit is a transaction of
 * records: notes added or removed, a time shift, and when the small maps
 * (tempo, meters, markers, loop, feel, key, glitch table) changed, their earlier
 * state. Moving the loop alone is not a step, and recording settings are
 * preferences, not edits.
 * Undo applies the inverses backwards and redo replays them. The oldest
 * transactions go when the buffer is full. */
enum { SONG_UNDO_BYTES = 16384, SONG_UNDO_LEVELS = 64 };
enum { SONG_UNDO_ADD = 1, SONG_UNDO_DEL, SONG_UNDO_SHIFT, SONG_UNDO_MAPS };
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
};
void song_undo_init(struct song_undo *u);
void song_undo_begin(struct song_undo *u, const struct song *s); /* nests */
void song_undo_record(struct song_undo *u, unsigned op, unsigned track, const struct song_event *e,
                      uint32_t extra);
/* Closes the transaction. Returns 0, or -1 when it was too big to keep
 * (the history is then cleared, since older steps no longer line up). */
int song_undo_end(struct song_undo *u, const struct song *s);
int song_undo(struct song_undo *u, struct song *s);  /* 0, or -1 when there is nothing */
int song_redo(struct song_undo *u, struct song *s);
unsigned song_undo_count(const struct song_undo *u, unsigned redo);
void song_maps_read(const struct song *s, struct song_maps *m);
#endif
