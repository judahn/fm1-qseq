/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* How song notes reach the engine. The engine has no timestamped input: a
 * note posted with fm1_midi_post plays at the start of the next block it
 * renders. So the player advances one block at a time, and the control loop
 * hands over each block's notes, through the block about to render, before
 * that block renders.
 *
 * Method B, which the hardware timing test chose: while a song plays or a
 * strum waits, the audio interrupt wakes the control loop after every block
 * it renders (app/board/audio.c reads song_isr_mode), so the hand-over
 * comes in the block before. On the FM-1, 1,312 of 1,361 notes came in time
 * and the other 49 one block late (docs/hardware-test-plan.md).
 *
 * Method A is the control loop alone, at its own pace, about every 3.7
 * blocks. Only the hardware test build has it, on its diagnostics page, so
 * the test can be run again. A is B without the wake, so keeping it costs
 * nothing there. The full build and the browser preview deliver by B.
 *
 * In the preview the render callback stands in for the wake: it polls just
 * before each block renders. */
#include "song_port.h"
#include "fm1.h"
#if SONG_HW_TEST
#include "diag.h"
#ifndef FM1_OPEN_WEB
#include "fm1_device.h"
#endif
#endif

volatile uint8_t song_isr_mode; /* 1: wake the control loop after every block */
#if SONG_HW_TEST
static uint8_t method = SONG_DELIVER_B;
static uint32_t due_block; /* the block the notes being sent are due in, for the diagnostics */
#define WAKING (method == SONG_DELIVER_B)
#else
#define WAKING 1
#endif
#ifdef FM1_OPEN_WEB
static uint32_t sim_rendered;
#endif
static uint32_t advanced; /* first block not yet advanced through */
static uint8_t emitting;

SONG_SMALL uint32_t song_port_blocks(void)
{
#ifdef FM1_OPEN_WEB
    return sim_rendered;
#else
    return fm1_audio_render_count();
#endif
}
#if SONG_HW_TEST
/* 24 MHz ticks: TIMER4 on the device (app/board/device.c), and a stand-in
 * from the millisecond clock in the browser. */
SONG_SMALL uint32_t song_port_ticks(void)
{
#ifdef FM1_OPEN_WEB
    return fm1_millis() * 24000U;
#else
    return fm1_device_ticks();
#endif
}
unsigned song_port_method(void) { return method; }
SONG_SMALL int song_port_set_method(unsigned m)
{
    if (m > SONG_DELIVER_B || song_port_player()->playing) return -1;
    method = (uint8_t)m;
    song_diag_reset();
    return 0;
}
#endif

/* The wake runs while there is something to send block by block. */
SONG_SMALL static void wake(void)
{
#if SONG_UI
    unsigned busy = song_port_player()->playing || song_chord_waiting();
#else
    unsigned busy = song_port_player()->playing;
#endif
    song_isr_mode = WAKING && busy ? 1U : 0U;
}

/* Live keys, and notes the control side sends. */
SONG_SMALL int song_midi_post(const uint8_t *message, size_t bytes)
{
#if SONG_UI
    if (!emitting) song_port_key(message, bytes); /* a live key, maybe for a take */
#endif
    return fm1_midi_post(message, bytes);
}
SONG_SMALL void song_deliver_note(const uint8_t message[3])
{
    (void)song_midi_post(message, 3);
#if SONG_HW_TEST
    if (emitting) song_diag_late(song_port_blocks(), due_block);
#endif
}
#if SONG_UI
/* The player's notes are never a live key, sent from the control side too
 * (a cut, a stop, a chase), so they never reach a take (0.5.1). */
SONG_SMALL void song_deliver_played(const uint8_t message[3])
{
    uint8_t was = emitting;
    emitting = 1;
    (void)song_midi_post(message, 3);
    emitting = was;
#if SONG_HW_TEST
    if (was) song_diag_late(song_port_blocks(), due_block);
#endif
}
SONG_SMALL void song_deliver_message(const uint8_t *message, size_t bytes) { (void)song_midi_post(message, bytes); }
#endif

/* Each poll: chord mode's strums, then the song, everything due before the
 * block about to render ends. */
SONG_SMALL void song_deliver_poll(void)
{
    struct song_player *p = song_port_player();
    uint32_t block = song_port_blocks();
    emitting = 1;
#if SONG_HW_TEST
    due_block = block;
#endif
#if SONG_UI
    song_chord_service(block);
#endif
    for (; advanced <= block && p->playing; ++advanced) {
#if SONG_HW_TEST
        due_block = advanced;
#endif
        song_player_advance(p, (uint64_t)(advanced + 1U) * FM1_BLOCK_FRAMES);
    }
    emitting = 0;
    wake();
}
#ifdef FM1_OPEN_WEB
/* The simulator calls this just before block `block` renders. */
SONG_SMALL void song_deliver_render(uint32_t block)
{
    sim_rendered = block;
    if (WAKING) song_deliver_poll();
    sim_rendered = block + 1U; /* the block renders before the control loop runs */
}
#endif

SONG_SMALL void song_deliver_start(unsigned count_in)
{
    struct song_player *p = song_port_player();
    uint32_t block = song_port_blocks();
    advanced = block;
#if SONG_UI
    song_player_count_in(p, (uint64_t)block * FM1_BLOCK_FRAMES, count_in);
#else
    (void)count_in;
    song_player_start(p, (uint64_t)block * FM1_BLOCK_FRAMES);
#endif
    song_deliver_poll();
}
SONG_SMALL void song_port_stop(void)
{
#if SONG_UI
    song_perf_end(); /* a performance take ends where playing got to */
#endif
    song_player_stop(song_port_player());
    wake();
#if SONG_UI
    song_player_rewind(song_port_player()); /* back to the start point (0.5.0) */
    song_player_chase(song_port_player(), song_port_player()->playhead); /* stopping: lanes chase too */
#endif
}
#if SONG_UI
SONG_SMALL int song_deliver_beat_lit(void)
{
    const struct song_player *p = song_port_player();
    return !p->playing || song_player_beat_lit(p, (uint64_t)song_port_blocks() * FM1_BLOCK_FRAMES);
}
#endif
/* Every caller moves the playhead on purpose (a knob, a marker, SHIFT+PLAY,
 * step entry, a song loaded), so the place becomes the start point too
 * (0.5.0). Playing, it is where the pending jump goes. */
SONG_SMALL void song_port_jump(uint32_t clock)
{
#if SONG_UI
    struct song_player *p = song_port_player();
    if (p->perf == 2) return; /* a performance take runs straight on */
    p->start_at = clock < song_end(p->song) ? clock : song_end(p->song);
#endif
    song_player_jump(song_port_player(), clock);
#if SONG_UI
    if (!song_port_player()->playing) song_player_chase(song_port_player(), song_port_player()->playhead);
#endif
}
SONG_SMALL int song_port_set_tempo(uint32_t clock, unsigned bpm100)
{
    return song_player_set_tempo(song_port_player(), clock, bpm100);
}
