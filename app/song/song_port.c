/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Song glue: owns the song and the player, and hands notes to the engine as
 * MIDI on each track's input channel, through the same queue as the keys. */
#include "song_port.h"
#include "instrument.h"

_Static_assert(SONG_FRAME_RATE == FM1_SAMPLE_RATE, "song frame rate");
_Static_assert(SONG_TRACKS == GROOVE_TRACKS, "song tracks");

static struct song song;
static struct song_player player;
static struct song_step step;
static uint64_t next_frame;           /* first frame of the next block to render */
static uint8_t ready;
#ifdef FM1_OPEN_WEB
static void capture(unsigned track, unsigned pitch, unsigned velocity);
static uint64_t start_frame;
static uint32_t start_clock;
#endif

static void deliver(void *context, unsigned track, unsigned pitch, unsigned velocity)
{
    (void)context;
    unsigned channel = groove_midi_input_channel(track);
    /* Play and Off tracks ignore arrangement playback, as with the stock
     * sequencer. Note-offs always go out. */
    if (channel > 15 || (velocity && groove_track_role(track) != GROOVE_TRACK_NORMAL)) return;
    const uint8_t message[3] = {(uint8_t)((velocity ? 0x90U : 0x80U) | channel), (uint8_t)pitch,
                                (uint8_t)velocity};
    (void)fm1_midi_post(message, sizeof message);
#ifdef FM1_OPEN_WEB
    capture(track, pitch, velocity);
#endif
}
static void ensure(void)
{
    if (ready) return;
    static const struct song_output output = {deliver, NULL};
    song_build_test(&song);
    song_player_init(&player, &song, &output);
    step_init(&step, &song, &output);
    ready = 1;
}
struct song *song_port_song(void) { ensure(); return &song; }
struct song_player *song_port_player(void) { ensure(); return &player; }
struct song_step *song_port_step(void) { ensure(); return &step; }

/* DEVICE STUB (song-mode). The engine has no timestamped input: whatever is
 * posted plays at the start of the next block it renders. The plan is to
 * read the render counter on each control poll and post what is due up to
 * the end of the next block, one block ahead of the audio. That waits for
 * the hardware timing test, so device playback stays off until then. */
#ifndef SONG_DEVICE_DELIVERY
#define SONG_DEVICE_DELIVERY 0
#endif
void song_port_poll(void)
{
#ifndef FM1_OPEN_WEB
    next_frame = (uint64_t)fm1_audio_render_count() * FM1_BLOCK_FRAMES;
#if SONG_DEVICE_DELIVERY
    if (ready && player.playing) song_player_advance(&player, next_frame + FM1_BLOCK_FRAMES);
#endif
#endif
}

int song_port_start(void)
{
    ensure();
#if defined(FM1_OPEN_WEB) || SONG_DEVICE_DELIVERY
#ifdef FM1_OPEN_WEB
    start_frame = next_frame;
    start_clock = player.playhead;
#endif
    song_player_start(&player, next_frame);
    return 0;
#else
    return -1;
#endif
}
void song_port_stop(void)
{
    if (ready) song_player_stop(&player);
}

#ifdef FM1_OPEN_WEB
static uint32_t render_block;
void song_port_render(uint64_t frame)
{
    next_frame = frame + FM1_BLOCK_FRAMES;
    render_block = (uint32_t)(frame / FM1_BLOCK_FRAMES);
    if (ready && player.playing) song_player_advance(&player, next_frame);
}

/* Headless test hooks: every note the player sends, with its block, and the
 * song itself, so a page script can check the timing on its own. */
#ifdef __wasm__
#define SONG_EXPORT(name) __attribute__((export_name(name)))
#else
#define SONG_EXPORT(name)
#endif
struct song_heard { uint32_t block; uint8_t track, pitch, velocity, unused; };
enum { HEARD = 8192 };
static struct song_heard heard[HEARD];
static unsigned heard_count, heard_on;
static void capture(unsigned track, unsigned pitch, unsigned velocity)
{
    if (heard_on && heard_count < HEARD)
        heard[heard_count++] = (struct song_heard){render_block, (uint8_t)track, (uint8_t)pitch, (uint8_t)velocity, 0};
}
SONG_EXPORT("sim_song_capture") void sim_song_capture(unsigned enable) { heard_on = enable != 0; heard_count = 0; }
SONG_EXPORT("sim_song_heard") const void *sim_song_heard(void) { return heard; }
SONG_EXPORT("sim_song_heard_count") unsigned sim_song_heard_count(void) { return heard_count; }
SONG_EXPORT("sim_song_info") uint32_t sim_song_info(unsigned what, unsigned track, unsigned index)
{
    ensure();
    unsigned count;
    const struct song_event *e = song_track(&song, track, &count);
    switch (what) {
    case 0: return count;
    case 1: return index < count ? song_event_start(&e[index]) : 0;
    case 2: return index < count ? e[index].a : 0;
    case 3: return index < count ? e[index].length : 0;
    case 4: return song.tempos;
    case 5: return index < song.tempos ? song.tempo[index].clock : 0;
    case 6: return index < song.tempos ? song.tempo[index].bpm100 : 0;
    case 7: return player.playing;
    case 8: return song_player_clock(&player);
    case 9: return (uint32_t)(start_frame / FM1_BLOCK_FRAMES);
    case 10: return start_clock;
    case 11: return song_end(&song);
    case 12: return song.markers;
    case 13: return index < song.markers ? song.marker[index].clock : 0;
    case 14: return song.loop_start;
    case 15: return song.loop_end;
    case 16: return song.loop_on | (uint32_t)player.jumping << 1;
    case 17: return step.pointer;
    case 18: return index < count ? e[index].b : 0;
    case 19: return song.swing | (uint32_t)song.swing_grid << 8 | (uint32_t)song.swing_tracks << 16;
    case 20: return song.key_root | (uint32_t)song.key_scale << 8;
    case 21: return song.tempo[0].bpm100 | (uint32_t)song.meter[0].beats << 16 | (uint32_t)song.meter[0].unit << 24;
    default: return 0;
    }
}
#else
void song_port_render(uint64_t frame) { (void)frame; }
#endif
