/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Song model, bars, beats, snapping and the tempo map. */
#include "harness.h"

static struct song song;

static void test_model(void)
{
    song_clear(&song, 8, 12000);
    CHECK_EQ(song.bars, 8);
    CHECK_EQ(song.tempos, 1);
    /* Notes stay sorted per track whatever order they arrive in. */
    CHECK_EQ(song_add_note(&song, 2, 960, 60, 100, 120), 0);
    CHECK_EQ(song_add_note(&song, 2, 0, 62, 100, 120), 0);
    CHECK_EQ(song_add_note(&song, 0, 480, 64, 90, 240), 0);
    CHECK_EQ(song_add_note(&song, 2, 480, 63, 100, 120), 0);
    CHECK_EQ(song_add_note(&song, 7, 30, 36, 127, 30), 0);
    unsigned n;
    const struct song_event *e = song_track(&song, 2, &n);
    CHECK_EQ(n, 3);
    CHECK_EQ(song_event_start(&e[0]), 0);
    CHECK_EQ(song_event_start(&e[1]), 480);
    CHECK_EQ(song_event_start(&e[2]), 960);
    CHECK_EQ(e[1].a, 63);
    e = song_track(&song, 7, &n);
    CHECK_EQ(n, 1);
    CHECK_EQ(e[0].a, 36);
    CHECK_EQ(song_find(&song, 2, 481), 2);
    CHECK_EQ(song_find(&song, 2, 0), 0);
    CHECK_EQ(song.events, 5);
    /* Invalid input is refused. */
    CHECK(song_add_note(&song, SONG_PARTS, 0, 60, 100, 10) < 0); /* past the clips' parts */
    CHECK(song_add_note(&song, 0, 0, 128, 100, 10) < 0);
    CHECK(song_add_note(&song, 0, 0, 60, 0, 10) < 0);
    CHECK(song_add_note(&song, 0, 0, 60, 100, 0) < 0);
    CHECK(song_set_tempo(&song, 0, 1999) < 0);
    CHECK(song_set_loop(&song, 960, 960) < 0);
    /* Markers stay sorted and keep their names. */
    CHECK_EQ(song_add_marker(&song, 3840, "B"), 0);
    CHECK_EQ(song_add_marker(&song, 0, "A"), 0);
    CHECK_EQ(song_add_marker(&song, 7680, "LONGNAME9"), 0);
    CHECK_EQ(song.markers, 3);
    CHECK_EQ(song.marker[0].name[0], 'A');
    CHECK_EQ(song.marker[1].clock, 3840);
    CHECK(!memcmp(song.marker[2].name, "LONGNAME", 8));
    CHECK_EQ(song_marker_step(&song, 0, 1), 1);
    CHECK_EQ(song_marker_step(&song, 3840, 1), 2);
    CHECK_EQ(song_marker_step(&song, 3840, -1), 0);
    CHECK_EQ(song_marker_step(&song, 7680, 1), -1);
    /* Removing takes every marker at exactly that clock, and only there. */
    CHECK_EQ(song_add_marker(&song, 3840, "B2"), 0);
    unsigned revision = song.revision;
    CHECK_EQ(song_remove_marker(&song, 3841), 0);
    CHECK_EQ(song.revision, revision);
    CHECK_EQ(song_remove_marker(&song, 3840), 2);
    CHECK(song.revision != revision);
    CHECK_EQ(song.markers, 2);
    CHECK(song.marker[0].clock == 0 && song.marker[1].clock == 7680);
    CHECK(!memcmp(song.marker[1].name, "LONGNAME", 8));
    CHECK_EQ(song_remove_marker(&song, 0), 1);
    CHECK_EQ(song_remove_marker(&song, 7680), 1);
    CHECK_EQ(song.markers, 0);
    /* The pool fills, then refuses. */
    song_clear(&song, 999, 12000);
    unsigned added = 0;
    while (!song_add_note(&song, added % 8, added * 30U, 60, 100, 30)) ++added;
    CHECK_EQ(added, SONG_EVENTS);
}

