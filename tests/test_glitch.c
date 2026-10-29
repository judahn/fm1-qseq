/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Glitch: attribute words and the table, repeats and flams at exact blocks,
 * chance and the other random choices, and the mangle jobs with undo. */
#include "harness.h"
#include "glitch.h"
#include "mangle.h"
#include "arrange.h"
#include "undo.h"

static struct song song;
static struct song_player player;
static struct song_undo undo;
static struct song_clip clip;

static void fresh(void)
{
    song_clear(&song, 16, 12000);
    song_undo_init(&undo);
    song.undo = NULL;
}
static void note(unsigned track, uint32_t at, unsigned pitch, unsigned length, uint32_t word)
{
    int g = song_glitch_intern(&song, word);
    const struct song_event e = {at | (uint32_t)(g > 0 ? g : 0) << SONG_GLITCH_SHIFT, (uint8_t)pitch, 100, (uint16_t)length};
    CHECK_EQ(song_add_event(&song, track, &e), 0);
}
static uint32_t word(unsigned field, int value) { return song_glitch_turn(0, field, value); }
static void play_from(uint32_t clock, uint32_t blocks)
{
    song_player_init(&player, &song, &listener);
    player.playhead = clock;
    reset_capture();
    block_now = 0;
    song_player_start(&player, 0);
    run_blocks(&player, 0, blocks);
    song_player_stop(&player);
}
/* Every event on every track, in any order within a clock. */
static uint64_t fingerprint(void)
{
    uint64_t sum = 0;
    for (unsigned t = 0; t < SONG_TRACKS; ++t) {
        unsigned n;
        const struct song_event *e = song_track(&song, t, &n);
        for (unsigned i = 0; i < n; ++i) {
            uint64_t h = (e[i].at ^ (uint64_t)e[i].a << 32 ^ (uint64_t)e[i].b << 40 ^ (uint64_t)e[i].length << 48) * 0x9e3779b97f4a7c15ULL;
            sum += (h ^ h >> 29 ^ t) * 1099511628211ULL;
        }
    }
    return sum;
}

static void test_words(void)
{
    uint32_t w = word(GLITCH_REPEATS, 9);
    CHECK_EQ(song_glitch_repeats(w), 8);
    w = song_glitch_turn(w, GLITCH_DRIFT, -3);
    CHECK_EQ(song_glitch_drift(w), -3);
    w = song_glitch_turn(w, GLITCH_DRIFT, -20);
    CHECK_EQ(song_glitch_drift(w), -8);
    CHECK_EQ(song_glitch_repeats(w), 8); /* other fields untouched */
    CHECK_EQ(song_glitch_percent(word(GLITCH_CHANCE, 4), GLITCH_CHANCE), 50);
    CHECK_EQ(song_glitch_percent(0, GLITCH_CHANCE), 100);
    CHECK_EQ(song_glitch_percent(word(GLITCH_FADE, 3), GLITCH_FADE), 15);
    fresh();
    CHECK_EQ(song_glitch_intern(&song, 0), 0);
    int a = song_glitch_intern(&song, w), b = song_glitch_intern(&song, w);
    CHECK(a > 0 && a == b);
    /* 31 entries, then unused ones come back. */
    for (unsigned i = 1; i < 40; ++i) (void)song_glitch_intern(&song, word(GLITCH_CHANCE, 1) | i << 20);
    note(0, 0, 60, 100, w);
    CHECK(song_glitch_intern(&song, word(GLITCH_FLAM, 1) | 7U << 24) > 0); /* reclaimed */
    CHECK_EQ(song_glitch_word(&song, song_track(&song, 0, &(unsigned){0})), w);
    /* Undo of a glitch edit restores the note and the table. */
    fresh();
    song.undo = &undo;
    note(1, 480, 62, 240, 0);
    song_undo_init(&undo);
    uint64_t before = fingerprint();
    song_undo_begin(&undo, &song);
    CHECK_EQ(song_glitch_apply(&song, 1, 0, word(GLITCH_REPEATS, 2)), 0);
    song_undo_end(&undo, &song);
    CHECK_EQ(song_glitch_repeats(song_glitch_word(&song, song_track(&song, 1, &(unsigned){0}))), 3);
    song_undo(&undo, &song);
    CHECK(fingerprint() == before);
    CHECK_EQ(song.glitch[1], 0);
}

/* Repeats on a synth track, straight and triplet, with drift and fade, and
 * a flam: every hit on its exact block. */
