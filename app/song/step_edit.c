/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Step entry settings and in-place edits. The knobs always show and change
 * the entry settings. When the pointer sits on notes, the selected notes
 * take the new length, gate or velocity too, so what is shown is what they
 * get, and a sweep's notes change with them (step_range.c). Landing on
 * notes loads their settings, as far as they match. The pitch knob is in
 * step_pitch.c. */
#include "step.h"
#include "glitch.h"
#include "song_config.h"
#include <string.h>
#if SONG_UI

/* The QY's ten dynamics. ffff has no published value, so it is 127. */
static const struct { uint8_t velocity; char name[5]; } dynamics[10] = {
    {8, "PPP"}, {24, "PP"}, {40, "P"}, {56, "MP"}, {64, "NORM"},
    {72, "MF"}, {88, "F"}, {104, "FF"}, {120, "FFF"}, {127, "FFFF"}};

static unsigned distance(unsigned a, unsigned b) { return a > b ? a - b : b - a; }
SONG_SMALL const char *step_velocity_name(unsigned velocity)
{
    unsigned best = 0;
    for (unsigned i = 1; i < 10; ++i)
        if (distance(velocity, dynamics[i].velocity) < distance(velocity, dynamics[best].velocity)) best = i;
    return dynamics[best].name;
}
SONG_SMALL void step_length_name(const struct song_step *st, char out[8])
{
    static const char names[STEP_LENGTHS][5] = {"1/64", "1/32", "1/16", "1/8", "1/4", "1/2", "1/1"};
    strcpy(out, names[st->length]);
    if (st->triplet) strcat(out, "T");
    if (st->dots) strcat(out, st->dots == 1 ? "." : "..");
}
/* R1 to R4 scatter around 96 by 8, 16, 24 or 32. */
SONG_SMALL unsigned step_entry_velocity(struct song_step *st)
{
    if (!st->random_level) return st->velocity;
    st->random ^= st->random << 13;
    st->random ^= st->random >> 17;
    st->random ^= st->random << 5;
    unsigned spread = 8U * st->random_level, v = 96U - spread + st->random % (2U * spread + 1U);
    return v > 127 ? 127 : v;
}

SONG_SMALL static void fix_dots(struct song_step *st)
{
    if (st->length == STEP_64TH && !st->triplet && st->dots == 2) st->dots = 1;
}
/* A sampler note sounds at its TUNE (0.6.6): the port tunes its pad. */
SONG_SMALL void step_preview(struct song_step *st)
{
    unsigned index[STEP_CHORD], n = step_selection(st, index);
    step_preview_stop(st);
    for (unsigned i = 0; i < n; ++i) {
        const struct song_event *e = &st->song->event[index[i]];
        if (st->out.tune) st->out.tune(st->out.context, st->track, e->a, song_glitch_tune(song_glitch_word(st->song, e)));
        st->out.note(st->out.context, st->track, e->a, e->b);
        st->preview[st->preview_count++] = e->a;
    }
    st->preview_track = st->track;
    if (n) ++st->serial;
}
/* The first length and gate that give exactly this note length. */
SONG_SMALL static void load(struct song_step *st, const struct song_event *e)
{
    static const uint8_t gates[STEP_GATES] = {1, 0, 2, 3};
    struct song_step trial = *st;
    if (!st->random_level) st->velocity = e->b;
    st->glitch = song_glitch_word(st->song, e);
    for (unsigned g = 0; g <= STEP_GATES; ++g) {
        trial.gate = g ? gates[g - 1] : st->gate;
        for (trial.dots = 0; trial.dots < 3; ++trial.dots)
            for (trial.triplet = 0; trial.triplet < 2; ++trial.triplet)
                for (trial.length = 0; trial.length < STEP_LENGTHS; ++trial.length) {
                    if (trial.length == STEP_64TH && !trial.triplet && trial.dots == 2) continue;
                    if (step_gate_clocks(&trial, step_clocks(&trial)) != e->length) continue;
                    st->length = trial.length; st->triplet = trial.triplet;
                    st->dots = trial.dots; st->gate = trial.gate;
                    return;
                }
    }
}
SONG_SMALL void step_land(struct song_step *st)
{
    unsigned index[STEP_CHORD];
    st->picked = st->tie_ok = 0;
    if (step_selection(st, index)) load(st, &st->song->event[index[0]]);
    step_preview(st);
}

SONG_SMALL static uint32_t entry_length(const struct song_step *st) { return step_gate_clocks(st, step_clocks(st)); }
/* The selected notes take the entry's new length. A sweep's keep their
 * differences instead: each scales as the entry's length did. */
