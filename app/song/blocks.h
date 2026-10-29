/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#ifndef FM1_SONG_BLOCKS_H
#define FM1_SONG_BLOCKS_H
#include "jobs.h"
#if SONG_UI

/* Clips in the song (design 8f): each track's blocks, "clip 3 from bar 9 to
 * bar 17" or a stop, are events in its own part (song.h), so bar edits,
 * undo and saving carry them like notes. Inside a block the track plays the
 * clip in its column, looping from the block's start, or nothing for a
 * stop, and its own events are silent. A block ends at its length or where
 * the track's next one starts. Every call is journaled. */

/* A block on track t over [from, to), the clip in `row` or a stop for
 * SONG_BLOCK_STOP. Blocks starting inside go, and one reaching in from
 * before ends at `from`. 0, or -1 when it doesn't fit. */
int song_block_put(struct song *s, unsigned track, uint32_t from, uint32_t to, unsigned row);
/* A clip copied in the grid (the clipboard's cell) as a block at `at` on
 * its track, the clip's length long. -1 when it doesn't fit, -2 when the
 * clip isn't this song's (the clipboard came from another song). */
int song_block_paste(struct song *s, const struct song_clip *board, uint32_t at);
/* Blocks of one clip back to back become one, when the first is a whole
 * number of the clip's passes, so its loop goes on unbroken. */
void song_blocks_join(struct song *s, unsigned tracks);
/* The Expand job: the blocks starting in the range become plain events on
 * their tracks, what they play note for note. The track's own events they
 * kept silent go, notes reaching into one end where it starts, and the
 * controllers chase across its edges as playback does. Returns how many
 * events it added, 0 with no blocks there, or -1 when the song can't hold
 * the result, and then nothing has changed. */
int song_job_expand(struct song *s, const struct song_range *r);
/* Recording a performance from the player's log of launches (player.h):
 * each becomes a block from its launch to the track's next launch, stop or
 * return to the song, a stop as a stop block. What a track played as the
 * take began is a block from there too (0.4.0): in the grid every track
 * plays a clip or is silent, so the song gets what was heard over the
 * take, silence as stop blocks. A track on the song is left alone. `row`
 * is the block each track has going. Begin arms the player, drain writes
 * what the log holds, and end closes every block where playing got to and
 * grows the song to it. */
struct song_player;
struct song_take { uint32_t from[SONG_TRACKS]; uint8_t row[SONG_TRACKS], begun; };
void song_take_begin(struct song_take *k, struct song_player *p);
void song_take_drain(struct song_take *k, struct song_player *p);
void song_take_end(struct song_take *k, struct song_player *p);
#endif
#endif
