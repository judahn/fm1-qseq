/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Live performance glue. K1 to K4 while recording: each knob drives one
 * lane on the take track. A turn sends the new value at once and writes it
 * into the take (touch.h). From its first turn a knob latches until the
 * take ends: its value holds when it stops turning, everything the playhead
 * passes on its lane is replaced, and only then does the lane go back to
 * what the song had (0.6.2, it let go 300 ms after the last turn before).
 * The player keeps a held lane quiet meanwhile. */
#include "song_port.h"
#include "instrument.h"
#include "lanes.h"
#include "touch.h"
#include "record.h"
#if SONG_UI
_Static_assert(SONG_DX7 == GROOVE_DX7 && SONG_303 == GROOVE_303 && SONG_808 == GROOVE_808 &&
               SONG_PSG == GROOVE_PSG && SONG_PLUCK == GROOVE_PLUCK && SONG_SAMPLER == GROOVE_SAMPLE,
               "engine numbers");
static struct { struct song_touch touch; uint8_t value, held, wrapped; } knobs[4]; /* wrapped: a loop pass ended on its touch */

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
SONG_SMALL void song_knob_turn(unsigned knob, int delta)
{
    struct song_recorder *r = song_port_recorder();
    struct song_player *p = song_port_player();
    if (!r->active || knob >= 4 || !delta) return;
    unsigned track = r->track, lane = song_knob_lane(track, knob);
    int value = (int)song_knob_value(track, knob) + delta;
    value = value < 0 ? 0 : value > 127 ? 127 : value;
    if (knobs[knob].held && knobs[knob].touch.lane != lane) song_knobs_release((uint32_t)song_player_clock(p));
    if (!knobs[knob].held) knobs[knob].wrapped = 0;
    knobs[knob].value = (uint8_t)value;
    knobs[knob].held = 1;
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
SONG_SMALL void song_knobs_release(uint32_t clock)
{
    for (unsigned k = 0; k < 4; ++k)
        if (knobs[k].held) let_go(k, clock);
    song_port_player()->touch_track = 0xff;
}
/* A loop pass ends. What it wrote becomes older data for the next pass,
 * and a latched knob writes its value again from the loop's start, so the
 * next pass replaces this one and the last pass is what stays (touch.h). */
SONG_SMALL void song_knobs_wrap(uint32_t loop_end)
{
    struct song *s = song_port_song();
    unsigned track = song_port_recorder()->track;
    for (unsigned k = 0; k < 4; ++k)
        if (knobs[k].held && knobs[k].touch.active) {
            song_touch_wrap(s, track, &knobs[k].touch, loop_end, !knobs[k].wrapped);
            knobs[k].wrapped = 1;
        }
    song_touch_settle(s, track);
    for (unsigned k = 0; k < 4; ++k)
        if (knobs[k].held) song_touch_again(s, track, &knobs[k].touch, knobs[k].touch.lane, knobs[k].value, s->loop_start);
}
#endif
