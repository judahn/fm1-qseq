/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#ifndef FM1_SONG_PLAYER_H
#define FM1_SONG_PLAYER_H
#include "song.h"

/* Song player. Its time base is audio frames. Advancing to frame N emits
 * every event due before N, so a caller that advances to the end of each
 * block puts every note on its exact block. Song time is carried in tempo
 * segment units, so remainders carry from block to block and nothing drifts. */
struct song_output {
    /* Velocity 0 is a note-off. Controllers join with automation. */
    void (*note)(void *context, unsigned track, unsigned pitch, unsigned velocity);
    void *context;
};
enum { SONG_HELD = 16 }; /* sounding notes tracked per track */
struct song_held { uint32_t end; uint8_t pitch; };
struct song_player {
    struct song *song;
    struct song_output out;
    uint64_t frame, pos;        /* audio frame, and song time at that frame */
    uint32_t next;              /* clocks below this have been emitted */
    uint32_t playhead;          /* position while stopped */
    uint32_t jump, jump_at;     /* pending jump and the bar line it waits for */
    uint32_t revision;          /* song revision the cursors belong to */
    uint8_t playing, jumping, segment;
    uint8_t held_count[SONG_TRACKS];
    uint16_t cursor[SONG_TRACKS];
    uint16_t late_first[SONG_TRACKS], late_count[SONG_TRACKS]; /* swung notes not yet due */
    struct song_held held[SONG_TRACKS][SONG_HELD];
};
void song_player_init(struct song_player *p, struct song *s, const struct song_output *out);
/* Start at the playhead. The first event lands in the block starting at frame. */
void song_player_start(struct song_player *p, uint64_t frame);
/* Stop where playback is, with a note-off for every sounding note. */
void song_player_stop(struct song_player *p);
void song_player_advance(struct song_player *p, uint64_t frame);
/* Stopped: move at once. Playing: wait for the next bar line. */
void song_player_jump(struct song_player *p, uint32_t clock);
uint32_t song_player_clock(const struct song_player *p);
/* Change the tempo at a clock, keeping playback in time from where it is. */
int song_player_set_tempo(struct song_player *p, uint32_t clock, unsigned bpm100);
/* Where a jump requested now would leave from (the next bar line), or the
 * pending target. Stopped: the playhead. */
uint32_t song_player_target(const struct song_player *p);
#endif