static void test_repeats(void)
{
    fresh();
    static const struct { uint32_t at; uint16_t length; uint8_t repeats; int8_t drift; uint8_t fade; } notes[] = {
        {1920, 480, 3, 0, 0}, {3840, 240, 2, 0, 0}, {5760, 480, 4, 0, 0}, {7680, 240, 5, 2, 3}, {9600, 160, 2, -1, 1}};
    for (unsigned i = 0; i < 5; ++i)
        note(1, notes[i].at, 60, notes[i].length,
             song_glitch_turn(song_glitch_turn(word(GLITCH_REPEATS, notes[i].repeats), GLITCH_DRIFT, notes[i].drift),
                              GLITCH_FADE, notes[i].fade));
    note(1, 11520, 64, 240, word(GLITCH_FLAM, 1));
    play_from(0, 4500);
    unsigned k = 0, bad = 0;
    for (unsigned i = 0; i < 5; ++i) {
        uint32_t w = song_glitch_word(&song, &song.event[song.first[1] + i]);
        unsigned n = song_glitch_repeats(w), fade = song_glitch_percent(w, GLITCH_FADE);
        for (unsigned j = 0; j < n; ++j) {
            const struct heard *h = nth_on(k++);
            uint32_t at = notes[i].at + (j * notes[i].length + n / 2U) / n;
            unsigned velocity = n > 1 ? 100U * (100U - (100U - fade) * j / (n - 1U)) / 100U : 100U;
            bad += !h || h->block != exact_block(&song, 0, 0, at) || h->pitch != 60 + notes[i].drift * (int)j ||
                   h->velocity != velocity;
        }
    }
    CHECK_EQ(k, 4 + 3 + 6 + 8 + 3);
    CHECK_EQ(bad, 0);
    const struct heard *grace = nth_on(k), *flam = nth_on(k + 1);
    CHECK(grace && grace->block == exact_block(&song, 0, 0, 11520 - GLITCH_FLAM_CLOCKS) && grace->velocity == 67);
    CHECK(flam && flam->block == exact_block(&song, 0, 0, 11520) && flam->velocity == 100);
    CHECK(balanced());
}

static void test_random(void)
{
    /* Half the 16ths, about half the time, the same with the same seed. */
    fresh();
    for (unsigned i = 0; i < 128; ++i) note(0, i * 120U, 60 + i % 12, 60, word(GLITCH_CHANCE, 4));
    song.seed = 7;
    play_from(0, 6000);
    unsigned first = ons();
    static struct heard copy[CAPTURE];
    memcpy(copy, heard, heard_count * sizeof heard[0]);
    unsigned copied = heard_count;
    CHECK(first > 40 && first < 88);
    play_from(0, 6000);
    CHECK(heard_count == copied && !memcmp(copy, heard, copied * sizeof heard[0]));
    song.seed = 8;
    play_from(0, 6000);
    CHECK(heard_count != copied || memcmp(copy, heard, copied * sizeof heard[0]));
    /* Each loop pass rolls again. */
    song.seed = 7;
    song.loop_start = 0; song.loop_end = 1920; song.loop_on = 1;
    play_from(0, 2000);
    uint32_t pass[2] = {0};
    for (unsigned i = 0; i < heard_count; ++i)
        for (unsigned p = 0; p < 2; ++p)
            for (unsigned k = 0; k < 16; ++k)
                if (heard[i].velocity && heard[i].block == exact_block(&song, 0, 0, p * 1920U + k * 120U)) pass[p] |= 1U << k;
    CHECK(pass[0] && pass[1] && pass[0] != pass[1]);
    song.loop_on = 0;
    /* Accent, octave jump, pad swap and humanize. */
    fresh();
    note(2, 960, 60, 100, word(GLITCH_ACCENT, 1));
    note(2, 1920, 60, 100, song_glitch_turn(word(GLITCH_OCTAVE, 3), GLITCH_RANGE, 0));
    note(7, 960, 36, 60, word(GLITCH_SWAP, 3));
    for (unsigned i = 0; i < 64; ++i) note(4, 2880 + i * 120U, 50, 60, 0);
    song.humanize_time[4] = 10; song.humanize_velocity[4] = 20;
    song_player_init(&player, &song, &listener);
    player.pad_count[7] = 3; player.pads[7][0] = 40; player.pads[7][1] = 41; player.pads[7][2] = 42;
    reset_capture();
    block_now = 0;
    song_player_start(&player, 0);
    run_blocks(&player, 0, 4500);
    unsigned accent = 0, octave = 0, swapped = 0, human = 0, human_bad = 0, moved = 0;
    for (unsigned i = 0; i < heard_count; ++i) {
        const struct heard *h = &heard[i];
        if (!h->velocity) continue;
        if (h->track == 2 && h->block == exact_block(&song, 0, 0, 960)) accent = h->velocity == 127;
        if (h->track == 2 && h->block == exact_block(&song, 0, 0, 1920)) octave = h->pitch == 48 || h->pitch == 72;
        if (h->track == 7) swapped = h->pitch >= 40 && h->pitch <= 42;
        if (h->track == 4) {
            uint32_t at = 2880 + human * 120U;
            uint32_t lo = exact_block(&song, 0, 0, at - 10), hi = exact_block(&song, 0, 0, at + 10);
            human_bad += h->block < lo || h->block > hi || h->velocity < 80 || h->velocity > 120;
            moved += h->block != exact_block(&song, 0, 0, at);
            ++human;
        }
    }
    CHECK(accent && octave && swapped);
    CHECK_EQ(human, 64);
    CHECK_EQ(human_bad, 0);
    CHECK(moved > 10);
    song_player_stop(&player);
    CHECK(balanced());
}

