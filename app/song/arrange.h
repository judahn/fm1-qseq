/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#ifndef FM1_SONG_ARRANGE_H
#define FM1_SONG_ARRANGE_H
#include "song.h"

/* Arranging (design section 6a). The selection is a clock range (the loop
 * region) on a set of tracks. The clipboard holds one selection, relative
 * to its start, and outlives the song it came from. Inserting and deleting
 * time works on every track, with markers, tempo changes and time
 * signatures after the point moving with the music. Every call is all or
 * nothing: 0, or -1 when it doesn't fit or there is nothing to do. */
enum { SONG_CLIP_EVENTS = 2048 };
struct song_clip {
    struct song_event event[SONG_CLIP_EVENTS];
    uint32_t glitch[32]; /* the song's glitch table when copied (glitch.h) */
    uint8_t track[SONG_CLIP_EVENTS];
    uint32_t length;
    uint16_t count;
    uint8_t tracks; /* the tracks it was copied from, one bit each */
    uint8_t cell;   /* a clip copied in the grid: its cell plus 1 (blocks.h), else 0 */
};
int song_copy(const struct song *s, struct song_clip *clip, unsigned tracks, uint32_t from, uint32_t to);
int song_cut(struct song *s, struct song_clip *clip, unsigned tracks, uint32_t from, uint32_t to);
/* The selection's events go, as a cut leaves it, with nothing copied and
 * nothing moving (0.5.5). -1 when there is nothing there. */
int song_erase(struct song *s, unsigned tracks, uint32_t from, uint32_t to);
/* The song grows to reach `to`, when something lands past its end. Not in
 * an open clip, which keeps its length. */
void song_grow(struct song *s, uint32_t to);
/* The song's length, 1 to SONG_MAX_BARS bars, from settings page 1 (0.5.8).
 * Shorter hides what's past the end without deleting it, and the loop
 * comes inside. */
void song_set_bars(struct song *s, int bars);
/* One copied track pastes onto `track`. Several paste back onto their own.
 * Over what's there, growing the song to fit, or inserting time. Clip
 * blocks pasted onto another track land as their clips' notes (0.5.7). */
int song_paste(struct song *s, const struct song_clip *clip, uint32_t at, unsigned track, unsigned insert);
/* Clipboard event i moved `at` on, its glitch found in this song's table. */
struct song_event song_board_event(struct song *s, const struct song_clip *clip, unsigned i, uint32_t at);
/* Copy the selection to right after itself, over what's there, which
 * stays where it is (0.5.5, it inserted before), and move the selection
 * onto the copy. The song grows to fit. */
int song_duplicate(struct song *s, unsigned tracks, uint32_t *from, uint32_t *to);
int song_insert_time(struct song *s, uint32_t at, uint32_t length);
int song_delete_time(struct song *s, uint32_t from, uint32_t to);
#endif
