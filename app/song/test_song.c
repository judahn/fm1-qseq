/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Phase 1 test song: 12 bars of A minor on the default project's sounds.
 * T1 DX7 lead, T2 303 bass, T3 PSG, T4 pluck, T5 sampler, T6 DX7 pad,
 * T7 303 counter line, T8 808 kit. It has 16ths, 8th and 16th triplets, a
 * 64th run, 32nd fills, chords, five markers and a tempo change at bar 9
 * (120 to 133 BPM), so every timing path gets used. */
#include "song.h"
#include "song_config.h"

enum { BAR = 1920, Q = 480, E = 240, S = 120, T8 = 160, T16 = 80, N32 = 60, N64 = 30 };
enum { LEAD, BASS, PSG, PLUCK, SAMPLER, PAD, COUNTER, DRUMS };
enum { KICK = 36, SNARE = 38, CLAP = 39, HAT = 42, OPEN_HAT = 46, RIM = 37, TOM_LOW = 47, TOM_HIGH = 48 };

static struct song *song;
SONG_SMALL static void n(unsigned track, uint32_t at, unsigned pitch, unsigned velocity, unsigned length)
{
    (void)song_add_note(song, track, at, pitch, velocity, length);
}
/* Chords per bar, low to high, and the bass root for each. */
static const uint8_t chords[12][4] = {
    {57, 60, 64, 69}, {57, 60, 64, 69}, {57, 60, 64, 69}, {53, 57, 60, 65},
    {55, 60, 64, 67}, {55, 59, 62, 67}, {57, 60, 64, 69}, {52, 56, 59, 64},
    {57, 60, 64, 69}, {53, 57, 60, 65}, {55, 59, 62, 67}, {57, 60, 64, 69}};
static const uint8_t roots[12] = {33, 33, 33, 29, 36, 31, 33, 28, 33, 29, 31, 33};

/* Lead phrases: pitch and length pairs, played back to back. */
SONG_SMALL static void phrase(uint32_t at, const uint8_t (*notes)[2], unsigned count, unsigned unit)
{
    for (unsigned i = 0; i < count; at += notes[i][1] * unit, ++i)
        if (notes[i][0]) n(LEAD, at, notes[i][0], 96, notes[i][1] * unit - 10);
}
SONG_SMALL static void lead(void)
{
    static const uint8_t bar3[][2] = {{69, 2}, {72, 1}, {71, 1}, {69, 2}, {67, 2}};
    static const uint8_t bar4[][2] = {{65, 2}, {69, 1}, {72, 1}, {77, 4}};
    static const uint8_t bar6[][2] = {{71, 1}, {72, 1}, {74, 2}, {71, 2}, {67, 2}};
    static const uint8_t bar8[][2] = {{64, 2}, {68, 2}, {71, 2}};
    for (unsigned i = 0; i < 4; ++i) n(LEAD, 0, chords[0][i] + (i == 3 ? 2 : 0), 70, 2 * BAR); /* held chord */
    phrase(2 * BAR, bar3, 5, E);
    phrase(3 * BAR, bar4, 4, E);
    /* Bar 5: two groups of 8th triplets, then a half note. */
    static const uint8_t triplets[6] = {76, 74, 72, 74, 72, 71};
    for (unsigned i = 0; i < 6; ++i) n(LEAD, 4 * BAR + i * T8, triplets[i], i % 3 ? 84 : 104, T8 - 10);
    n(LEAD, 4 * BAR + 2 * Q, 72, 96, 2 * Q);
    phrase(5 * BAR, bar6, 5, E);
    phrase(7 * BAR, bar8, 3, E);
    /* Bar 8, beat 4: a 64th-note run up two octaves of A minor. */
    static const uint8_t run[16] = {69, 71, 72, 74, 76, 77, 79, 81, 83, 84, 86, 88, 89, 91, 93, 95};
    for (unsigned i = 0; i < 16; ++i) n(LEAD, 7 * BAR + 3 * Q + i * N64, run[i], 70 + i * 3, N64);
    /* Bars 9 and 10 at the new tempo: 16th arpeggios. */
    for (unsigned bar = 8; bar < 10; ++bar)
        for (unsigned i = 0; i < 16; ++i)
            n(LEAD, bar * BAR + i * S, chords[bar][i % 4] + 12, i % 4 ? 80 : 110, S - 20);
    /* Bar 11: 16th triplets down a G chord. */
    static const uint8_t falling[6] = {79, 74, 71, 67, 74, 71};
    for (unsigned i = 0; i < 24; ++i) n(LEAD, 10 * BAR + i * T16, falling[i % 6], i % 3 ? 76 : 100, T16 - 10);
    n(LEAD, 11 * BAR, 69, 100, BAR - 30);
}