static void test_mangle(void)
{
    fresh();
    song.undo = &undo;
    for (unsigned i = 0; i < 16; ++i) note(0, i * 120U, 60 + i, 60, 0);
    for (unsigned i = 0; i < 8; ++i) note(1, i * 240U, 40 + i, 100, 0);
    const struct song_event cc = {600 | (uint32_t)SONG_CONTROL << 24, 74, 10, 0};
    song_add_event(&song, 0, &cc);
    song_undo_init(&undo);
    uint64_t before = fingerprint();
    song_undo_begin(&undo, &song);
    CHECK(song_scramble(&song, 1, 0, 1920, 99) > 0);
    song_undo_end(&undo, &song);
    unsigned n, seen = 0, ok = 1;
    const struct song_event *e = song_track(&song, 0, &n);
    CHECK_EQ(n, 17);
    for (unsigned i = 0; i < n; ++i) {
        if (!song_is_note(&e[i])) { ok &= song_event_start(&e[i]) == 600; continue; }
        ok &= song_event_start(&e[i]) % 120U == 0 && !(seen >> (song_event_start(&e[i]) / 120U) & 1U);
        seen |= 1U << (song_event_start(&e[i]) / 120U);
        ok &= !i || song_event_start(&e[i]) >= song_event_start(&e[i - 1]);
    }
    CHECK(ok);
    CHECK_EQ(seen, 0xffff);
    uint64_t scrambled = fingerprint();
    song_undo(&undo, &song);
    CHECK(fingerprint() == before);
    song_redo(&undo, &song);
    CHECK(fingerprint() == scrambled);
    song_undo(&undo, &song);
    /* Reverse: the last 16th first, lengths kept. */
    song_undo_begin(&undo, &song);
    CHECK_EQ(song_reverse(&song, 1, 0, 1920), 16);
    song_undo_end(&undo, &song);
    e = song_track(&song, 0, &n);
    CHECK(e[0].a == 75 && song_event_start(&e[0]) == 60 && e[n - 1].a == 60 && song_event_start(&e[n - 1]) == 1860);
    song_undo(&undo, &song);
    CHECK(fingerprint() == before);
    /* Shuffle between tracks 1 and 2: nothing lost, some moved, undoable. */
    song_undo_begin(&undo, &song);
    int moved = song_shuffle_tracks(&song, 3, 0, 1920, 5);
    song_undo_end(&undo, &song);
    unsigned a, b;
    song_track(&song, 0, &a); song_track(&song, 1, &b);
    CHECK(moved > 0);
    CHECK_EQ(a + b, 25);
    CHECK(a != 17);
    song_undo(&undo, &song);
    CHECK(fingerprint() == before);
}

/* Copy and paste keep glitch through another table. */
static void test_travel(void)
{
    fresh();
    uint32_t w = song_glitch_turn(word(GLITCH_REPEATS, 4), GLITCH_FLAM, 1);
    note(2, 480, 60, 240, w);
    CHECK_EQ(song_copy(&song, &clip, 1U << 2, 0, 1920), 0);
    CHECK_EQ(clip.count, 1);
    memset(song.glitch, 0, sizeof song.glitch);
    song.glitch[1] = 12345; /* another song's table */
    CHECK_EQ(song_paste(&song, &clip, 3840, 5, 0), 0);
    unsigned n;
    const struct song_event *e = song_track(&song, 5, &n);
    CHECK_EQ(n, 1);
    CHECK_EQ(song_glitch_word(&song, &e[0]), w);
}

int main(void)
{
    test_words();
    test_repeats();
    test_random();
    test_mangle();
    test_travel();
    return report("test_glitch");
}
