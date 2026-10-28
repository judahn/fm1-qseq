/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#ifndef FM1_SONG_ARRANGE_H
#define FM1_SONG_ARRANGE_H
#include "song.h"

/* Arranging (design section 6a). The selection is a clock range (the loop
 * region) on a set of tracks. The clipboard holds one selection, relative
 * to its start, and outlives the song it came from. Inserting and deleting
 * time works on every track, with markers, tempo changes and time
 * signatures after the point moving with the music. The FX lane belongs to
 * every track: its stutters travel with any copy, paste and duplicate, and
 * a cut takes them only with track 1. Every call is all or nothing: 0, or
 * -1 when it doesn't fit or there is nothing to do. */
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
/* One copied track pastes onto `track`. Several paste back onto their own. */
int song_paste(struct song *s, const struct song_clip *clip, uint32_t at, unsigned track, unsigned insert);
/* Clipboard event i moved `at` on, its glitch found in this song's table. */
struct song_event song_board_event(struct song *s, const struct song_clip *clip, unsigned i, uint32_t at);
/* Copy the selection to right after itself, inserting, and move the
 * selection onto the copy. */
int song_duplicate(struct song *s, unsigned tracks, uint32_t *from, uint32_t *to);
int song_insert_time(struct song *s, uint32_t at, uint32_t length);
int song_delete_time(struct song *s, uint32_t from, uint32_t to);
#endif
