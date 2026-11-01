/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#ifndef FM1_SONG_PORT_H
#define FM1_SONG_PORT_H
#include "step.h"
#include "song_config.h"
#include <stddef.h>

/* The song and player the panel works with, and their link to the engine.
 * Notes go to the engine as MIDI on each track's input channel. Control
 * task only, the player included. */
struct song *song_port_song(void);
struct song_player *song_port_player(void);
struct song_step *song_port_step(void); /* step entry, previewing through the engine */
int song_port_start(void);  /* 0 */
void song_port_stop(void);
void song_port_jump(uint32_t clock); /* on purpose: it becomes the start point (0.5.0, player.h) */
int song_port_set_tempo(uint32_t clock, unsigned bpm100);
/* Browser preview: called from sim_render() before the queued MIDI drains,
 * with the first frame of the block about to render. */
void song_port_render(uint64_t frame);
/* Control poll: the delivery, and the diagnostics' timing in the test build. */
void song_port_poll(void);
/* app/board/audio.c: blocks rendered so far (device only). */
uint32_t fm1_audio_render_count(void);

/* Note delivery (song_deliver.c): method B, and in the hardware test build
 * a choice of A or B on the diagnostics page. */
enum { SONG_DELIVER_A, SONG_DELIVER_B };
#if SONG_HW_TEST
unsigned song_port_method(void);
int song_port_set_method(unsigned method);           /* while stopped */
uint32_t song_port_ticks(void);                      /* 24 MHz */
#endif
uint32_t song_port_blocks(void);                     /* blocks rendered so far */
int song_midi_post(const uint8_t *message, size_t bytes); /* live keys */
void song_deliver_note(const uint8_t message[3]);
void song_deliver_played(const uint8_t message[3]); /* the player's: never a live key (0.5.1) */
void song_deliver_poll(void);                        /* through the block about to render */
void song_deliver_start(unsigned count_in);
void song_deliver_render(uint32_t block);            /* browser preview only */
int song_deliver_beat_lit(void);                     /* PLAY's light: stopped lit, playing with the beat (0.5.1) */
/* The audio interrupt reads song_isr_mode after each render: 1 wakes the
 * control loop (method B). */
extern volatile uint8_t song_isr_mode;
#if SONG_UI
/* Editing: the undo journal, the clipboard and real-time recording
 * (song_takes.c). A take ends with PLAY or REC and playing goes on, but in
 * its count-in it is called off and playing stops. */
struct song_undo *song_port_undo(void);
struct song_clip *song_port_clip(void);
struct song_recorder *song_port_recorder(void);
int song_port_record(unsigned mode, unsigned track); /* from a stop, with count-in and metronome */
void song_port_record_stop(void);                    /* ends a take and stops */
void song_port_key(const uint8_t *message, size_t bytes); /* a live key on its way to the engine */
int64_t song_port_live_clock(void);                   /* where a key or knob lands now, -1 in a count-in */
unsigned song_port_recording(void);  /* a take into the song or a clip, running or armed */
unsigned song_port_take_full(void);  /* 1 once after a take ended because the song filled up */
void song_takes_poll(void);
void song_deliver_message(const uint8_t *message, size_t bytes); /* automation, as notes go */
/* Clips (design 8f, song_port.c): launches, and a clip opened in the editor
 * and closed again, playing on in step with the bar (player.h). */
void song_port_launch(unsigned track, unsigned to);
int song_port_clip_open(unsigned cell);
void song_port_clip_close(void);
void song_port_clip_launched(void); /* playing: the open clip plays on, launched, once it closes */
/* REC while playing in an open clip: loop recording into it from its next
 * pass (or at once with `now`), the rest playing on. Armed until then.
 * Ending it with keep_playing leaves playback going. */
int song_port_record_clip(unsigned now);
unsigned song_port_clip_armed(void);
unsigned song_port_clip_live(void);   /* taking into the open clip while playing */
void song_port_clip_disarm(void);
void song_port_record_end(unsigned keep_playing);
void song_port_record_end_at(uint32_t clock); /* an open clip's take, ending at its end, playing on */
/* Recording into a cell from the grid (song_grid_take.c, 0.3.0): REC on a
 * cell, `bars` long when empty (0 for FREE). A clip already there is
 * overdubbed, or emptied first when the song's mode is REPLACE (0.4.0).
 * REC again ends it, or sets a FREE take's end at the next bar line. The
 * take ending calls ended(), with whether it began and its clip clock.
 * State: 0 none, 1 armed, 2 recording, plus 4 while FREE has no end and 8
 * for a new clip. */
int song_grid_rec(unsigned cell, unsigned bars);
void song_grid_rec_key(void);
unsigned song_grid_rec_state(void);
unsigned song_grid_rec_cell(void);
void song_grid_take_poll(void);
void song_grid_take_ended(unsigned began, uint32_t clock);
/* The engine's tempo follows the song's where the playhead is, whole BPM
 * (song_tempo.c, 0.3.0): follow() each poll, and with force after the song
 * changed, and adopt() takes a change made on a stock page or by tap tempo
 * into the song, at the tempo in force there. */
void song_tempo_follow(unsigned force);
void song_tempo_adopt(void);
unsigned song_tempo_now(void); /* the change in force where the song is, a clip open or not (0.5.2) */
/* The battery's ADC reading (fm1_controls), each poll, and the smoothed
 * value shown, at most once a second (0.3.0). Full is a full battery read on
 * a device, unplugged (0.3.2). Empty is an estimate, 0.81 of full, which is
 * 3.4 V if full is 4.2 V: lower it if a flat battery reads less. */
