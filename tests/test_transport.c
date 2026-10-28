/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Transport: loop seams, jumps at the bar line, note-offs and the song end. */
#include "harness.h"

static struct song song;
static struct song_player player;

/* A two-bar loop at 133 BPM, where a bar is not a whole number of frames.
 * Each pass must play every note once, with the remainder carried so the
 * loop keeps exact time over many passes. */
static void test_loop_seam(void)
{
    song_clear(&song, 8, 13300);
    song_add_note(&song, 0, 1000, 50, 100, 100);   /* before the loop: once */
    song_add_note(&song, 0, 1920, 60, 100, 30);    /* loop start */
    song_add_note(&song, 1, 1950, 61, 100, 30);    /* 64th after it */
    song_add_note(&song, 2, 5730, 62, 100, 30);    /* 64th before the end */
    song_add_note(&song, 3, 5000, 63, 100, 1920);  /* runs past the end: cut at the seam */
    song_add_note(&song, 4, 5760, 64, 100, 30);    /* loop end: never while looping */
    song_set_loop(&song, 1920, 5760);
    song.loop_on = 1;
    song_player_init(&player, &song, &listener);
    reset_capture();
    song_player_start(&player, 0);
    run_blocks(&player, 0, 20000);
    struct exact ls = exact_frame(&song, 1920), le = exact_frame(&song, 5760);
    unsigned passes = 0, bad = 0, seen[5] = {0};
    for (unsigned i = 0; i < heard_count; ++i) {
        const struct heard *h = &heard[i];
        if (!h->velocity) continue;
        ++seen[h->track];
        if (h->track == 4) continue;
        if (h->track == 0 && h->pitch == 60) ++passes;
        unsigned pass = h->track == 0 && h->pitch == 50 ? 0 : passes - 1;
        uint32_t clock = h->track == 0 ? (h->pitch == 50 ? 1000 : 1920) : h->track == 1 ? 1950
            : h->track == 2 ? 5730 : 5000;
        struct exact f = exact_frame(&song, clock);
        /* frame = F(clock) + pass * (F(end) - F(start)), all exact. */
        wide den = f.den * ls.den * le.den;
        wide num = f.num * ls.den * le.den + (wide)pass * (le.num * ls.den - ls.num * le.den) * f.den;
        bad += h->block != (uint32_t)(num / (den * BLOCK));
    }
    CHECK(passes > 10);
    CHECK_EQ(bad, 0);
    CHECK_EQ(seen[0], passes + 1);
    CHECK(seen[1] == passes || seen[1] == passes - 1);
    CHECK(seen[3] == passes || seen[3] == passes - 1);
    CHECK(seen[2] == passes || seen[2] == passes - 1); /* the last pass may be cut short */
    CHECK_EQ(seen[4], 0);
    /* The long note is cut at each seam, in the block where the next pass starts. */
    for (unsigned i = 0; i < heard_count; ++i)
        if (heard[i].track == 3 && !heard[i].velocity) {
            unsigned k = i;
            while (k < heard_count && !(heard[k].velocity && heard[k].pitch == 60)) ++k;
            if (k < heard_count) CHECK_EQ(heard[i].block, heard[k].block);
        }
    song_player_stop(&player);
    CHECK(balanced());
}

/* A marker jump while playing waits for the next bar line, then continues
 * in time. While stopped it moves at once. */
static void test_jump(void)
{
    song_clear(&song, 8, 12000);
    for (unsigned beat = 0; beat < 32; ++beat)
        song_add_note(&song, 0, beat * 480U, 60 + beat / 4, 100, 240);
    song_add_note(&song, 1, 0, 40, 100, 3840); /* sounding across the jump */
    song_add_marker(&song, 5U * 1920U, "SIX");
    song_player_init(&player, &song, &listener);
    song_player_jump(&player, song.marker[0].clock);
    CHECK_EQ(song_player_clock(&player), 9600);
    player.playhead = 0;
    reset_capture();
    song_player_start(&player, 0);
    run_blocks(&player, 0, 216);           /* into beat 2 of bar 1 */
    CHECK_EQ(song_player_clock(&player) / 480U, 1);
    song_player_jump(&player, song.marker[0].clock);
    CHECK_EQ(song_player_target(&player), 9600);
    run_blocks(&player, 216, 1400);
    /* Bar 1 plays out, bar 2 never sounds, bar 6 starts where bar 2 would. */
    CHECK_EQ(nth_on(4)->pitch, 60);
    CHECK_EQ(nth_on(5)->pitch, 65);
    CHECK_EQ(nth_on(5)->block, exact_block(&song, 0, 0, 1920));
    CHECK_EQ(nth_on(6)->block, exact_block(&song, 0, 0, 2400));
    for (unsigned i = 0; i < heard_count; ++i) CHECK(heard[i].pitch != 61);
    /* The long bass note got its note-off at the jump. */
    for (unsigned i = 0; i < heard_count; ++i)
        if (heard[i].track == 1 && !heard[i].velocity) CHECK_EQ(heard[i].block, nth_on(5)->block);
    song_player_stop(&player);
    CHECK(balanced());
}

/* Note-offs on stop, retriggered pitches, the held-note limit and song end. */
static void test_note_offs(void)
{
    song_clear(&song, 2, 12000);
    song_add_note(&song, 0, 0, 60, 100, 960);
    song_add_note(&song, 0, 480, 60, 90, 960);    /* same pitch before the first ends */
    for (unsigned i = 0; i < 20; ++i) song_add_note(&song, 5, 0, 40 + i, 100, 3000);
    song_add_note(&song, 6, 1900, 70, 100, 1920); /* past the song end */
    song_player_init(&player, &song, &listener);
    reset_capture();
    song_player_start(&player, 0);
    run_blocks(&player, 0, 300);
    song_player_stop(&player);
    CHECK(!player.playing);
    CHECK(balanced());
    /* The retrigger sends its note-off before the new note-on. */
    for (unsigned i = 1; i < heard_count; ++i)
        if (heard[i].track == 0 && heard[i].velocity == 90) CHECK(!heard[i - 1].velocity && heard[i - 1].pitch == 60);
    /* Stopping kept the position, and playing on resumes from it. */
    uint32_t at = song_player_clock(&player);
    CHECK(at > 0);
    reset_capture();
    song_player_start(&player, 300U * BLOCK);
    run_blocks(&player, 300, 4000);
    CHECK(!player.playing);              /* stopped by itself at the end */
    CHECK_EQ(song_player_clock(&player), 0);
    CHECK(balanced());
}

int main(void)
{
    test_loop_seam();
    test_jump();
    test_note_offs();
    return report("test_transport");
}
