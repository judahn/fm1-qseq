/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#ifndef FM1_SONG_PORT_H
#define FM1_SONG_PORT_H
#include "step.h"
#include "song_config.h"
#include <stddef.h>

/* The song and player the panel works with, and their link to the engine.
 * Notes go to the engine as MIDI on each track's input channel. Control
 * task only, except song_port_render in the browser preview. */
struct song *song_port_song(void);
struct song_player *song_port_player(void);
struct song_step *song_port_step(void); /* step entry, previewing through the engine */
int song_port_start(void);  /* 0, or -1 where playback is not wired yet */
void song_port_stop(void);
void song_port_jump(uint32_t clock);                 /* player calls that are safe under method C */
int song_port_set_tempo(uint32_t clock, unsigned bpm100);
/* Browser preview: called from sim_render() before the queued MIDI drains,
 * with the first frame of the block about to render. */
void song_port_render(uint64_t frame);
/* Control poll: diagnostics timing and delivery methods A and B. */
void song_port_poll(void);
/* app/board/audio.c: blocks rendered so far (device only). */
uint32_t fm1_audio_render_count(void);

/* Note delivery (song_deliver.c). */
enum { SONG_DELIVER_A, SONG_DELIVER_B, SONG_DELIVER_C };
unsigned song_port_method(void);
int song_port_set_method(unsigned method);           /* while stopped */
uint32_t song_port_blocks(void);                     /* blocks rendered so far */
uint32_t song_port_ticks(void);                      /* 24 MHz */
int song_midi_post(const uint8_t *message, size_t bytes); /* live keys: queued under method C */
void song_deliver_note(const uint8_t message[3]);
void song_deliver_poll(void);
void song_deliver_start(unsigned count_in);
void song_deliver_render(uint32_t block);            /* browser preview only */
/* The audio interrupt reads song_isr_mode after each render: 1 wakes the
 * control loop (method B), 2 calls song_isr_block before rendering (C). */
extern volatile uint8_t song_isr_mode;
void song_isr_block(uint32_t block);
#if SONG_UI
/* Editing: the undo journal, the clipboard and real-time recording. */
struct song_undo *song_port_undo(void);
struct song_clip *song_port_clip(void);
struct song_recorder *song_port_recorder(void);
int song_port_record(unsigned mode, unsigned track); /* with count-in and metronome */
void song_port_record_stop(void);
void song_port_key(const uint8_t *message, size_t bytes); /* a live key on its way to the engine */
int64_t song_port_live_clock(void);                   /* where a key or knob lands now, -1 in a count-in */
void song_deliver_message(const uint8_t *message, size_t bytes); /* automation, as notes go */
uint32_t song_port_lock(void);                        /* the player, under delivery method C */
void song_port_unlock(uint32_t state);
uint32_t song_port_hold(struct song_place *place);    /* player.h, with the player locked meanwhile */
void song_port_resume(const struct song_place *place, uint32_t state);
/* K1 to K4 while recording (song_knobs.c): each drives a lane on the take
 * track, chosen per track (lanes.h), recorded in touch style. */
unsigned song_port_engine(unsigned track);
unsigned song_knob_lane(unsigned track, unsigned knob);
unsigned song_knob_value(unsigned track, unsigned knob); /* what it is at or setting now */
unsigned song_knob_held(unsigned knob);
void song_knob_turn(unsigned knob, int delta, uint32_t now);
void song_knobs_poll(uint32_t now);      /* lets go of knobs that stopped moving */
void song_knobs_release(uint32_t clock); /* lets go of every knob: the take ends */
void song_knobs_wrap(uint32_t loop_end); /* a loop pass ends */
/* GLITCH held in the song view (song_knobs.c): the stutter, slice 0..4 for
 * 1/4 to 1/64, plus 8 for a triplet. A take records it on the FX lane. */
void song_stutter_hold(unsigned slice, int drift);   /* starts, or a new slice */
void song_stutter_drift(int drift);
void song_stutter_release(void);
unsigned song_stuttering(void);
#endif
#endif
