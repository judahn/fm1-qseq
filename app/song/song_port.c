/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Song glue: owns the song, the player and step entry, and hands notes to
 * the engine as MIDI on each track's input channel (song_deliver.c says how). */
#include "song_port.h"
#include "diag.h"
#include "instrument.h"
#if SONG_UI
#include "undo.h"
#include "arrange.h"
#include "record.h"
#include "lanes.h"
#include "glitch.h"
static struct song_undo undo;
static struct song_clip clip;
static struct song_recorder recorder;
static uint8_t loop_before;
static uint64_t first_frame; /* the take's first beat, after any count-in */
#endif

_Static_assert(SONG_FRAME_RATE == FM1_SAMPLE_RATE, "song frame rate");
_Static_assert(SONG_TRACKS == GROOVE_TRACKS, "song tracks");

static struct song song;
static struct song_player player;
#if SONG_UI
static struct song_step step;
#endif
static uint8_t ready;
#if SONG_UI
/* The clock for a key played now, or -1 in the count-in. A key up to 50 ms
 * early still lands on the first beat. */
SONG_SMALL static int64_t live_clock(void)
{
    uint64_t frame = (uint64_t)song_port_blocks() * FM1_BLOCK_FRAMES;
    if (frame >= first_frame) return song_player_clock_at(&player, frame);
    return frame + FM1_SAMPLE_RATE / 20U >= first_frame ? (int64_t)recorder.from : -1;
}
#endif
#ifdef FM1_OPEN_WEB
static void capture(unsigned track, unsigned pitch, unsigned velocity, unsigned kind);
static uint32_t start_block, start_clock;
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
int64_t song_port_live_clock(void) { return live_clock(); }
/* Each drum track's pads, for glitch's pad swap, as playing starts. */
SONG_SMALL static void pads(void)
{
    for (unsigned t = 0; t < SONG_TRACKS; ++t) {
        unsigned n = groove_pad_count(t);
        player.pad_count[t] = (uint8_t)(n < 16 ? n : 16);
        for (unsigned i = 0; i < player.pad_count[t]; ++i) player.pads[t][i] = (uint8_t)groove_pad_note(t, i);
    }
}
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
    song_diag_reset();
    song_diag.heaviest = -1;
#if SONG_UI
    song_undo_init(&undo);
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

/* Each control poll: timing for the diagnostics page, then methods A and B. */
SONG_SMALL void song_port_poll(void)
{
    ensure();
    song_diag_poll(song_port_ticks(), fm1_millis(), song_port_blocks());
    song_deliver_poll();
#if SONG_UI
    if (recorder.active && !player.playing) song_port_record_stop(); /* the song ended */
    else if (recorder.active && live_clock() >= 0) {
        uint32_t clock = (uint32_t)live_clock();
        if (recorder.mode == SONG_REC_LOOP && clock < recorder.last) song_knobs_wrap(song.loop_end);
        song_rec_follow(&recorder, clock);
    }
    song_knobs_poll(fm1_millis());
#endif
}

/* Device playback is on in the hardware test build (song_config.h). */
SONG_SMALL int song_port_start(void)
{
    ensure();
#if defined(FM1_OPEN_WEB) || SONG_HW_TEST
#ifdef FM1_OPEN_WEB
    start_block = song_port_blocks();
    start_clock = player.playhead;
#endif
    song_diag_reset();
#if SONG_UI
    player.click = song.metronome == 1;
    pads();
#endif
    song_deliver_start(0);
    return 0;
#else
    return -1;
#endif
}

#if SONG_UI
struct song_undo *song_port_undo(void) { ensure(); return &undo; }
struct song_clip *song_port_clip(void) { return &clip; }
struct song_recorder *song_port_recorder(void) { ensure(); return &recorder; }

/* Live keys on the take track, at the clock playing now. */
SONG_SMALL void song_port_key(const uint8_t *m, size_t bytes)
{
    int64_t clock = live_clock();
    if (!recorder.active || bytes != 3 || (m[0] & 0xe0U) != 0x80U || clock < 0 ||
        (m[0] & 15U) != groove_midi_input_channel(recorder.track)) return;
    song_rec_note(&recorder, m[1], (m[0] & 0xf0U) == 0x90U ? m[2] : 0, (uint32_t)clock);
}
/* A take is one undo step. Loop recording starts at the loop and repeats it. */
SONG_SMALL int song_port_record(unsigned mode, unsigned track)
{
    ensure();
    if (player.playing || mode == SONG_REC_STEP || mode >= SONG_REC_MODES) return -1;
    song_undo_begin(&undo, &song);
    loop_before = song.loop_on;
    if (mode == SONG_REC_LOOP) { song.loop_on = 1; player.playhead = song.loop_start; }
    song_rec_start(&recorder, &song, track, mode, player.playhead);
    uint32_t from, to;
    if (song_rec_quiet(&recorder, &from, &to)) { player.mute_track = (uint8_t)track; player.mute_from = from; player.mute_to = to; }
    player.click = song.metronome != 0;
#ifdef FM1_OPEN_WEB
    start_block = song_port_blocks();
    start_clock = player.playhead;
#endif
    song_diag_reset();
    uint64_t now = (uint64_t)song_port_blocks() * FM1_BLOCK_FRAMES;
    pads();
    song_deliver_start(song.count_in);
    first_frame = song.count_in ? player.frame : now; /* in the count-in the player waits at the first beat */
    return 0;
}
SONG_SMALL void song_port_record_stop(void)
{
    if (!recorder.active) return;
    song_stutter_release(); /* a stutter still held goes on the FX lane */
    int64_t now = live_clock();
    uint32_t clock = player.playing && now >= 0 ? (uint32_t)now : recorder.last;
    song_knobs_release(clock); /* before the chase on stop */
    song_port_stop();
    song_rec_stop(&recorder, clock);
    player.mute_track = 0xff;
    player.click = song.metronome == 1;
    if (recorder.mode == SONG_REC_LOOP) song.loop_on = loop_before;
    song_undo_end(&undo, &song);
}
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
