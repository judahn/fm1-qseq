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
#if SONG_UI
#include "version.h"
uint32_t song_store_probe(unsigned what); /* song_store_list.inc */
uint32_t song_panel_probe(unsigned what); /* song_step.inc */
int sim_song_kit_copy_run(void);
#endif
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
    case 13: { /* a marker's clock, or with track 1 its first 4 name bytes */
        if (index >= song.markers) return 0;
        const uint8_t *n = (const uint8_t *)song.marker[index].name;
        return track ? n[0] | (uint32_t)n[1] << 8 | (uint32_t)n[2] << 16 | (uint32_t)n[3] << 24 : song.marker[index].clock;
    }
    case 14: return song.loop_start;
    case 15: return song.loop_end;
    case 16: return song.loop_on | (uint32_t)player.jumping << 1;
#if SONG_UI
    case 17: return step.pointer;
    case 28: return (uint32_t)song_port_live_clock(); /* a key's clock now */
    case 29: { /* recording, undo and clipboard state, by index */
        const struct song_recorder *r = song_port_recorder();
        const uint32_t v[] = {r->active, r->taken, r->passes, song.rec_mode, song.count_in,
                              song.quantize, song.strength, song.metronome, player.click, song_undo_count(&undo, 0),
                              song_undo_count(&undo, 1), clip.length, clip.tracks, song.bars, player.mute_track, r->track};
        return index < sizeof v / sizeof v[0] ? v[index] : 0;
    }
    case 30: return index < count ? song_event_kind(&e[index]) : 0;
    case 31: return song_knob_lane(track, index) | song_knob_value(track, index) << 8 | song_knob_held(index) << 16;
    case 32: return track ? song.meters : index < song.meters ? song.meter[index].bar |
                    (uint32_t)song.meter[index].beats << 16 | (uint32_t)song.meter[index].unit << 24 : 0;
    case 33: return player.touch_track | (uint32_t)player.touch_lane[index & 3U] << 8;
    case 34: return song_port_engine(track);
    case 35: return index < count ? song_glitch_word(&song, &e[index]) : 0;
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
    case 36: return song_grid_rec_state() | song_grid_rec_cell() << 8; /* a cell's take (0.3.0) */
    case 47: return song_perf_state() | (uint32_t)song_port_clip_armed() << 8 | (uint32_t)song_port_clip_live() << 16; /* phase 7b */
    case 49: return song.chord[track & 7U] | (uint32_t)song.chord_type[track & 7U] << 8; /* phase 8 */
    case 50: return song_port_blocks();
    case 52: { char n[17]; uint32_t v; song_store_name(song_store_id(index), n); memcpy(&v, n, 4); return v; } /* a stored name */
    case 53: { static const char q[16] = QSEQ_VERSION; uint32_t v; memcpy(&v, q + (index & 3U) * 4U, 4); return v; } /* the release */
    case 54: return groove_tempo_saved() | groove_tempo_current() << 16; /* the engine's tempo (0.3.0) */
    case 55: return song_port_battery_shown();
    case 56: return groove_arp_enabled(track & 7U);
    case 57: { uint32_t v; memcpy(&v, song.name, 4); return v; } /* the working song's name (0.3.1) */
    case 58: return song_panel_probe(index); /* 0.5.0: the roll's zoom, the glitch page, what shows, the sweep, and the song view's zoom (0.5.2) */
    case 59: return track ? player.start_song : player.start_at; /* the start point (0.5.0) */
    case 60: { /* a pad's tune as the engine has it, and the track's edited mark (0.6.6) */
        uint8_t v[GROOVE_TONE_PARAMS];
        struct groove_view g = {0};
        groove_get_view(track & 7U, 0, &g);
        return groove_tone_copy(track & 7U, index, v) < 0 ? 0xffffU : v[SAMPLE_DRUM_TUNE] | (uint32_t)g.modified << 8;
    }
    case 63: { /* a pad's six settings, the first four bytes (index 0 to 7) or the last two (8 to 15) (0.6.6) */
        uint8_t v[GROOVE_TONE_PARAMS] = {0};
        if (groove_tone_copy(track & 7U, index & 7U, v) < 0) return 0xffffffffU;
        return index < 8 ? v[0] | (uint32_t)v[1] << 8 | (uint32_t)v[2] << 16 | (uint32_t)v[3] << 24 : v[4] | (uint32_t)v[5] << 8;
    }
    case 61: return song.chromatic[track & 7U]; /* chromatic mode (0.6.6) */
    case 62: return step.glitch; /* step entry's entry word */
#endif
    case 18: return index < count ? e[index].b : 0;
    case 19: return song.swing | (uint32_t)song.swing_grid << 8 | (uint32_t)song.swing_tracks << 16;
    case 20: return song.key_root | (uint32_t)song.key_scale << 8;
    case 21: return song.tempo[0].bpm100 | (uint32_t)song.meter[0].beats << 16 | (uint32_t)song.meter[0].unit << 24;
#if SONG_HW_TEST /* the diagnostics page (build.sh --test-preview) */
    case 22: return song_port_method();
    case 23: return index < SONG_DIAG_LATE ? song_diag.late[index] : song_diag.notes;
    case 24: return song_diag.polls;
    case 25: return song_diag.largest_free;
    case 26: return (uint32_t)(int32_t)song_diag.heaviest;
    case 27: return index < 6 ? song_diag.engine_free[index] : 0;
#endif
    case 51: return song_isr_mode; /* the wake (method B) */
    default: return 0;
    }
}
#if SONG_UI
SONG_EXPORT("sim_song_kit_copy") int sim_song_kit_copy(void) { return sim_song_kit_copy_run(); }
/* A tempo change set as an import or an old song can bring it, hundredths
 * and all, which the tempo knobs snap to their grid (0.5.2). */
SONG_EXPORT("sim_song_tempo") int sim_song_tempo(unsigned clock, unsigned bpm100) { return song_port_set_tempo(clock, bpm100); }
/* For the TUNE checks (0.6.6): a pad's tune turned as on the sound pages,
 * and a note's glitch word as a song file can bring it, on any track. */
SONG_EXPORT("sim_song_tone") void sim_song_tone(unsigned track, unsigned pad, int delta) { groove_tone_edit(track, pad, SAMPLE_DRUM_TUNE, delta); }
SONG_EXPORT("sim_song_glitch") int sim_song_glitch(unsigned track, unsigned index, uint32_t word)
{
    return song_glitch_apply(&song, track, index, word);
}
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
#endif
