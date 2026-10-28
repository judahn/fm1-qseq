/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Song settings: scales, snapping, the scale-step pitch knob, swing at
 * playback and changing tempo while playing. */
#include "harness.h"

static struct song song;
static struct song_player player;

static void test_scales(void)
{
    static const char *const degrees[SONG_SCALES] = {
        "0 1 2 3 4 5 6 7 8 9 10 11", "0 2 4 5 7 9 11", "0 2 3 5 7 8 10", "0 2 3 5 7 9 10",
        "0 1 3 5 7 8 10", "0 2 4 6 7 9 11", "0 2 4 5 7 9 10", "0 1 3 5 6 8 10",
        "0 2 3 5 7 8 11", "0 2 3 5 7 9 11", "0 2 4 7 9", "0 3 5 7 10", "0 3 5 6 7 10"};
    song_clear(&song, 4, 12000);
    for (unsigned k = 0; k < SONG_SCALES; ++k) {
        song.key_scale = (uint8_t)k;
        unsigned mask = 0;
        for (const char *p = degrees[k]; *p;) {
            unsigned d = 0;
            while (*p >= '0' && *p <= '9') d = d * 10 + (unsigned)(*p++ - '0');
            mask |= 1U << d;
            while (*p == ' ') ++p;
        }
        CHECK_EQ(song_scale_mask(&song), mask);
    }
    /* A minor has the notes of C major. */
    song.key_scale = 2; song.key_root = 9;
    unsigned minor = song_scale_mask(&song);
    song.key_scale = 1; song.key_root = 0;
    CHECK_EQ(minor, song_scale_mask(&song));
    /* Snapping goes up to the next scale note, and down only at the top. */
    CHECK_EQ(song_scale_snap(&song, 61), 62);
    CHECK_EQ(song_scale_snap(&song, 60), 60);
    CHECK_EQ(song_scale_snap(&song, 66), 67);
    song.key_root = 1; /* C# major has no G, and 127 is a G */
    CHECK_EQ(song_scale_snap(&song, 127), 126);
    song.key_scale = 0;
    CHECK_EQ(song_scale_snap(&song, 61), 61);
    /* The pitch knob: scale steps, from in or out of the scale. */
    song.key_scale = 2; song.key_root = 9;
    CHECK_EQ(song_scale_step(&song, 69, 1), 71);
    CHECK_EQ(song_scale_step(&song, 71, 1), 72);
    CHECK_EQ(song_scale_step(&song, 72, -1), 71);
    CHECK_EQ(song_scale_step(&song, 70, 1), 71);
    CHECK_EQ(song_scale_step(&song, 70, -1), 69);
    CHECK_EQ(song_scale_step(&song, 69, 7), 81);
    CHECK_EQ(song_scale_step(&song, 127, 1), 127);
    CHECK_EQ(song_scale_step(&song, 0, -1), 0);
    song.key_scale = 0;
    CHECK_EQ(song_scale_step(&song, 60, 3), 63);
}

/* Swing moves only notes exactly on an off-beat, only on swinging tracks,
 * and lands them on the exact block of their swung clock. */
static void test_swing(void)
{
    song_clear(&song, 4, 13300);
    for (unsigned i = 0; i < 16; ++i) {
        song_add_note(&song, 0, i * 240U, 60, 100, 100);       /* 8ths, swinging */
        song_add_note(&song, 1, i * 240U, 48, 100, 100);       /* 8ths, straight */
        song_add_note(&song, 2, i * 120U, 72 + i % 2, 100, 60); /* 16ths */
    }
    song_add_note(&song, 0, 280, 61, 100, 20);  /* off the grid, between 240 and its swung time */
    song_add_note(&song, 0, 1760, 62, 100, 20); /* an 8th triplet: not an off-beat */
    song.swing = 33;
    song.swing_tracks = 0x05; /* tracks 1 and 3 */
    for (unsigned grid = 0; grid < 2; ++grid) {
        song.swing_grid = (uint8_t)grid;
        uint32_t step = grid ? 120 : 240, delay = (step * 33U + 50U) / 100U;
        CHECK_EQ(delay, grid ? 40 : 79);
        song_player_init(&player, &song, &listener);
        reset_capture();
        song_player_start(&player, 0);
        run_blocks(&player, 0, 3000);
        unsigned bad = 0, swung = 0;
        for (unsigned t = 0; t < 3; ++t) {
            unsigned n;
            const struct song_event *e = song_track(&song, t, &n);
            for (unsigned i = 0; i < n; ++i) {
                uint32_t start = song_event_start(&e[i]);
                uint32_t moved = (t != 1 && start % (2 * step) == step) ? delay : 0;
                swung += moved != 0;
                uint32_t on = exact_block(&song, 0, 0, start + moved), off = exact_block(&song, 0, 0, start + moved + e[i].length);
                unsigned found = 0;
                for (unsigned k = 0; k < heard_count; ++k) {
                    if (heard[k].track != t || heard[k].pitch != e[i].a) continue;
                    if (heard[k].velocity && heard[k].block == on) found |= 1;
                    if (!heard[k].velocity && heard[k].block == off) found |= 2;
                }
                bad += found != 3;
            }
        }
        CHECK_EQ(swung, grid ? 8 : 12);
        CHECK_EQ(bad, 0);
        CHECK(balanced());
    }
    /* Swing never changes what is stored. */
    CHECK_EQ(song_event_start(&song.event[song.first[0] + 1]), 240);
    song.swing = 0;
    CHECK_EQ(song_swing(&song, 0, 240), 0);
}

/* A new tempo while playing keeps the place and carries on at the new rate. */
static void test_tempo_change(void)
{
    song_clear(&song, 16, 12000);
    song_player_init(&player, &song, &listener);
    song_player_start(&player, 0);
    run_blocks(&player, 0, 1000);
    uint32_t before = song_player_clock(&player);
    CHECK_EQ(song_player_set_tempo(&player, 0, 9000), 0);
    CHECK_EQ(song_player_clock(&player), before);
    run_blocks(&player, 1000, 1000);
    /* 1000 blocks at 90 BPM: 128000 * 480 * 90 / 2646000 = 2089.8 clocks. */
    uint32_t after = song_player_clock(&player);
    CHECK(after - before >= 2089 && after - before <= 2091);
    /* Inside a later segment, whose start moves when the first tempo does. */
    song_clear(&song, 16, 12000);
    song_set_tempo(&song, 2U * 1920U, 10000);
    song_player_init(&player, &song, &listener);
    song_player_start(&player, 0);
    run_blocks(&player, 0, 1500);
    before = song_player_clock(&player);
    CHECK(before > 2U * 1920U);
    CHECK_EQ(song_player_set_tempo(&player, 0, 14000), 0);
    CHECK_EQ(song_player_clock(&player), before);
    run_blocks(&player, 1500, 100);
    after = song_player_clock(&player); /* still 100 BPM here: 100 blocks = 232.2 clocks */
    CHECK(after - before >= 231 && after - before <= 233);
}

int main(void)
{
    test_scales();
    test_swing();
    test_tempo_change();
    return report("test_feel");
}
