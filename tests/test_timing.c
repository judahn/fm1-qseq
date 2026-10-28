/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Player timing: drift over 999 bars, odd note values and carried remainders. */
#include "harness.h"

static struct song song;
static struct song_player player;

/* 999 bars with two tempo changes at odd positions. Every note must land on
 * the block that exact rational arithmetic gives, from bar 1 to bar 999. */
static void test_no_drift(void)
{
    song_clear(&song, 999, 12000);
    song_set_tempo(&song, 400U * 1920U + 557U, 9753);
    song_set_tempo(&song, 700U * 1920U + 160U, 21111);
    for (unsigned bar = 0; bar < 999; ++bar) {
        song_add_note(&song, 0, bar * 1920U, 60, 100, 240);
        song_add_note(&song, 1, bar * 1920U + 1280U, 64, 100, 30); /* last 8th triplet */
        song_add_note(&song, 2, bar * 1920U + 1890U, 67, 100, 30); /* last 64th */
    }
    song_add_note(&song, 3, 998U * 1920U + 1919U, 72, 100, 1);    /* very last clock */
    song_player_init(&player, &song, &listener);
    reset_capture();
    song_player_start(&player, 0);
    uint32_t blocks = 0, mismatched = 0;
    while (player.playing && blocks < 2000000U) {
        run_blocks(&player, blocks, 1);
        ++blocks;
        if (blocks % 50000U == 0) {
            /* The playhead is the last clock started before this frame. */
            uint32_t c = song_player_clock(&player);
            struct exact at = exact_frame(&song, c), after = exact_frame(&song, c + 1);
            wide frame = (wide)blocks * BLOCK;
            CHECK(at.num <= frame * at.den && after.num > frame * after.den);
        }
    }
    CHECK(!player.playing);
    CHECK_EQ(ons(), 3U * 999U + 1U);
    for (unsigned i = 0, n = 0; i < heard_count; ++i) {
        if (!heard[i].velocity) continue;
        unsigned track = heard[i].track, bar = n / 3U;
        uint32_t clock = track == 3 ? 998U * 1920U + 1919U
            : bar * 1920U + (track == 0 ? 0U : track == 1 ? 1280U : 1890U);
        mismatched += heard[i].block != exact_block(&song, 0, 0, clock);
        ++n;
    }
    CHECK_EQ(mismatched, 0);
    CHECK(balanced());
    /* The song ends after bar 999 at the exact end frame. */
    CHECK_EQ(blocks - 1, exact_block(&song, 0, 0, song_end(&song)));
}

/* Triplets, 64ths and a 32nd triplet at an awkward tempo, played from the
 * top and from a playhead that is not on a bar line. */
static void test_odd_values(void)
{
    song_clear(&song, 8, 13300);
    unsigned count = 0;
    for (unsigned i = 0; i < 12; ++i, ++count) song_add_note(&song, 0, 1920U + i * 160U, 60 + i, 100, 150);
    for (unsigned i = 0; i < 24; ++i, ++count) song_add_note(&song, 1, 3840U + i * 80U, 40 + i, 100, 70);
    for (unsigned i = 0; i < 32; ++i, ++count) song_add_note(&song, 2, 5760U + i * 30U, 70 + i % 16, 100, 30);
    for (unsigned i = 0; i < 12; ++i, ++count) song_add_note(&song, 3, 7680U + i * 40U, 50 + i, 100, 40);
    static const uint32_t origins[2] = {0, 1920U + 160U};
    static const uint64_t starts[2] = {0, 128000U};
    for (unsigned run = 0; run < 2; ++run) {
        song_player_init(&player, &song, &listener);
        player.playhead = origins[run];
        reset_capture();
        song_player_start(&player, starts[run]);
        uint32_t first = (uint32_t)(starts[run] / BLOCK);
        run_blocks(&player, first, 4000);
        unsigned expected = run ? count - 1 : count, mismatched = 0;
        CHECK_EQ(ons(), expected);
        for (unsigned t = 0; t < 4; ++t) {
            unsigned c;
            const struct song_event *e = song_track(&song, t, &c);
            for (unsigned i = 0; i < c; ++i) {
                uint32_t clock = song_event_start(&e[i]);
                if (clock < origins[run]) continue;
                const struct heard *h = NULL;
                for (unsigned k = 0; k < heard_count; ++k)
                    if (heard[k].velocity && heard[k].track == t && heard[k].pitch == e[i].a &&
                        heard[k].block == exact_block(&song, starts[run], origins[run], clock)) h = &heard[k];
                mismatched += !h;
            }
        }
        CHECK_EQ(mismatched, 0);
        CHECK(balanced());
    }
}

