/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* song-mode: live recording, the port's side (design sections 5 and 8f).
 * The recorder (record.h) on the take track, the clock a key lands on, the
 * song view's takes from a stop with their count-in, an open clip's take
 * from its next pass, and ending them. A take is one undo step. PLAY or
 * REC ends a take and playing goes on (0.3.0), but in the count-in, before
 * anything is recorded, PLAY or REC calls the take off and playing stops.
 * A take that fills the song's event memory ends there. The grid's takes
 * into a cell are in song_grid_take.c. See song_port.h. */
#include "song_port.h"
#include "instrument.h"
#include "player.h"
#include "record.h"
#include "undo.h"
#if SONG_UI

static struct song_recorder recorder;
static uint8_t loop_before, clip_armed, clip_live, full; /* REC in an open clip: waiting for its next pass, taking while playing */
static uint32_t clip_last;
static uint64_t first_frame; /* the take's first beat, after any count-in */
/* Keys while a cell's or an open clip's take waits for its first beat, or
 * in a count-in (0.5.1). Those still within the catch as it starts go on
 * that beat, and one let go before it keeps the length it was held. */
enum { EARLY = 8 };
struct early_key { uint64_t on, off; uint8_t pitch, velocity, lead, channel; };
static struct early_key early[EARLY];
static uint8_t early_count;
enum { ROOM = SONG_REC_HELD + 8 }; /* events kept for the keys still down and the knobs letting go */
#ifdef FM1_OPEN_WEB
void song_port_mark_start(void); /* song_port.c, for the headless check */
#endif

struct song_recorder *song_port_recorder(void) { return &recorder; }
static uint64_t frame_now(void) { return (uint64_t)song_port_blocks() * FM1_BLOCK_FRAMES; }
/* The catch (record.h) in frames, and a key's frames held in clocks, at
 * the tempo where the take starts. */
SONG_SMALL static uint64_t catch_frames(uint32_t clocks)
{
    const struct song *s = song_port_song();
    return clocks * SONG_UNITS_PER_CLOCK / song_rate(s->tempo[song_tempo_at(s, recorder.from)].bpm100);
}
SONG_SMALL static uint32_t held_clocks(uint64_t frames_held)
{
    const struct song *s = song_port_song();
    return (uint32_t)(frames_held * song_rate(s->tempo[song_tempo_at(s, recorder.from)].bpm100) / SONG_UNITS_PER_CLOCK);
}
/* The clock for a key played now, or -1 in the count-in. A key within the
 * catch, up to 100 ms early (0.5.1), still lands on the first beat. */
SONG_SMALL int64_t song_port_live_clock(void)
{
    uint64_t frame = frame_now();
    if (frame >= first_frame) return song_player_clock_at(song_port_player(), frame);
    return frame + catch_frames(song_rec_window(song_port_song(), recorder.from)) >= first_frame ? (int64_t)recorder.from : -1;
}
/* A key while a take waits: kept, the ones too old to be caught let go.
 * A key going up returns 1 plus the frames it was held, or 0 if it wasn't
 * kept. */
SONG_SMALL static uint64_t keep_early(unsigned pitch, unsigned velocity, unsigned lead, unsigned channel)
{
    uint64_t now = frame_now(), window = catch_frames(song_rec_window(song_port_song(), 0));
    unsigned kept = 0;
    for (unsigned i = 0; i < early_count; ++i)
        if (early[i].on + window >= now) early[kept++] = early[i];
    early_count = (uint8_t)kept;
    if (!velocity) {
        for (unsigned i = 0; i < early_count; ++i)
            if (early[i].pitch == pitch && early[i].channel == channel && !early[i].off) {
                early[i].off = now;
                return 1U + now - early[i].on;
            }
    } else if (early_count < EARLY)
        early[early_count++] = (struct early_key){now, 0, (uint8_t)pitch, (uint8_t)velocity, (uint8_t)lead, (uint8_t)channel};
    return 0;
}
/* The take has begun: keys from just before go on its first beat. */
SONG_SMALL static void early_take(void)
{
    uint64_t now = frame_now(), window = catch_frames(song_rec_window(song_port_song(), recorder.from));
    for (unsigned i = 0; i < early_count; ++i)
        if (early[i].on + window >= now && early[i].channel == groove_midi_input_channel(recorder.track))
            song_rec_early(&recorder, early[i].pitch, early[i].velocity, early[i].lead,
                           early[i].off ? held_clocks(early[i].off - early[i].on) + 1U : 0);
    early_count = 0;
}
static unsigned take_waits(void) { return !recorder.active && (clip_armed || (song_grid_rec_state() & 3U) == 1U); }

