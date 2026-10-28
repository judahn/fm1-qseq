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
#endif
};
enum { SONG_HELD = 16 }; /* sounding notes tracked per track */
struct song_held { uint32_t end; uint8_t pitch; };
/* A glitched note's repeats still to come (player_glitch.inc). */
enum { SONG_HITS = 4 };
struct song_hit { uint32_t start; uint16_t length; uint8_t pitch, velocity, repeats, next, fade, scale; int8_t drift; };
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
     * choices, each drum track's pads (set by the port), and the stutter:
     * repeats of [anchor - len, anchor) from stut_at to stut_to, NEVER
     * when off or while held. */
    struct song_hit hit[SONG_TRACKS][SONG_HITS];
    uint8_t hits[SONG_TRACKS], pad_count[SONG_TRACKS], pads[SONG_TRACKS][16];
    uint32_t pass, stut_at, stut_to, stut_anchor, stut_len;
    uint16_t stut_k[SONG_TRACKS], stut_i[SONG_TRACKS];
    int8_t stut_drift;
    uint8_t stut_running;
#endif
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
/* Start with a count-in of `bars` bars of clicks before the playhead. */
void song_player_count_in(struct song_player *p, uint64_t frame, unsigned bars);
/* The clock playing at an audio frame up to the last advance. */
uint32_t song_player_clock_at(const struct song_player *p, uint64_t frame);
/* Sends every automated lane's value as of `clock` (lanes.h). Playback
 * does this itself on start, jumps and loop wraps. */
void song_player_chase(struct song_player *p, uint32_t clock);
/* An edit that may change the tempo map under playback (undo, inserting
 * or deleting bars): hold the place first and resume after, so playing
 * goes on from the same clock and time. */
struct song_place { uint64_t inside; uint32_t at, rate; };
#if SONG_UI
/* Stutter (design 8e): from the next grid line of `slice` clocks, every
 * track repeats the slice before it, each repeat `drift` semitones on,
 * until stopped. Song time runs on, so playing rejoins in time. Stop
 * returns where it began, or 0xffffffff when it never did. */
void song_player_stutter(struct song_player *p, uint32_t slice, int drift);
void song_player_stutter_drift(struct song_player *p, int drift);
uint32_t song_player_stutter_stop(struct song_player *p);
/* Normalize's capture (player_bake.inc): the notes the tracks in the mask
 * would send from the top, rolled as the next start of `live` would roll
 * them, as plain notes with their tracks. How many, or -1 past `room`. */
int song_player_bake(const struct song_player *live, struct song *s, unsigned tracks, struct song_event *out,
                     uint8_t *track, unsigned room);
#endif
void song_player_hold(const struct song_player *p, struct song_place *place);
void song_player_resume(struct song_player *p, const struct song_place *place);
#endif