enum { SONG_BATTERY_EMPTY = 505, SONG_BATTERY_FULL = 622 };
void song_port_battery(unsigned raw, uint32_t now);
unsigned song_port_battery_shown(void);
/* Recording a performance in the grid (song_perf.c): REC on the SONG row
 * arms it, and the take starts with the next launch or PLAY. Each launch
 * writes its track a block from its launch point to the track's next
 * launch, stop or return to the song, and what each track played as the
 * take began is written from there (0.4.0), a silent track as a stop
 * block. Stopping playback, or REC again, ends the take, one undo step.
 * State: 0 off, 1 armed, 2 taking. */
int song_perf_arm(void);
void song_perf_end(void);
void song_perf_poll(void);
unsigned song_perf_state(void);
/* Chord mode on the keys (song_chord.c, design 8g). The panel hands over
 * the keys held each poll: 1 when chord mode took them (on, on a pitched
 * track). In step entry each root's chord becomes the entry. The delivery
 * sends the strums' notes due before block `block` ends. A chord's notes go
 * into a take with their offsets, and a note let go before it sounded is
 * dropped from it. */
int song_chord_keys(unsigned track, uint32_t keys, int octave, unsigned mode, struct song_step *entry);
void song_chord_service(uint32_t block);
unsigned song_chord_waiting(void); /* strum notes still to go */
void song_chord_forget(void); /* a song loaded or new */
void song_port_chord_take(unsigned track, const uint8_t *pitch, const uint8_t *offset, unsigned count);
void song_port_chord_drop(unsigned track, unsigned pitch);
/* Per-note pitch on sampler tracks (song_tune.c, 0.6.6). A note about to
 * start sets its pad's tune to the user's setting plus its semitones, when
 * the pad has something else. Stopped, the pads go back to the user's
 * settings two blocks after the last sampler note (settle, each poll, 1
 * while it waits), and at once before the sounds are captured or replaced
 * (restore, and forget, which also drops what it knew of the edited
 * marks). clean: the tracks whose edited mark only our changes set, while
 * nothing else has been edited. A live key tunes its pad too, and a take
 * gets its semitones (taken, once). */
void song_tune_note(unsigned track, unsigned pitch, int semitones);
void song_tune_restore(void);
unsigned song_tune_settle(void);
void song_tune_forget(void);
unsigned song_tune_clean(void);
void song_tune_live(unsigned track, unsigned pitch, int semitones);
int song_tune_taken(void);
/* K1 to K4 while recording (song_knobs.c): each drives a lane on the take
 * track, chosen per track (lanes.h), recorded in touch style. */
unsigned song_port_engine(unsigned track);
unsigned song_knob_lane(unsigned track, unsigned knob);
unsigned song_knob_value(unsigned track, unsigned knob); /* what it is at or setting now */
unsigned song_knob_held(unsigned knob);
void song_knob_turn(unsigned knob, int delta); /* it latches until the take ends (0.6.2) */
void song_knobs_release(uint32_t clock); /* lets go of every knob: the take ends */
void song_knobs_wrap(uint32_t loop_end); /* a loop pass ends, latched knobs go on from its start */
/* Storage (song_store.c, phase 5a): songs, kits and the recovery copy in
 * the flash pool. Writes happen only while stopped. 0, or an error. */
enum { SONG_STORE_FULL = -20, SONG_STORE_PLAYING = -21, SONG_STORE_BUSY = -22 };
void song_store_init(void);
void song_store_poll(uint32_t now);          /* a recovery copy after playback stops */
int song_store_save(unsigned as_new);        /* the working song, to its slot or a new one */
int song_store_load(unsigned id);
int song_store_new(int factory);             /* blank (-1) or a factory sound set */
int song_store_rename(unsigned id, const char name[12]);
int song_store_duplicate(unsigned id);
int song_store_delete(unsigned id);
unsigned song_store_count(void);
unsigned song_store_id(unsigned index);
void song_store_name(unsigned id, char out[17]);
uint32_t song_store_bytes(unsigned id);
unsigned song_store_kits(void);
unsigned song_store_kit_id(unsigned index);
unsigned song_store_kit(void);               /* the one bound to the pads */
void song_store_kit_name(unsigned id, char out[17]);
int song_store_use_kit(unsigned id);         /* rebinds the pads, nothing written */
int song_store_delete_kit(unsigned id);
unsigned song_store_recovery(void);          /* 1 while a recovery copy waits at boot */
int song_store_restore(void);
int song_store_drop_recovery(void);          /* DELETE on the list: the copy goes from flash */
unsigned song_store_unsaved(void);           /* 1 when the song changed since its last save or load */
struct song_gauge { uint32_t pool, songs, samples, free; unsigned events; };
void song_store_gauge(struct song_gauge *g);
void song_port_ready_pads(void); /* each drum track's pads, as a start would take them (phase 6) */
/* Transfers to and from a computer (song_store_io.inc, phase 5b). */
struct song_pool;
struct song_pool *song_store_pool(void);
unsigned song_store_busy(void);              /* 1 while a kit is coming in */
/* A song payload from a computer, checked whole, then stored as `id`, or
 * as a new song for 0. The stored copy is the payload, byte for byte. */
int song_store_put(unsigned id, const uint8_t *data, uint32_t bytes, unsigned *made);
/* Ends a kit sent through the sample calls as a library kit, named, with
 * pad settings if given. The pads keep the kit they have. */
int song_store_kit_finish(const char name[16], const uint8_t *params, unsigned *made);
int song_store_kit_set(unsigned id, const char name[16], const uint8_t *params); /* NULL: no pad settings */
void song_store_clear(void);                 /* every song and the recovery copy */
#endif
#endif
