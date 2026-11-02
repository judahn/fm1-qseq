/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#ifndef FM1_SONG_PLAYER_H
#define FM1_SONG_PLAYER_H
#include "song.h"
#include "song_config.h"

/* Song player. Its time base is audio frames. Advancing to frame N emits
 * every event due before N, so a caller that advances to the end of each
 * block puts every note on its exact block. Song time is carried in tempo
 * segment units, so remainders carry from block to block and nothing drifts. */
struct song_output {
    void (*note)(void *context, unsigned track, unsigned pitch, unsigned velocity); /* velocity 0: off */
    void *context;
#if SONG_UI
    /* Automation: a controller (status 0xb0, number, value) or a program
     * change (0xc0, program). NULL leaves it out. */
    void (*control)(void *context, unsigned track, unsigned status, unsigned a, unsigned b);
    /* A sampler track's note about to start (0.6.6): the semitones its pad
     * plays at, from the note's TUNE, 0 for none. NULL leaves it out. */
    void (*tune)(void *context, unsigned track, unsigned pitch, int semitones);
#endif
};
enum { SONG_HELD = 16 }; /* sounding notes tracked per track */
struct song_held { uint32_t end; uint8_t pitch; };
/* A glitched note's repeats still to come (player_glitch.inc). */
enum { SONG_HITS = 4, SONG_MOVED = 8 };
struct song_hit { uint32_t start; uint16_t length; uint8_t pitch, velocity, repeats, next, fade, scale; int8_t drift, tune; };
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
    /* Recording support on the editing pages: older notes on the take track
     * stay quiet in [mute_from, mute_to), the metronome, and the count-in
     * (clicks before the song starts, at exact frames). */
    uint64_t pre_origin, pre_units;
    uint32_t mute_from, mute_to, click_next, click_off, pre_rate;
    uint16_t pre_left, pre_done;
    uint8_t mute_track, click, click_track, pre_beats;
#if SONG_UI
    /* Lanes a knob is recording on touch_track play nothing from the song.
     * program: the last program change sent per track, 0xff for none. */
    uint8_t touch_track, touch_lane[4], program[SONG_TRACKS];
    /* Glitch: repeats due per track, the pass count that seeds the random
     * choices, each drum track's pads, and the sampler tracks, whose notes
     * play their TUNE (set by the port). */
    struct song_hit hit[SONG_TRACKS][SONG_HITS];
    uint8_t hits[SONG_TRACKS], pad_count[SONG_TRACKS], pads[SONG_TRACKS][16], samplers;
    uint32_t pass;
    /* Notes moved on past the playhead this pass (0.6.7, song_player_moved):
     * each stays quiet at its new clock until playing gets there, or the
     * pass ends. */
    struct song_moved { uint32_t at; uint8_t track, pitch; } moved[SONG_MOVED];
    uint8_t moved_count;
    /* Clips (design 8f, player_clips.inc): what each track plays, a row of
     * the grid, SONG_FROM_SONG or SONG_FROM_STOP, a launch waiting for its
     * clock, and where the clip's pass began. Those are transport clocks:
     * the song clock plus `shift`, running on through loops and jumps, so
     * clips keep their phase. A clip's notes are timed on the transport.
     * `home` is what every track plays when it isn't launched (0.4.0): the
     * song in the song view, nothing in the grid (song_player_view). */
    uint32_t shift, base[SONG_TRACKS], launch_at[SONG_TRACKS];
    uint8_t from[SONG_TRACKS], waiting[SONG_TRACKS], home;
    /* A track on the song inside a clip block (blocks.h) plays that clip,
     * row in blk (SONG_FROM_STOP for a stop, SONG_FROM_NONE outside one),
     * from base, a song clock here, until blk_end. blk_next is the track's
     * next block from its cursor. */
    uint32_t blk_end[SONG_TRACKS];
    uint16_t blk_next[SONG_TRACKS];
    uint8_t blk[SONG_TRACKS];
    /* The clip open in the editor plays on its track, its passes from the
     * transport clock edit_base, while the song plays on. The editor works
     * in the clip's time: song_player_clock, the playhead, jumps and the
     * target are in it, and the song's own place while stopped is song_at.
     * A jump while playing moves the clip's pass at the next bar line. */
    uint32_t edit_base, song_at;
    /* The start point (0.5.0): where PLAY starts and every stop comes back
     * to, in the editor's time, and the song's own while a clip is open.
     * Every jump made on purpose sets it (song_port_jump). */
    uint32_t start_at, start_song;
    /* Recording a performance (design 8f): perf is 1 while armed and 2 once
     * the take runs, when the song's loop and end are ignored. Every launch
     * that takes effect meanwhile is logged, at its song clock, and what
     * each track played as the take began is kept in `began`, from song
     * clock began_at (0.4.0). */
    struct song_launch { uint32_t clock; uint8_t track, to; } log[16];
    uint32_t began_at;
    uint8_t perf, log_in, log_out, began[SONG_TRACKS];
    /* A live take on the song, in REPLACE or OVERDUB, runs on past the
     * song's end, and the take grows the song as it goes (0.5.8). */
    uint8_t open_end;
