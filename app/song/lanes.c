/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Automation lanes: the parameters each engine takes, reading a lane's
 * value at a point, chasing, and clearing. See lanes.h. */
#include "lanes.h"
#include "song_config.h"
#include <stddef.h>
#if SONG_UI

/* Controllers as web/engine/sound.c's message() and the engines' MIDI
 * handlers take them. CC1 is the LFO depth wheel on most engines, the
 * DX7's own mod wheel, and envelope mod on the 303. */
static const struct song_param params[] = {
    {7, 100, "VOLUME"}, {10, 64, "PAN"}, {11, 127, "EXPRESSION"}, {91, 0, "REVERB"},
    {1, 0, "LFO DEPTH"}, {1, 0, "MOD WHEEL"}, {2, 127, "BREATH"}, {4, 127, "FOOT"},
    {74, 64, "CUTOFF"}, {71, 64, "RESONANCE"}, {1, 64, "ENV MOD"}, {75, 64, "DECAY"},
    {5, 0, "SLIDE"}, {74, 64, "NOISE"}, {72, 32, "RELEASE"}, {74, 100, "BRIGHT"}};
static const uint8_t dx7[] = {0, 1, 5, 3, 2, 6, 7}, bass[] = {8, 9, 10, 11, 0, 1, 2, 3, 12},
                     drums[] = {0, 1, 3, 2, 4}, psg[] = {0, 13, 14, 1, 2, 3, 4},
                     pluck[] = {0, 15, 11, 14, 1, 2, 3, 4};
static const struct { const uint8_t *index; uint8_t count; } engines[SONG_ENGINES] = {
    {dx7, sizeof dx7}, {bass, sizeof bass}, {drums, sizeof drums},
    {psg, sizeof psg}, {pluck, sizeof pluck}, {drums, sizeof drums}};

SONG_SMALL unsigned song_params(unsigned engine, const uint8_t **index)
{
    if (engine >= SONG_ENGINES) engine = SONG_SAMPLER;
    *index = engines[engine].index;
    return engines[engine].count;
}
SONG_SMALL const struct song_param *song_param(unsigned index) { return &params[index % (sizeof params / sizeof params[0])]; }
SONG_SMALL const struct song_param *song_param_for(unsigned engine, unsigned cc)
{
    const uint8_t *index;
    for (unsigned i = 0, n = song_params(engine, &index); i < n; ++i)
        if (params[index[i]].cc == cc) return &params[index[i]];
    return NULL;
}
SONG_SMALL unsigned song_knob_cc(const struct song *s, unsigned engine, unsigned track, unsigned knob)
{
    const uint8_t *index;
    unsigned cc = track < SONG_TRACKS && knob < 4 ? s->knob_cc[track][knob] : 0;
    if (cc && song_param_for(engine, cc)) return cc;
    unsigned n = song_params(engine, &index);
    return params[index[knob < n ? knob : 0]].cc;
}
SONG_SMALL unsigned song_programs(unsigned engine)
{
    return engine == SONG_DX7 ? 128U : engine == SONG_303 ? 2U : engine == SONG_PSG ? 4U : 0U;
}

SONG_SMALL struct song_event song_lane_event(uint32_t clock, unsigned lane, unsigned value)
{
    if (lane == SONG_LANE_PROGRAM) return (struct song_event){clock | (uint32_t)SONG_PROGRAM << 24, (uint8_t)value, 0, 0};
    return (struct song_event){clock | (uint32_t)SONG_CONTROL << 24, (uint8_t)lane, (uint8_t)value, 0};
}
static int matches(const struct song_event *e, unsigned lane) { return song_is_lane(e) && song_event_lane(e) == lane; }

