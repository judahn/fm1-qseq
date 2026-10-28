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