#endif
};
#if SONG_UI
enum { SONG_FROM_SONG = 0xff, SONG_FROM_STOP = 0xfe, SONG_FROM_NONE = 0xfd };
#endif
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
/* Start with a count-in of `bars` bars of clicks before the playhead. */
void song_player_count_in(struct song_player *p, uint64_t frame, unsigned bars);
/* The clock playing at an audio frame up to the last advance. */
uint32_t song_player_clock_at(const struct song_player *p, uint64_t frame);
#if SONG_UI
int song_player_beat_lit(const struct song_player *p, uint64_t frame); /* PLAY's light, playing (0.5.1) */
#endif
/* Sends every automated lane's value as of `clock` (lanes.h). Playback
 * does this itself on start, jumps and loop wraps. */
void song_player_chase(struct song_player *p, uint32_t clock);
/* An edit that may change the tempo map under playback (undo, inserting
 * or deleting bars): hold the place first and resume after, so playing
 * goes on from the same clock and time. */
struct song_place { uint64_t inside; uint32_t at, rate; };
#if SONG_UI
/* Normalize's capture (player_bake.inc): the notes the tracks in the mask
 * would send from the top, rolled as the next start of `live` would roll
 * them, as plain notes with their tracks. A sampler note's TUNE, which no
 * plain note can hold, rides in bits 26 to 31 of its `at` (0.6.6). How
 * many, or -1 past `room`. */
int song_player_bake(const struct song_player *live, struct song *s, unsigned tracks, struct song_event *out,
                     uint8_t *track, unsigned room);
/* Launching (design 8f): a track to a row of the grid, back to the song or
 * stopped, at the song's launch quantize from where playing has reached,
 * exactly on that clock. Stopped, it takes over as playing starts. A track
 * playing a clip or stopped stays that way through loops and jumps, until
 * it is launched again or playing stops, which hands every track back to
 * `home`. */
void song_player_launch(struct song_player *p, unsigned track, unsigned to);
/* Playback follows the view (0.4.0). The song view (grid 0) plays only the
 * song: every track goes back to it. The grid (1) plays only launched
 * clips: every track on the song stops, unless a clip's launch waits for
 * it, and a return to the song still waiting is called off. Playing, that
 * happens at the next launch point, and stopped, at once. In the grid the
 * transport goes round at the song's end instead of stopping. */
void song_player_view(struct song_player *p, unsigned grid);
/* A launch for recording into the cell (0.3.0): the same, but with the
 * launch quantize off it waits for the next beat. */
void song_player_launch_rec(struct song_player *p, unsigned track, unsigned row);
/* The transport clock playing now. */
uint32_t song_player_transport(const struct song_player *p);
/* After a clip opens in the editor (opening) or closes, with its track:
 * playing, the clip comes in at once in step with the song's bar, and on
 * closing the track goes back to what it played. A clip the track plays as
 * launched keeps its pass going as it opens, and a track left on the open
 * clip's pass (from and base set to it) plays on through the close.
 * Stopped, the playhead moves between the clip's time and the song's, and
 * the start point does too, the clip's beginning at its top. */
void song_player_edit(struct song_player *p, unsigned track, unsigned opening);
/* A track's notes, repeats and waiting swung notes end now. */
void song_player_cut(struct song_player *p, unsigned track);
/* A note on the track moved while playing (0.6.7, step entry's MOVE and
 * NUDGE), from clock `from` to `to` in the track's own clocks (the open
 * clip's on its track). Moved on past the playhead it has sounded this
 * pass already, so it waits for the next, and moved again it keeps
 * waiting. A note held sounding ends when it was due to, as after any
 * edit, and one moved back past the playhead plays from the next pass. */
void song_player_moved(struct song_player *p, unsigned track, unsigned pitch, uint32_t from, uint32_t to);
/* Stopped: the playhead back at the start point, kept inside the song (or
 * the open clip's timeline). Playing to the song's end does it too. */
void song_player_rewind(struct song_player *p);
#endif
void song_player_hold(const struct song_player *p, struct song_place *place);
void song_player_resume(struct song_player *p, const struct song_place *place);
#endif