/* REC while playing with a clip open: loop recording into it from its next
 * pass, while everything else plays on, armed until the clip goes round.
 * With `now` the take starts at once: the clip's pass begins here. */
SONG_SMALL int song_port_record_clip(unsigned now)
{
    if (!song_port_song()->open || !song_port_player()->playing || recorder.active) return -1;
    clip_armed = 1;
    clip_last = now ? 0xffffffffU : song_player_clock(song_port_player());
    return 0;
}
SONG_SMALL static void clip_take(void)
{
    struct song *s = song_port_song();
    struct song_player *p = song_port_player();
    if (!clip_armed) return;
    uint32_t at = song_player_clock(p);
    if (!s->open || !p->playing) { clip_armed = 0; return; }
    if (at >= clip_last) { clip_last = at; return; }
    clip_armed = 0;
    song_undo_begin(song_port_undo(), s);
    loop_before = s->loop_on;
    song_rec_start(&recorder, s, song_editing(s), SONG_REC_LOOP, 0);
    p->click = s->metronome != 0;
    first_frame = 0; /* keys count from here */
    clip_live = 1;
    early_take();
}
unsigned song_port_clip_armed(void) { return clip_armed; }
unsigned song_port_clip_live(void) { return clip_live; }
void song_port_clip_disarm(void) { clip_armed = 0; }
/* What a key or a chord may set off first: a cell's take starting or
 * ending (song_grid_take.c), then an open clip's take at its next pass. */
SONG_SMALL static void takes_now(void)
{
    song_grid_take_poll();
    clip_take();
}

/* Chord mode's notes into the take, each with its lead after the chord's
 * first, so input quantize moves the chord as a whole. */
SONG_SMALL void song_port_chord_take(unsigned track, const uint8_t *pitch, const uint8_t *offset, unsigned count)
{
    takes_now();
    int64_t clock = song_port_live_clock();
    for (unsigned i = 0; take_waits() && i < count; ++i)
        keep_early(pitch[i], 100, offset[i], groove_midi_input_channel(track));
    for (unsigned i = 0; recorder.active && clock >= 0 && track == recorder.track && i < count; ++i)
        song_rec_lead(&recorder, pitch[i], 100, (uint32_t)clock + offset[i], offset[i]);
}
void song_port_chord_drop(unsigned track, unsigned pitch) { if (track == recorder.track) song_rec_drop(&recorder, pitch); }

/* Live keys on the take track, at the clock playing now. */
SONG_SMALL void song_port_key(const uint8_t *m, size_t bytes)
{
    if (bytes == 3 && (m[0] & 0xe0U) == 0x80U) takes_now(); /* a key at the top of the clip's next pass starts its take */
    int64_t clock = song_port_live_clock();
    if (bytes == 3 && (m[0] & 0xe0U) == 0x80U && (take_waits() || (recorder.active && frame_now() < first_frame))) {
        uint64_t held = keep_early(m[1], (m[0] & 0xf0U) == 0x90U ? m[2] : 0, 0, m[0] & 15U);
        if (held && recorder.active && clock >= 0 && (m[0] & 15U) == groove_midi_input_channel(recorder.track)) {
            song_rec_let_go(&recorder, m[1], held_clocks(held) + 1U); /* let go before the first beat */
            return;
        }
    }
    if (!recorder.active || bytes != 3 || (m[0] & 0xe0U) != 0x80U || clock < 0 ||
        (m[0] & 15U) != groove_midi_input_channel(recorder.track)) return;
    song_rec_note(&recorder, m[1], (m[0] & 0xf0U) == 0x90U ? m[2] : 0, (uint32_t)clock);
}
/* From a stop. A take is one undo step. Loop recording starts at the loop
 * and repeats it. */