/* Remainders across thousands of loop passes, and across tempo changes when
 * playback starts off the grid. Song time must stay exact, not just close. */
static void test_carry(void)
{
    song_clear(&song, 4, 13300);
    song_add_note(&song, 0, 1920, 60, 100, 30);
    song_add_note(&song, 1, 2370, 61, 100, 30);
    song_set_loop(&song, 1920, 2400);  /* one beat: 19894.7 frames */
    song.loop_on = 1;
    song_player_init(&player, &song, &listener);
    reset_capture();
    song_player_start(&player, 0);
    run_blocks(&player, 0, 300000);
    struct exact ls = exact_frame(&song, 1920), le = exact_frame(&song, 2400);
    struct exact length = sum(le, (struct exact){-ls.num, ls.den});
    unsigned wraps = 0, bad = 0;
    for (unsigned i = 0; i < heard_count; ++i) {
        if (!heard[i].velocity) continue;
        if (heard[i].pitch == 60 && i) ++wraps;
        struct exact at = sum(exact_frame(&song, heard[i].pitch == 60 ? 1920 : 2370), scaled(length, wraps));
        bad += heard[i].block != block_of(at);
    }
    CHECK(wraps > 1900);
    CHECK_EQ(bad, 0);
    /* Song time now: audio time less every pass played. */
    struct exact played = scaled(length, wraps);
    played.num = -played.num;
    CHECK(time_matches(&player, sum((struct exact){300000 * BLOCK, 1}, played)));

    /* Four tempo changes at odd clocks, started from an odd clock. */
    song_clear(&song, 64, 13333);
    song_set_tempo(&song, 10U * 1920U + 77U, 9753);
    song_set_tempo(&song, 20U * 1920U + 333U, 21111);
    song_set_tempo(&song, 33U * 1920U + 1U, 7777);
    song_set_tempo(&song, 50U * 1920U + 959U, 15000);
    for (uint32_t c = 0; c + 37 < 64U * 1920U; c += 37) song_add_note(&song, 0, c, 60 + c % 24, 100, 30);
    const uint32_t origin = 1234;
    const uint64_t start = 7 * BLOCK;
    song_player_init(&player, &song, &listener);
    player.playhead = origin;
    reset_capture();
    song_player_start(&player, start);
    unsigned drifted = 0;
    for (uint32_t b = 7; player.playing; ++b) {
        run_blocks(&player, b, 1);
        if (player.playing && b % 997 == 0) {
            struct exact now = exact_frame(&song, origin);
            drifted += !time_matches(&player, sum(now, (struct exact){(wide)(b + 1) * BLOCK - start, 1}));
        }
    }
    CHECK_EQ(drifted, 0);
    bad = 0;
    unsigned played_notes = 0;
    for (unsigned i = 0; i < heard_count; ++i) {
        if (!heard[i].velocity) continue;
        uint32_t clock = origin + (37 - origin % 37) % 37 + 37 * played_notes++;
        bad += heard[i].block != exact_block(&song, start, origin, clock) || heard[i].pitch != 60 + clock % 24;
    }
    CHECK(played_notes > 3000);
    CHECK_EQ(bad, 0);
}

int main(void)
{
    test_no_drift();
    test_odd_values();
    test_carry();
    return report("test_timing");
}
