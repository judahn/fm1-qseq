/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Song glue: owns the song, the player and step entry, and hands notes to
 * the engine as MIDI on each track's input channel (song_deliver.c says how).
 * Live recording is in song_takes.c. */
#include "song_port.h"
#include "instrument.h"
#if SONG_HW_TEST
#include "diag.h"
#endif
#include <string.h>
#if SONG_UI
#include "undo.h"
#include "record.h"
#include "glitch.h"
#include "clips.h"
static struct song_undo undo;
static struct song_clip clip;
#endif

_Static_assert(SONG_FRAME_RATE == FM1_SAMPLE_RATE, "song frame rate");
_Static_assert(SONG_TRACKS == GROOVE_TRACKS, "song tracks");

static struct song song;
static struct song_player player;
#if SONG_UI
static struct song_step step;
#endif
static uint8_t ready;
#ifdef FM1_OPEN_WEB
static void capture(unsigned track, unsigned pitch, unsigned velocity, unsigned kind);
static uint32_t start_block, start_clock;
/* Where a take began playing, for the headless check (song_takes.c). */
void song_port_mark_start(void) { start_block = song_port_blocks(); start_clock = player.playhead; }
#endif

SONG_SMALL static void deliver(void *context, unsigned track, unsigned pitch, unsigned velocity)
{
    (void)context;
    unsigned channel = groove_midi_input_channel(track);
    /* Play and Off tracks ignore arrangement playback, as with the stock
     * sequencer. Note-offs always go out. */
    if (channel > 15 || (velocity && groove_track_role(track) != GROOVE_TRACK_NORMAL)) return;
    const uint8_t message[3] = {(uint8_t)((velocity ? 0x90U : 0x80U) | channel), (uint8_t)pitch,
                                (uint8_t)velocity};
    song_deliver_note(message);
#ifdef FM1_OPEN_WEB
    capture(track, pitch, velocity, 0);
#endif
}
#if SONG_UI
/* Automation takes the same path, on Normal tracks only. */
SONG_SMALL static void deliver_control(void *context, unsigned track, unsigned status, unsigned a, unsigned b)
{
    (void)context;
    unsigned channel = groove_midi_input_channel(track);
    if (channel > 15 || groove_track_role(track) != GROOVE_TRACK_NORMAL) return;
    const uint8_t message[3] = {(uint8_t)(status | channel), (uint8_t)a, (uint8_t)b};
    song_deliver_message(message, status == 0xc0 ? 2U : 3U);
#ifdef FM1_OPEN_WEB
    capture(track, a, b, status);
#endif
}
/* Each drum track's pads, for glitch's pad swap, as playing starts. */
SONG_SMALL static void pads(void)
{
    for (unsigned t = 0; t < SONG_TRACKS; ++t) {
        unsigned n = groove_pad_count(t);
        player.pad_count[t] = (uint8_t)(n < 16 ? n : 16);
        for (unsigned i = 0; i < player.pad_count[t]; ++i) player.pads[t][i] = (uint8_t)groove_pad_note(t, i);
    }
}
void song_port_ready_pads(void) { pads(); } /* Normalize rolls pad swaps as playing would */
/* Undo's hook for Copy Track's sound (undo.h). */
SONG_SMALL static void put_sound(unsigned track, const uint8_t *setup) { (void)groove_timbre_import(track, setup, SONG_SOUND_BYTES); }
#endif
SONG_SMALL static void ensure(void)
{
    if (ready) return;
#if SONG_UI
    static const struct song_output output = {deliver, NULL, deliver_control};
#else
    static const struct song_output output = {deliver, NULL};
#endif
    song_build_test(&song);
    song_player_init(&player, &song, &output);
#if SONG_UI
    step_init(&step, &song, &output);
#endif
#if SONG_HW_TEST
    song_diag_reset();
    song_diag.heaviest = -1;
#endif
#if SONG_UI
    song_undo_init(&undo);
    undo.sound = put_sound;
    song.undo = &undo; /* after the test song, which is not an edit */
    song.strength = 100; /* recording defaults: full strength, a bar of count-in, clicks while recording */
    song.count_in = 1;
    song.metronome = 2;
#endif
    ready = 1;
}
struct song *song_port_song(void) { ensure(); return &song; }
struct song_player *song_port_player(void) { ensure(); return &player; }
#if SONG_UI
struct song_step *song_port_step(void) { ensure(); return &step; }
#endif

/* Each control poll: timing for the diagnostics page, then the delivery. */
SONG_SMALL void song_port_poll(void)
{
    ensure();
#if SONG_HW_TEST
    song_diag_poll(song_port_ticks(), fm1_millis(), song_port_blocks());
#endif
    song_deliver_poll();
#if SONG_UI
    song_takes_poll();
    song_perf_poll();
    song_tempo_follow(0);
    song_knobs_poll(fm1_millis());
    song_store_poll(fm1_millis());
#endif
}

SONG_SMALL int song_port_start(void)
{
    ensure();
#ifdef FM1_OPEN_WEB
    start_block = song_port_blocks();
    start_clock = player.playhead;
#endif
#if SONG_HW_TEST
    song_diag_reset();
#endif
#if SONG_UI
    player.click = song.metronome == 1;
    pads();
#endif
    song_deliver_start(0);
    return 0;
}

#if SONG_UI
struct song_undo *song_port_undo(void) { ensure(); return &undo; }
struct song_clip *song_port_clip(void) { return &clip; }

/* Clips. Opening one while playing: it plays at once on its track, in step
 * with the bar the song is in (or on in its pass, launched there already),
 * and the song plays on. Closing gives the track back to what it played. */
SONG_SMALL void song_port_launch(unsigned track, unsigned to)
{
    song_player_launch(&player, track, to);
}
SONG_SMALL int song_port_clip_open(unsigned cell)
{
    int r = song_clip_open(&song, cell);
    if (!r) {
        player.waiting[cell / SONG_GRID_ROWS] = SONG_FROM_NONE;
        song_player_edit(&player, cell / SONG_GRID_ROWS, 1);
    }
    return r;
}
SONG_SMALL void song_port_clip_close(void)
{
    if (!song.open) return;
    unsigned t = song_editing(&song);
    song_port_clip_disarm();
    song_port_recorder()->taken = 0; /* a take into the clip can't be requantized from the track */
    song_clip_close(&song);
    song_player_edit(&player, t, 0);
}
/* The battery (0.3.0): the panel's reading each poll, smoothed, and what
 * the song list shows, taken from it at most once a second. */
static uint16_t battery_smooth, battery_shown;
static uint32_t battery_at;
SONG_SMALL void song_port_battery(unsigned raw, uint32_t now)
{
    battery_smooth = (uint16_t)(battery_smooth ? battery_smooth + raw - (battery_smooth >> 4) : raw << 4);
    if (!battery_shown || now - battery_at >= 1000U) { battery_shown = (uint16_t)((battery_smooth + 8U) >> 4); battery_at = now; }
}
unsigned song_port_battery_shown(void) { return battery_shown; }
#endif

#ifdef FM1_OPEN_WEB
SONG_SMALL void song_port_render(uint64_t frame)
{
    if (ready) song_deliver_render((uint32_t)(frame / FM1_BLOCK_FRAMES));
}

#include "song_probe.inc" /* headless test hooks */
#else
void song_port_render(uint64_t frame) { (void)frame; }
#endif
