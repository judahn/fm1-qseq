/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#ifndef FM1_SONG_PORT_H
#define FM1_SONG_PORT_H
#include "step.h"

/* The song and player the panel works with, and their link to the engine.
 * Notes go to the engine as MIDI on each track's input channel. Control
 * task only, except song_port_render in the browser preview. */
struct song *song_port_song(void);
struct song_player *song_port_player(void);
struct song_step *song_port_step(void); /* step entry, previewing through the engine */
int song_port_start(void);  /* 0, or -1 where playback is not wired yet */
void song_port_stop(void);
/* Browser preview: called from sim_render() before the queued MIDI drains,
 * with the first frame of the block about to render. */
void song_port_render(uint64_t frame);
/* Control poll. The device delivery stub lives here. */
void song_port_poll(void);
/* app/board/audio.c: blocks rendered so far (device only). */
uint32_t fm1_audio_render_count(void);
#endif
