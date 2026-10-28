/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* song-mode: headless test hooks for the browser preview, included by
 * song_port.c. Every note the player sends, with its block, and the song
 * itself, so a page script can check the timing on its own. */
#ifdef __wasm__
#define SONG_EXPORT(name) __attribute__((export_name(name)))
#else
#define SONG_EXPORT(name)
#endif
/* kind: 0 for a note, else the status of a controller (pitch is its number,
 * velocity its value) or program change (pitch is the program). */
uint32_t song_store_probe(unsigned what); /* song_store_list.inc */
int sim_song_kit_copy_run(void);
struct song_heard { uint32_t block; uint8_t track, pitch, velocity, kind; };
enum { HEARD = 8192 };
static struct song_heard heard[HEARD];
static unsigned heard_count, heard_on;
SONG_SMALL static void capture(unsigned track, unsigned pitch, unsigned velocity, unsigned kind)
{
    if (heard_on && heard_count < HEARD)
        heard[heard_count++] = (struct song_heard){song_port_blocks(), (uint8_t)track, (uint8_t)pitch, (uint8_t)velocity,
                                                   (uint8_t)kind};
}
SONG_EXPORT("sim_song_capture") void sim_song_capture(unsigned enable) { heard_on = enable != 0; heard_count = 0; }
void song_probe_heard(unsigned track, unsigned pitch, unsigned velocity) { capture(track, pitch, velocity, 0); } /* chord mode's notes */
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
    case 9: return start_block;
    case 10: return start_clock;
    case 11: return song_end(&song);
    case 12: return song.markers;
    case 13: return index < song.markers ? song.marker[index].clock : 0;
    case 14: return song.loop_start;
    case 15: return song.loop_end;
    case 16: return song.loop_on | (uint32_t)player.jumping << 1;
#if SONG_UI
    case 17: return step.pointer;
    case 28: return (uint32_t)live_clock(); /* a key's clock now */
    case 29: { /* recording, undo and clipboard state, by index */
        const uint32_t v[] = {recorder.active, recorder.taken, recorder.passes, song.rec_mode, song.count_in,
                              song.quantize, song.strength, song.metronome, player.click, song_undo_count(&undo, 0),
                              song_undo_count(&undo, 1), clip.length, clip.tracks, song.bars, player.mute_track};
        return index < sizeof v / sizeof v[0] ? v[index] : 0;
    }
    case 30: return index < count ? song_event_kind(&e[index]) : 0;
    case 31: return song_knob_lane(track, index) | song_knob_value(track, index) << 8 | song_knob_held(index) << 16;
    case 32: return track ? song.meters : index < song.meters ? song.meter[index].bar |
                    (uint32_t)song.meter[index].beats << 16 | (uint32_t)song.meter[index].unit << 24 : 0;
    case 33: return player.touch_track | (uint32_t)player.touch_lane[index & 3U] << 8;
    case 34: return song_port_engine(track);
    case 35: return index < count ? song_glitch_word(&song, &e[index]) : 0;
    case 36: return index ? player.stut_to : player.stut_at;
    case 37: return song.humanize_time[track & 7U] | (uint32_t)song.humanize_velocity[track & 7U] << 8;
    case 38: return song_store_probe(index);
    case 39: return index < SONG_CELLS ? song.clip_length[index] : 0; /* clips (phase 7), by cell */
    case 40: return index < SONG_CELLS ? song.count[song_owner(&song, SONG_TRACKS + index)] : 0;
    case 41: return player.from[track & 7U] | (uint32_t)player.waiting[track & 7U] << 8;
    case 42: return song.open | (uint32_t)song.launch_q << 8;
    case 43: return song_player_transport(&player);
    case 44: return player.base[track & 7U];
    case 45: return player.launch_at[track & 7U];
    case 46: { uint32_t v; memcpy(&v, song.row_name[index & 15U], 4); return v; }
    case 47: return song_perf_state() | (uint32_t)song_port_clip_armed() << 8 | (uint32_t)clip_live << 16; /* phase 7b */
    case 49: return song.chord[track & 7U] | (uint32_t)song.chord_type[track & 7U] << 8; /* phase 8 */
    case 50: return song_port_blocks();
#endif
    case 18: return index < count ? e[index].b : 0;
    case 19: return song.swing | (uint32_t)song.swing_grid << 8 | (uint32_t)song.swing_tracks << 16;
    case 20: return song.key_root | (uint32_t)song.key_scale << 8;
    case 21: return song.tempo[0].bpm100 | (uint32_t)song.meter[0].beats << 16 | (uint32_t)song.meter[0].unit << 24;
    case 22: return song_port_method();
    case 23: return index < SONG_DIAG_LATE ? song_diag.late[index] : song_diag.notes;
    case 24: return song_diag.polls;
    case 25: return song_diag.largest_free;
    case 26: return (uint32_t)(int32_t)song_diag.heaviest;
    case 27: return index < 6 ? song_diag.engine_free[index] : 0;
    default: return 0;
    }
}
SONG_EXPORT("sim_song_kit_copy") int sim_song_kit_copy(void) { return sim_song_kit_copy_run(); }
/* Phase 5b: the page's stock sample loader sends its bank through the
 * transfer calls, so it lands in the pool as a kit, and templates are
 * songs made from the factory sound sets, saved so they can be read out. */
#include <stdlib.h>
static uint8_t *sim_buffer;
SONG_EXPORT("sim_song_buffer") uint8_t *sim_song_buffer(unsigned bytes)
{
    free(sim_buffer);
    sim_buffer = bytes && bytes <= 1048576U ? malloc(bytes) : NULL;
    return sim_buffer;
}
SONG_EXPORT("sim_song_bank") int sim_song_bank(unsigned bytes)
{
    int r = sim_buffer ? groove_samples_begin(bytes) : -1;
    for (unsigned at = 0; !r && at < bytes; at += 128)
        r = groove_samples_write(at, sim_buffer + at, bytes - at < 128 ? bytes - at : 128);
    return r ? r : groove_samples_finish();
}
SONG_EXPORT("sim_song_template") int sim_song_template(unsigned factory)
{
    int r = song_store_new((int)factory);
    if (!r) r = song_store_save(1);
    return r ? r : (int)song.id;
}
