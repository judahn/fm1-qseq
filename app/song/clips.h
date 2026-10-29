/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#ifndef FM1_SONG_CLIPS_H
#define FM1_SONG_CLIPS_H
#include "arrange.h"
#if SONG_UI

/* Clips (design 8f). A clip is one track's loop, notes and automation as on
 * a track, a beat to 16 bars long, kept in a cell of the grid: 8 tracks by
 * 16 rows, each cell a part of the song's event pool of its own (song.h).
 * The editor works on a clip by trading its part with its track's, so
 * everything that edits track t edits the clip instead, and undo, saving
 * and the player find every part where it is (song_owner). */
static inline unsigned song_cell(unsigned track, unsigned row) { return track * SONG_GRID_ROWS + row; }
static inline unsigned song_cell_part(unsigned cell) { return SONG_TRACKS + cell; }
uint32_t song_clip_max(const struct song *s); /* 16 bars */
/* Opens a cell in the editor, a new clip a bar long when it is empty. The
 * loop becomes the clip, and the loop end its length, which follows it on
 * every input (song_clip_follow) and when it closes. Closing puts the
 * parts and the loop back. Open: 0, or -1 when a clip is open already. */
int song_clip_open(struct song *s, unsigned cell);
void song_clip_follow(struct song *s);
void song_clip_close(struct song *s);
/* The grid's copy, cut, paste and delete on a cell (design 8f with 6a),
 * through the clipboard. A paste takes one copied track into the cell, or
 * several across its row onto their own tracks, and copied bars of the song
 * become clips there. Journaled, all or nothing: 0, or -1. */
int song_clip_copy(const struct song *s, struct song_clip *board, unsigned cell);
int song_clip_cut(struct song *s, struct song_clip *board, unsigned cell);
int song_clip_paste(struct song *s, const struct song_clip *board, unsigned cell);
int song_clip_delete(struct song *s, unsigned cell);
/* A cell's clip emptied of its events, keeping its length (0.4.0, REC in
 * REPLACE on a cell). Journaled: 0, or -1 with no clip there. */
int song_clip_empty(struct song *s, unsigned cell);
/* A cell's length set, journaled (0 empties it of its length, not its
 * events). */
void song_clip_resize(struct song *s, unsigned cell, unsigned length);
#endif
#endif
