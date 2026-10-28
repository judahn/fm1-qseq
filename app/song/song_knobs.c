/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Live performance glue. K1 to K4 while recording: each knob drives one
 * lane on the take track. A turn sends the new value at once and writes it
 * into the take (touch.h), and a knob left alone for a moment lets go, so
 * the lane goes back to what the song had. The player keeps a held lane
 * quiet meanwhile. And GLITCH: the stutter, written to the FX lane when a
 * take is running. */
#include "song_port.h"
#include "instrument.h"
#include "lanes.h"
#include "touch.h"
#include "record.h"
#include "glitch.h"
#if SONG_UI
_Static_assert(SONG_DX7 == GROOVE_DX7 && SONG_303 == GROOVE_303 && SONG_808 == GROOVE_808 &&
               SONG_PSG == GROOVE_PSG && SONG_PLUCK == GROOVE_PLUCK && SONG_SAMPLER == GROOVE_SAMPLE,
               "engine numbers");
enum { LET_GO_MS = 300 };
static struct { struct song_touch touch; uint32_t moved; uint8_t value, held; } knobs[4];

unsigned song_port_engine(unsigned track) { return groove_source_type(track); }
SONG_SMALL unsigned song_knob_lane(unsigned track, unsigned knob)
{
    return song_knob_cc(song_port_song(), song_port_engine(track), track, knob);
}
unsigned song_knob_held(unsigned knob) { return knob < 4 && knobs[knob].held; }

/* Before a lane has anything: the engine's own volume and reverb send,
 * or the parameter's usual starting point. */
SONG_SMALL static unsigned engine_value(unsigned track, unsigned lane)
{
    if (lane == 7) { struct groove_view v = {0}; groove_get_view(track, 0, &v); return v.volume; }
    if (lane == 91) { uint8_t sends[2] = {0}; groove_sends_get(track, sends); return sends[0] * 127U / 15U; }
    const struct song_param *p = song_param_for(song_port_engine(track), lane);
    return p ? p->initial : 64U;
}
SONG_SMALL unsigned song_knob_value(unsigned track, unsigned knob)
{
    if (knob >= 4) return 0;
    unsigned lane = song_knob_lane(track, knob);
    if (knobs[knob].held && song_port_recorder()->track == track && knobs[knob].touch.lane == lane)
        return knobs[knob].value;
    int value = song_lane_at(song_port_song(), track, lane, song_player_clock(song_port_player()));
    return value >= 0 ? (unsigned)value : engine_value(track, lane);
}
SONG_SMALL static void send(unsigned track, unsigned lane, unsigned value)
{
    const uint8_t message[3] = {(uint8_t)(0xb0U | groove_midi_input_channel(track)), (uint8_t)lane, (uint8_t)value};
    (void)song_midi_post(message, 3);
}
SONG_SMALL void song_knob_turn(unsigned knob, int delta, uint32_t now)
{
    struct song_recorder *r = song_port_recorder();
    struct song_player *p = song_port_player();
    if (!r->active || knob >= 4 || !delta) return;
    unsigned track = r->track, lane = song_knob_lane(track, knob);
    int value = (int)song_knob_value(track, knob) + delta;
    value = value < 0 ? 0 : value > 127 ? 127 : value;
    if (knobs[knob].held && knobs[knob].touch.lane != lane) song_knobs_release((uint32_t)song_player_clock(p));
    knobs[knob].value = (uint8_t)value;
    knobs[knob].held = 1;
    knobs[knob].moved = now;
    knobs[knob].touch.lane = (uint8_t)lane;
    p->touch_track = (uint8_t)track;
    p->touch_lane[knob] = (uint8_t)lane;
    send(track, lane, (unsigned)value);
    int64_t clock = song_port_live_clock(); /* the count-in sounds but doesn't record */
    if (clock >= 0) song_touch_move(song_port_song(), track, &knobs[knob].touch, lane, (unsigned)value, (uint32_t)clock);
}
SONG_SMALL static void let_go(unsigned knob, uint32_t clock)
{
    unsigned track = song_port_recorder()->track;
    int old = song_touch_release(song_port_song(), track, &knobs[knob].touch, clock);
    if (old >= 0) send(track, knobs[knob].touch.lane, (unsigned)old);
    knobs[knob].held = 0;
    song_port_player()->touch_lane[knob] = 0xff;
}
SONG_SMALL void song_knobs_poll(uint32_t now)
{
    int64_t clock = song_port_live_clock();
    for (unsigned k = 0; k < 4; ++k)
        if (knobs[k].held && now - knobs[k].moved >= LET_GO_MS && clock >= 0) let_go(k, (uint32_t)clock);
}
SONG_SMALL void song_knobs_release(uint32_t clock)
{
    for (unsigned k = 0; k < 4; ++k)
        if (knobs[k].held) let_go(k, clock);
    song_port_player()->touch_track = 0xff;
}
/* The next pass may touch what this one wrote, as it would older data. */
SONG_SMALL void song_knobs_wrap(uint32_t loop_end)
{
    struct song *s = song_port_song();
    unsigned track = song_port_recorder()->track, count;
    song_knobs_release(loop_end);
    song_track(s, track, &count);
    for (struct song_event *e = s->event + s->first[track]; count--; ++e)
        if (!song_is_note(e)) e->at &= ~SONG_FRESH;
}

static uint8_t stutter_slice, stutter_on;
static int8_t stutter_drift;
SONG_SMALL void song_stutter_hold(unsigned slice, int drift)
{
    struct song_player *p = song_port_player();
    uint32_t state = song_port_lock();
    song_player_stutter(p, song_slice_clocks(slice), drift);
    song_port_unlock(state);
    stutter_slice = (uint8_t)slice;
    stutter_drift = (int8_t)drift;
    stutter_on = p->stut_at != 0xffffffffU;
}
SONG_SMALL void song_stutter_drift(int drift)
{
    stutter_drift = (int8_t)drift;
    song_player_stutter_drift(song_port_player(), drift);
}
SONG_SMALL void song_stutter_release(void)
{
    struct song_player *p = song_port_player();
    if (!stutter_on) return;
    stutter_on = 0;
    uint32_t state = song_port_lock(), began = song_player_stutter_stop(p), to = p->stut_to;
    song_port_unlock(state);
    if (began != 0xffffffffU && song_port_recorder()->active && to > began)
        (void)song_fx_add(song_port_song(), began, to - began, stutter_slice, stutter_drift);
}
unsigned song_stuttering(void) { return stutter_on && song_port_player()->stut_at != 0xffffffffU; }
#endif
