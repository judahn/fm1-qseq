/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* How song notes reach the engine. The engine has no timestamped input: a
 * note posted with fm1_midi_post plays at the start of the next block it
 * renders. So the player advances one block at a time and hands each
 * block's notes over before that block renders.
 *   A  the control loop posts each note one block ahead (device default).
 *   B  as A, and the audio interrupt wakes the control loop every block.
 *   C  experiment: the audio interrupt is the only caller of fm1_midi_post.
 *      Notes and live keys from the control task go through a single
 *      producer queue that the interrupt drains. The SDK says fm1_midi_post
 *      is control task only, so C is opt-in and only used on purpose.
 * The browser preview's render callback can post safely, so it defaults
 * to C, which is the exact per-block path of the earlier phases. */
#include "song_port.h"
#include "diag.h"
#include "instrument.h"
#ifndef FM1_OPEN_WEB
#include "fm1_device.h"
#endif

volatile uint8_t song_isr_mode; /* 0 off, 1 wake every block (B), 2 deliver (C) */
#ifdef FM1_OPEN_WEB
static uint8_t method = SONG_DELIVER_C;
static uint32_t sim_rendered;
#else
static uint8_t method = SONG_DELIVER_A;
#endif
static uint32_t advanced;        /* first block not yet advanced through */
static uint32_t due_block;       /* the block the notes being emitted are due in */
static uint8_t emitting, in_isr;
static struct { uint8_t data[3], count; } queue[64];
static volatile uint8_t queue_head, queue_tail; /* control task writes head, interrupt tail */

SONG_SMALL uint32_t song_port_blocks(void)
{
#ifdef FM1_OPEN_WEB
    return sim_rendered;
#else
    return fm1_audio_render_count();
#endif
}
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

/* Method C changes the player from the interrupt, so the control task masks
 * interrupts around its own player calls. Elsewhere this costs nothing. */
SONG_SMALL static uint32_t lock(void)
{
#ifndef FM1_OPEN_WEB
    if (method == SONG_DELIVER_C) return fm1_device_irq_save();
#endif
    return 0;
}
SONG_SMALL static void unlock(uint32_t state)
{
#ifndef FM1_OPEN_WEB
    if (method == SONG_DELIVER_C) fm1_device_irq_restore(state);
#endif
    (void)state;
}
SONG_SMALL static void wake_mode(void)
{
    song_isr_mode = method == SONG_DELIVER_C ? 2U : method == SONG_DELIVER_B && song_port_player()->playing ? 1U : 0U;
}
SONG_SMALL int song_port_set_method(unsigned m)
{
    if (m > SONG_DELIVER_C || song_port_player()->playing) return -1;
    method = (uint8_t)m;
    wake_mode();
    song_diag_reset();
    return 0;
}

/* Live keys and control-side notes. Under C they queue for the interrupt. */
SONG_SMALL int song_midi_post(const uint8_t *message, size_t bytes)
{
#if SONG_UI
    if (!emitting && !in_isr) song_port_key(message, bytes); /* a live key, maybe for a take */
#endif
    if (method != SONG_DELIVER_C || in_isr) return fm1_midi_post(message, bytes);
    if (!message || !bytes || bytes > 3) return -1;
    uint8_t head = queue_head, next = (uint8_t)((head + 1U) % 64U);
    if (next == queue_tail) return -2;
    for (unsigned i = 0; i < bytes; ++i) queue[head].data[i] = message[i];
    queue[head].count = (uint8_t)bytes;
    queue_head = next; /* publish after the data */
    return 0;
}
SONG_SMALL void song_deliver_note(const uint8_t message[3])
{
    (void)song_midi_post(message, 3);
    if (emitting) song_diag_late(song_port_blocks(), due_block);
}
#if SONG_UI
SONG_SMALL void song_deliver_message(const uint8_t *message, size_t bytes) { (void)song_midi_post(message, bytes); }
#endif

SONG_SMALL static void advance_to(uint32_t last)
{
    struct song_player *p = song_port_player();
    emitting = 1;
    for (; advanced <= last && p->playing; ++advanced) {
        due_block = advanced;
        song_player_advance(p, (uint64_t)(advanced + 1U) * FM1_BLOCK_FRAMES);
    }
    emitting = 0;
    if (!p->playing) wake_mode();
}
/* Methods A and B: from the control loop, through the block about to render. */
SONG_SMALL void song_deliver_poll(void)
{
    if (method != SONG_DELIVER_C && song_port_player()->playing) advance_to(song_port_blocks());
}
/* Method C, and the browser's render callback: just before block `block`
 * renders. Drains the control queue, then the song's notes for that block. */
__attribute__((noinline)) void song_isr_block(uint32_t block)
{
    in_isr = 1;
    while (queue_tail != queue_head) {
        uint8_t at = queue_tail;
        (void)fm1_midi_post(queue[at].data, queue[at].count);
        queue_tail = (uint8_t)((at + 1U) % 64U);
    }
    if (method == SONG_DELIVER_C && song_port_player()->playing) advance_to(block);
    in_isr = 0;
}
#ifdef FM1_OPEN_WEB
/* The simulator stands in for the audio interrupt, once per block. */
SONG_SMALL void song_deliver_render(uint32_t block)
{
    sim_rendered = block;
    if (method == SONG_DELIVER_C) song_isr_block(block);
    else if (method == SONG_DELIVER_B) song_deliver_poll(); /* a control wake every block */
    sim_rendered = block + 1U; /* the block renders before the control loop runs */
}
#endif

SONG_SMALL void song_deliver_start(unsigned count_in)
{
    struct song_player *p = song_port_player();
    uint32_t state = lock(), block = song_port_blocks();
    advanced = block;
#if SONG_UI
    song_player_count_in(p, (uint64_t)block * FM1_BLOCK_FRAMES, count_in);
#else
    (void)count_in;
    song_player_start(p, (uint64_t)block * FM1_BLOCK_FRAMES);
#endif
    unlock(state);
    wake_mode();
    song_deliver_poll();
}
SONG_SMALL void song_port_stop(void)
{
    uint32_t state = lock();
    song_player_stop(song_port_player());
    unlock(state);
    wake_mode();
#if SONG_UI
    song_player_chase(song_port_player(), song_port_player()->playhead); /* stopping: lanes chase too */
#endif
}
#if SONG_UI
/* Around an edit that may move the tempo map while playing. */
SONG_SMALL uint32_t song_port_hold(struct song_place *place)
{
    uint32_t state = lock();
    song_player_hold(song_port_player(), place);
    return state;
}
SONG_SMALL void song_port_resume(const struct song_place *place, uint32_t state)
{
    song_player_resume(song_port_player(), place);
    unlock(state);
}
#endif
SONG_SMALL void song_port_jump(uint32_t clock)
{
    uint32_t state = lock();
    song_player_jump(song_port_player(), clock);
    unlock(state);
#if SONG_UI
    if (!song_port_player()->playing) song_player_chase(song_port_player(), song_port_player()->playhead);
#endif
}
SONG_SMALL int song_port_set_tempo(uint32_t clock, unsigned bpm100)
{
    uint32_t state = lock();
    int result = song_player_set_tempo(song_port_player(), clock, bpm100);
    unlock(state);
    return result;
}
