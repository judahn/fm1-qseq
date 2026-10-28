/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Step entry settings and in-place edits. The knobs always show and change
 * the entry settings. When the pointer sits on notes, the selected notes
 * take the new length, gate or velocity too, so what is shown is what they
 * get. Landing on notes loads their settings, as far as they match. */
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
SONG_SMALL static void preview(struct song_step *st)
{
    unsigned index[STEP_CHORD], n = step_selection(st, index);
    step_preview_stop(st);
    for (unsigned i = 0; i < n; ++i) {
        const struct song_event *e = &st->song->event[index[i]];
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
    preview(st);
}

SONG_SMALL static void apply_length(struct song_step *st)
{
    unsigned index[STEP_CHORD], n = step_selection(st, index);
    uint32_t length = step_gate_clocks(st, step_clocks(st));
    for (unsigned i = 0; i < n; ++i) {
        struct song_event e = st->song->event[index[i]];
        e.length = (uint16_t)length;
        step_change(st, index[i], &e);
    }
}
SONG_SMALL void step_set_length(struct song_step *st, int delta)
{
    int length = (int)st->length + delta;
    st->length = (uint8_t)(length < 0 ? 0 : length >= STEP_LENGTHS ? STEP_LENGTHS - 1 : length);
    fix_dots(st);
    apply_length(st);
}
SONG_SMALL void step_pick_length(struct song_step *st, unsigned length)
{
    st->length = (uint8_t)(length < STEP_LENGTHS ? length : STEP_QUARTER);
    fix_dots(st);
    apply_length(st);
}
SONG_SMALL void step_toggle_triplet(struct song_step *st)
{
    st->triplet ^= 1U;
    fix_dots(st);
    apply_length(st);
}
SONG_SMALL void step_cycle_dots(struct song_step *st)
{
    st->dots = (uint8_t)((st->dots + 1U) % 3U);
    if (st->length == STEP_64TH && !st->triplet && st->dots == 2) st->dots = 0;
    apply_length(st);
}
SONG_SMALL void step_set_gate(struct song_step *st, int delta)
{
    int gate = (int)st->gate + delta;
    st->gate = (uint8_t)(gate < 0 ? 0 : gate >= STEP_GATES ? STEP_GATES - 1 : gate);
    apply_length(st);
}
/* On notes: a relative change, so a chord keeps its balance. On an empty
 * position: 1 to 127, then R1 to R4. */
SONG_SMALL void step_set_velocity(struct song_step *st, int delta)
{
    unsigned index[STEP_CHORD], n = step_selection(st, index);
    if (n) {
        for (unsigned i = 0; i < n; ++i) {
            struct song_event e = st->song->event[index[i]];
            int v = e.b + delta;
            e.b = (uint8_t)(v < 1 ? 1 : v > 127 ? 127 : v);
            step_change(st, index[i], &e);
        }
        st->velocity = st->song->event[index[0]].b;
        st->random_level = 0;
        return;
    }
    int v = (st->random_level ? 127 + st->random_level : st->velocity) + delta;
    v = v < 1 ? 1 : v > 127 + STEP_RANDOMS ? 127 + STEP_RANDOMS : v;
    st->random_level = (uint8_t)(v > 127 ? v - 127 : 0);
    if (v <= 127) st->velocity = (uint8_t)v;
}
/* Glitch, one field at a time: on notes each keeps its other fields, so a
 * chord keeps its differences. On an empty position: the entry setting. */
SONG_SMALL int step_set_glitch(struct song_step *st, unsigned field, int delta)
{
    unsigned index[STEP_CHORD], n = step_selection(st, index);
    int result = 0;
    for (unsigned i = 0; i < n; ++i) {
        uint32_t word = song_glitch_turn(song_glitch_word(st->song, &st->song->event[index[i]]), field, delta);
        result |= song_glitch_apply(st->song, st->track, index[i] - st->song->first[st->track], word);
    }
    st->glitch = n ? song_glitch_word(st->song, &st->song->event[index[0]]) : song_glitch_turn(st->glitch, field, delta);
    return result;
}
SONG_SMALL void step_pick(struct song_step *st, unsigned pitch)
{
    if (pitch > 127) return;
    if (!st->picked) memset(st->pick, 0, sizeof st->pick);
    st->picked = 1;
    st->pick[pitch / 32U] |= 1U << (pitch % 32U);
}

SONG_SMALL static int find_pitch(const struct song_step *st, unsigned pitch)
{
    unsigned count, i = song_find(st->song, st->track, st->pointer);
    const struct song_event *e = song_track(st->song, st->track, &count);
    for (; i < count && song_event_start(&e[i]) == st->pointer; ++i)
        if (song_is_note(&e[i]) && e[i].a == pitch) return 1;
    return 0;
}
/* Move each selected note, highest first going up and lowest first going
 * down, so a chord moves without notes bumping into each other. A note that
 * would land on another note's pitch stays put. */
SONG_SMALL static unsigned move(struct song_step *st, int steps, unsigned octaves, const uint8_t *pads, unsigned count)
{
    unsigned index[STEP_CHORD], n = step_selection(st, index), moved = 0;
    for (unsigned a = 0; a < n; ++a) /* by pitch */
        for (unsigned b = a + 1; b < n; ++b)
            if (st->song->event[index[b]].a < st->song->event[index[a]].a) {
                unsigned t = index[a]; index[a] = index[b]; index[b] = t;
            }
    for (unsigned k = 0; k < n; ++k) {
        unsigned at = index[steps > 0 ? n - 1 - k : k];
        struct song_event changed = st->song->event[at], *e = &changed;
        int to = e->a;
        if (pads) {
            unsigned pad = 0;
            while (pad < count && pads[pad] != e->a) ++pad;
            int next = (int)pad + steps;
            if (pad == count || next < 0 || next >= (int)count) continue;
            to = pads[next];
        } else if (octaves) to = e->a + steps * 12;
        else to = (int)song_scale_step(st->song, e->a, steps);
        if (to < 0 || to > 127 || to == e->a || find_pitch(st, (unsigned)to)) continue;
        if (st->picked) {
            st->pick[e->a / 32U] &= ~(1U << (e->a % 32U));
            st->pick[to / 32] |= 1U << (to % 32);
        }
        e->a = (uint8_t)to;
        step_change(st, at, e);
        ++moved;
    }
    if (moved) preview(st);
    return moved;
}
SONG_SMALL unsigned step_transpose(struct song_step *st, int steps, unsigned octaves)
{
    return move(st, steps, octaves, NULL, 0);
}
SONG_SMALL unsigned step_transpose_pads(struct song_step *st, const uint8_t *pads, unsigned count, int steps)
{
    return move(st, steps, 0, pads, count);
}
#endif