static void test_bars(void)
{
    song_clear(&song, 999, 12000);
    CHECK_EQ(song_bar_clock(&song, 0), 0);
    CHECK_EQ(song_bar_clock(&song, 998), 998U * 1920U);
    CHECK_EQ(song_end(&song), 999U * 1920U);
    /* 3/4 from bar 3, 7/8 from bar 5. */
    CHECK_EQ(song_set_meter(&song, 2, 3, 4), 0);
    CHECK_EQ(song_set_meter(&song, 4, 7, 8), 0);
    CHECK_EQ(song_bar_clock(&song, 2), 3840);
    CHECK_EQ(song_bar_clock(&song, 4), 3840 + 2 * 1440);
    CHECK_EQ(song_bar_clock(&song, 5), 3840 + 2 * 1440 + 1680);
    uint32_t start, length;
    CHECK_EQ(song_bar_at(&song, 3840 + 1440 + 5, &start, &length), 3);
    CHECK_EQ(start, 3840 + 1440);
    CHECK_EQ(length, 1440);
    unsigned bbc[3];
    song_bbc(&song, 3840 + 2 * 1440 + 3 * 240 + 17, bbc);
    CHECK_EQ(bbc[0], 5); /* bar 5, 4th eighth, clock 17 */
    CHECK_EQ(bbc[1], 4);
    CHECK_EQ(bbc[2], 17);
    song_bbc(&song, 998U * 1920U, bbc); /* not 4/4 any more, so not bar 999 */
    CHECK(bbc[0] > 999);
}

static void test_snap(void)
{
    song_clear(&song, 16, 12000);
    song_set_meter(&song, 2, 3, 4);
    /* Zoom 0 moves bar to bar, zoom 1 by half bars, 2 by beats, 3 by 8ths. */
    CHECK_EQ(song_snap_move(&song, 0, 0, 1), 1920);
    CHECK_EQ(song_snap_move(&song, 100, 0, 1), 1920);
    CHECK_EQ(song_snap_move(&song, 100, 0, -1), 0);
    CHECK_EQ(song_snap_move(&song, 1920, 1, 1), 2880);
    CHECK_EQ(song_snap_move(&song, 3840, 1, 1), 3840 + 720);   /* half of 3/4 */
    CHECK_EQ(song_snap_move(&song, 3840, 1, -1), 2880);
    CHECK_EQ(song_snap_move(&song, 3840, 2, 3), 3840 + 1440);
    CHECK_EQ(song_snap_move(&song, 3841, 3, -1), 3840);
    CHECK_EQ(song_snap_move(&song, 3840, 3, -1), 3840 - 240);
    CHECK_EQ(song_snap_move(&song, 0, 0, -5), 0);
    CHECK_EQ(song_snap_move(&song, 0, 0, 100), song_end(&song));
    CHECK_EQ(song_snap_move(&song, 3840 + 1440 + 10, 0, -1), 3840 + 1440);
}

static void test_tempo_map(void)
{
    song_clear(&song, 999, 13333);
    CHECK_EQ(song_set_tempo(&song, 400U * 1920U + 557U, 9753), 0);
    CHECK_EQ(song_set_tempo(&song, 700U * 1920U + 77U, 21111), 0);
    CHECK_EQ(song.tempos, 3);
    CHECK_EQ(song_tempo_at(&song, 0), 0);
    CHECK_EQ(song_tempo_at(&song, 400U * 1920U + 556U), 0);
    CHECK_EQ(song_tempo_at(&song, 400U * 1920U + 557U), 1);
    CHECK_EQ(song_tempo_at(&song, 998U * 1920U), 2);
    /* Each segment start is the exact time of its first clock, rounded down
     * to the new segment's unit: under 1/(480 * bpm100) of a frame per tempo
     * change, however long the song. */
    for (unsigned k = 0; k < song.tempos; ++k) {
        struct exact f = exact_frame(&song, song.tempo[k].clock);
        wide rate = song_rate(song.tempo[k].bpm100), min_rate = song_rate(9753);
        wide error = f.num * rate - (wide)song.tempo_units[k] * f.den; /* frames * den * rate */
        CHECK(error >= 0);
        CHECK(error * min_rate < (wide)(k + 1) * f.den * rate);
    }
    CHECK_EQ(song_rescale(10, 3, 4), 8);
    CHECK_EQ(song_rescale(12, 3, 4), 9);
    CHECK(song_rescale(0xffffffffffffULL, 14400000U, 960000U) == 0xffffffffffffULL * 15U);
}

int main(void)
{
    test_model();
    test_bars();
    test_snap();
    test_tempo_map();
    return report("test_model");
}