SONG_SMALL int song_port_record(unsigned mode, unsigned track)
{
    struct song *s = song_port_song();
    struct song_player *p = song_port_player();
    if (p->playing || mode == SONG_REC_STEP || mode >= SONG_REC_MODES) return -1;
    song_undo_begin(song_port_undo(), s);
    loop_before = s->loop_on;
    if (mode == SONG_REC_LOOP) { s->loop_on = 1; p->playhead = s->loop_start; }
    song_rec_start(&recorder, s, track, mode, p->playhead);
    p->open_end = !s->open && (mode == SONG_REC_REPLACE || mode == SONG_REC_OVERDUB); /* on past the end (0.5.8) */
    uint32_t from, to;
    if (song_rec_quiet(&recorder, &from, &to)) { p->mute_track = (uint8_t)track; p->mute_from = from; p->mute_to = to; }
    p->click = s->metronome != 0;
#ifdef FM1_OPEN_WEB
    song_port_mark_start();
#endif
    uint64_t now = (uint64_t)song_port_blocks() * FM1_BLOCK_FRAMES;
    early_count = 0;
    song_port_ready_pads();
    song_deliver_start(s->count_in);
    first_frame = s->count_in ? p->frame : now; /* in the count-in the player waits at the first beat */
    return 0;
}
/* Ends the take at `clock`, or what is armed. Playing goes on with
 * keep_playing. A cell's take closes after the take's own step. */
SONG_SMALL static void take_end(unsigned keep_playing, uint32_t clock)
{
    struct song *s = song_port_song();
    struct song_player *p = song_port_player();
    unsigned began = recorder.active && song_port_live_clock() >= 0;
    clip_armed = 0;
    p->open_end = 0;
    if (recorder.active) {
        song_knobs_release(clock); /* before the chase on stop */
        if (!keep_playing) song_port_stop();
        song_rec_stop(&recorder, clock);
        p->mute_track = 0xff;
        p->click = s->metronome == 1;
        if (recorder.mode == SONG_REC_LOOP) s->loop_on = loop_before;
        song_undo_end(song_port_undo(), s);
    } else if (!keep_playing) song_port_stop();
    clip_live = 0;
    song_grid_take_ended(began, clock);
}
SONG_SMALL void song_port_record_end(unsigned keep_playing)
{
    int64_t now = song_port_live_clock();
    unsigned counting = recorder.active && now < 0; /* nothing recorded yet: called off, and playing stops */
    take_end(keep_playing && !counting, song_port_player()->playing && now >= 0 ? (uint32_t)now : recorder.last);
}
SONG_SMALL void song_port_record_stop(void) { song_port_record_end(0); }
SONG_SMALL void song_port_record_end_at(uint32_t clock) { take_end(1, clock); }
unsigned song_port_recording(void) { return recorder.active || clip_armed || song_grid_rec_state(); }
unsigned song_port_take_full(void) { unsigned was = full; full = 0; return was; }

/* Each control poll: a cell's take starting or ending, an open clip's take
 * at its next pass, the song's end, or growing past it, a loop pass, and the
 * song filling up. */
SONG_SMALL void song_takes_poll(void)
{
    struct song *s = song_port_song();
    takes_now();
    if (recorder.active && !song_port_player()->playing) { song_port_record_stop(); return; } /* the song ended */
    int64_t clock = song_port_live_clock();
    if (!recorder.active || clock < 0) return;
    if (s->events + ROOM >= SONG_EVENTS) { full = 1; take_end(1, (uint32_t)clock); return; }
    struct song_player *p = song_port_player();
    while (p->open_end && song_bar_clock(s, s->bars) <= (uint32_t)clock) { /* past the end: the song grows a bar at a time */
        if (s->bars >= SONG_MAX_BARS) p->open_end = 0; /* then playing stops there */
        else ++s->bars;
    }
    if (recorder.mode == SONG_REC_LOOP && (uint32_t)clock < recorder.last) song_knobs_wrap(s->loop_end);
    song_rec_follow(&recorder, (uint32_t)clock);
}
#endif