SONG_SMALL static int apply_length(struct song_step *st, uint32_t before)
{
    unsigned index[STEP_CHORD], n = step_selection(st, index);
    uint32_t length = entry_length(st), from, to;
    if (step_range(st, &from, &to)) { step_range_scale(st, before, length); return 0; }
    for (unsigned i = 0; i < n; ++i) {
        struct song_event e = st->song->event[index[i]];
        e.length = (uint16_t)length;
        step_change(st, index[i], &e);
    }
    return 0;
}
SONG_SMALL int step_set_length(struct song_step *st, int delta)
{
    uint32_t before = entry_length(st);
    if (step_too_big(st)) return STEP_TOO_BIG;
    int length = (int)st->length + delta;
    st->length = (uint8_t)(length < 0 ? 0 : length >= STEP_LENGTHS ? STEP_LENGTHS - 1 : length);
    fix_dots(st);
    return apply_length(st, before);
}
SONG_SMALL int step_pick_length(struct song_step *st, unsigned length)
{
    uint32_t before = entry_length(st);
    if (step_too_big(st)) return STEP_TOO_BIG;
    st->length = (uint8_t)(length < STEP_LENGTHS ? length : STEP_QUARTER);
    fix_dots(st);
    return apply_length(st, before);
}
SONG_SMALL int step_toggle_triplet(struct song_step *st)
{
    uint32_t before = entry_length(st);
    if (step_too_big(st)) return STEP_TOO_BIG;
    st->triplet ^= 1U;
    fix_dots(st);
    return apply_length(st, before);
}
SONG_SMALL int step_cycle_dots(struct song_step *st)
{
    uint32_t before = entry_length(st);
    if (step_too_big(st)) return STEP_TOO_BIG;
    st->dots = (uint8_t)((st->dots + 1U) % 3U);
    if (st->length == STEP_64TH && !st->triplet && st->dots == 2) st->dots = 0;
    return apply_length(st, before);
}
SONG_SMALL int step_set_gate(struct song_step *st, int delta)
{
    uint32_t before = entry_length(st);
    if (step_too_big(st)) return STEP_TOO_BIG;
    int gate = (int)st->gate + delta;
    st->gate = (uint8_t)(gate < 0 ? 0 : gate >= STEP_GATES ? STEP_GATES - 1 : gate);
    return apply_length(st, before);
}
/* On notes: a relative change, so a chord keeps its balance and a sweep
 * its accents. On an empty position: 1 to 127, then R1 to R4. */
SONG_SMALL int step_set_velocity(struct song_step *st, int delta)
{
    unsigned index[STEP_CHORD], n = step_selection(st, index);
    uint32_t from, to;
    if (step_too_big(st)) return STEP_TOO_BIG;
    if (step_range(st, &from, &to)) { step_range_velocity(st, delta); return 0; }
    if (n) {
        for (unsigned i = 0; i < n; ++i) {
            struct song_event e = st->song->event[index[i]];
            int v = e.b + delta;
            e.b = (uint8_t)(v < 1 ? 1 : v > 127 ? 127 : v);
            step_change(st, index[i], &e);
        }
        st->velocity = st->song->event[index[0]].b;
        st->random_level = 0;
        return 0;
    }
    int v = (st->random_level ? 127 + st->random_level : st->velocity) + delta;
    v = v < 1 ? 1 : v > 127 + STEP_RANDOMS ? 127 + STEP_RANDOMS : v;
    st->random_level = (uint8_t)(v > 127 ? v - 127 : 0);
    if (v <= 127) st->velocity = (uint8_t)v;
    return 0;
}
/* Glitch, one field at a time: on notes each keeps its other fields, so a
 * chord keeps its differences. On an empty position: the entry setting. */
SONG_SMALL int step_set_glitch(struct song_step *st, unsigned field, int delta)
{
    unsigned index[STEP_CHORD], n = step_selection(st, index);
    uint32_t from, to;
    int result = 0;
    if (step_too_big(st)) return STEP_TOO_BIG;
    if (step_range(st, &from, &to)) return step_range_glitch(st, field, delta);
    for (unsigned i = 0; i < n; ++i) {
        uint32_t word = song_glitch_turn(song_glitch_word(st->song, &st->song->event[index[i]]), field, delta);
        result |= song_glitch_apply(st->song, st->track, index[i] - st->song->first[st->track], word);
    }
    st->glitch = n ? song_glitch_word(st->song, &st->song->event[index[0]]) : song_glitch_turn(st->glitch, field, delta);
    return result;
}
SONG_SMALL void step_pick(struct song_step *st, unsigned pitch)
{
    uint32_t from, to, bit = 1U << (pitch % 32U);
    if (pitch > 127) return;
    step_forget(st); /* picking makes the selection again (0.6.8) */
    if (!st->picked) memset(st->pick, 0, sizeof st->pick);
    if (st->picked && step_range(st, &from, &to)) st->pick[pitch / 32U] ^= bit; /* in a sweep a key toggles its pitch */
    else st->pick[pitch / 32U] |= bit;
    st->picked = 1;
}
#endif