SONG_SMALL static void band(void)
{
    for (unsigned bar = 2; bar < 12; ++bar) {
        uint32_t at = bar * BAR;
        unsigned brk = bar == 6 || bar == 7, root = roots[bar];
        /* 303: 16ths with octave hops, quarter notes in the break. */
        for (unsigned i = 0; i < 16; ++i)
            if (brk ? !(i % 4) : (0x5577U >> i) & 1U)
                n(BASS, at + i * S, root + (i % 4 == 2 ? 12 : 0), i % 4 ? 80 : 112, brk ? Q - 40 : S - 20);
        /* Pluck chord stabs on beats 2 and 4. */
        for (unsigned beat = 1; beat < 4; beat += 2)
            for (unsigned i = 0; i < 4; ++i) n(PLUCK, at + beat * Q, chords[bar][i], 90, 200);
        /* DX7 pad: root and fifth, a bar long. */
        n(PAD, at, chords[bar][0], 64, BAR - 40);
        n(PAD, at, chords[bar][2], 64, BAR - 40);
        /* PSG: 8th-triplet arpeggios in bars 5 to 8 and 11 to 12. */
        if ((bar >= 4 && bar < 8) || bar >= 10)
            for (unsigned i = 0; i < 12; ++i) n(PSG, at + i * T8, chords[bar][i % 3] + 12, 88, T8 - 20);
        /* Sampler: rim on the offbeat 8ths, shaker 16ths in the break. */
        for (unsigned i = 0; i < 8; ++i)
            if (!brk && (i & 1)) n(SAMPLER, at + i * E, RIM, 70, 100);
        if (brk)
            for (unsigned i = 0; i < 16; ++i) n(SAMPLER, at + i * S, HAT, i % 2 ? 60 : 90, 100);
        /* 303 counter line in the fast section: 8ths on root and seventh. */
        if (bar >= 8)
            for (unsigned i = 0; i < 8; ++i) n(COUNTER, at + i * E, root + 24 + (i & 1 ? 10 : 0), 90, E - 40);
    }
    /* Bar 6, beat 4: a 32nd tom fill on the sampler. */
    for (unsigned i = 0; i < 8; ++i) n(SAMPLER, 5 * BAR + 3 * Q + i * N32, i & 1 ? TOM_LOW : TOM_HIGH, 100, N32);
}

SONG_SMALL static void drums(void)
{
    for (unsigned bar = 0; bar < 12; ++bar) {
        uint32_t at = bar * BAR;
        if (bar < 2) { /* intro: kick on one, 8th hats */
            n(DRUMS, at, KICK, 110, S);
            for (unsigned i = 0; i < 8; ++i) n(DRUMS, at + i * E, HAT, i & 1 ? 60 : 90, S);
        } else if (bar == 6 || bar == 7) { /* break: kick, claps, 8th hats */
            n(DRUMS, at, KICK, 120, S);
            n(DRUMS, at + Q, CLAP, 100, S);
            n(DRUMS, at + 3 * Q, CLAP, 100, S);
            for (unsigned i = 0; i < 8; ++i) n(DRUMS, at + i * E, HAT, 70, S);
        } else {
            n(DRUMS, at, KICK, 120, S);
            n(DRUMS, at + Q + Q / 2, KICK, 90, S);
            n(DRUMS, at + 2 * Q, KICK, 110, S);
            n(DRUMS, at + Q, SNARE, 110, S);
            n(DRUMS, at + 3 * Q, SNARE, 110, S);
            for (unsigned i = 0; i < 16; ++i)
                if (i != 14) n(DRUMS, at + i * S, HAT, i & 1 ? 55 : 85, S / 2);
            n(DRUMS, at + 14 * S, OPEN_HAT, 90, E);
        }
    }
    /* Bar 8, beat 4: a 32nd snare roll into the fast section. */
    for (unsigned i = 0; i < 8; ++i) n(DRUMS, 7 * BAR + 3 * Q + i * N32, SNARE, 60 + i * 8, N32);
}

SONG_SMALL void song_build_test(struct song *s)
{
    song = s;
    song_clear(s, 12, 12000);
    (void)song_set_tempo(s, 8 * BAR, 13300);
#if SONG_UI /* markers and the loop only matter to the song view */
    (void)song_add_marker(s, 0, "INTRO");
    (void)song_add_marker(s, 2 * BAR, "GROOVE");
    (void)song_add_marker(s, 6 * BAR, "BREAK");
    (void)song_add_marker(s, 8 * BAR, "FAST");
    (void)song_add_marker(s, 10 * BAR, "OUTRO");
    (void)song_set_loop(s, 2 * BAR, 6 * BAR);
#endif
    lead();
    band();
    drums();
}