SONG_SMALL int song_lane_at(const struct song *s, unsigned track, unsigned lane, uint32_t clock)
{
    unsigned count, end = song_find(s, track, clock + 1U);
    const struct song_event *e = song_track(s, track, &count);
    for (unsigned i = end; i-- > 0;)
        if (matches(&e[i], lane)) return (int)song_event_value(&e[i]);
    for (unsigned i = end; i < count && lane != SONG_LANE_PROGRAM; ++i)
        if (matches(&e[i], lane)) return (int)song_event_value(&e[i]);
    return -1;
}
SONG_SMALL int song_lane_find(const struct song *s, unsigned track, unsigned lane, uint32_t clock)
{
    unsigned count, i = song_find(s, track, clock);
    const struct song_event *e = song_track(s, track, &count);
    for (; i < count && song_event_start(&e[i]) == clock; ++i)
        if (matches(&e[i], lane)) return (int)i;
    return -1;
}
SONG_SMALL int song_lane_set(struct song *s, unsigned track, unsigned lane, uint32_t clock, unsigned value, unsigned fresh)
{
    struct song_event e = song_lane_event(clock | (fresh ? SONG_FRESH : 0U), lane, value);
    int at = song_lane_find(s, track, lane, clock);
    if (at < 0) return song_add_event(s, track, &e);
    unsigned count;
    const struct song_event *old = song_track(s, track, &count) + at;
    if (old->at == e.at && old->a == e.a && old->b == e.b) return 0;
    return song_set_event(s, track, (unsigned)at, &e);
}

SONG_SMALL void song_chase(const struct song *s, unsigned track, uint32_t clock, song_lane_fn fn, void *context)
{
    uint8_t value[SONG_LANES];
    uint32_t seen[5] = {0}, send[5] = {0};
    unsigned count, at = song_find(s, track, clock);
    const struct song_event *e = song_track(s, track, &count);
    for (unsigned i = 0; i < count; ++i) {
        if (!song_is_lane(&e[i])) continue;
        unsigned lane = song_event_lane(&e[i]), word = lane >> 5, bit = 1U << (lane & 31U);
        if (i < at) { value[lane] = (uint8_t)song_event_value(&e[i]); seen[word] |= bit; send[word] |= bit; }
        else if (!(seen[word] & bit)) { /* the first change after: it sets the lane until then */
            seen[word] |= bit;
            if (song_event_start(&e[i]) > clock && lane != SONG_LANE_PROGRAM) {
                value[lane] = (uint8_t)song_event_value(&e[i]);
                send[word] |= bit;
            }
        }
    }
    /* Before a track's first program change: the program the song was saved with. */
    unsigned bit = 1U << (SONG_LANE_PROGRAM & 31U), base = s->base_program[track];
    if ((seen[SONG_LANE_PROGRAM >> 5] & bit) && !(send[SONG_LANE_PROGRAM >> 5] & bit) && base) {
        value[SONG_LANE_PROGRAM] = (uint8_t)(base - 1U);
        send[SONG_LANE_PROGRAM >> 5] |= bit;
    }
    for (unsigned lane = 0; lane < SONG_LANES; ++lane)
        if (send[lane >> 5] >> (lane & 31U) & 1U) fn(context, track, lane, value[lane]);
}
SONG_SMALL unsigned song_lane_clear(struct song *s, unsigned tracks, unsigned lane, uint32_t from, uint32_t to)
{
    unsigned removed = 0;
    for (unsigned t = 0; t < SONG_TRACKS; ++t) {
        if (!(tracks >> t & 1U)) continue;
        unsigned count, first = song_find(s, t, from);
        const struct song_event *e = song_track(s, t, &count);
        for (unsigned i = song_find(s, t, to); i-- > first;)
            if (matches(&e[i], lane) && !song_remove(s, t, i)) ++removed;
    }
    return removed;
}
SONG_SMALL int64_t song_lane_next(const struct song *s, unsigned track, unsigned lane, uint32_t clock, int direction)
{
    unsigned count, i = song_find(s, track, direction > 0 ? clock + 1U : clock);
    const struct song_event *e = song_track(s, track, &count);
    if (direction > 0) {
        for (; i < count; ++i) if (matches(&e[i], lane)) return song_event_start(&e[i]);
    } else
        while (i-- > 0) if (matches(&e[i], lane)) return song_event_start(&e[i]);
    return -1;
}
#endif
