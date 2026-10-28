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
#endif
#endif
