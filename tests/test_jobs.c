/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Edit jobs, notes and events (phase 6): each job's results on edge cases,
 * notes across the range edges, triplets, chords and empty ranges, with
 * undo and redo after each one. */
#include "jobs_harness.h"
#include "clips.h"

static void test_quantize(void)
{
    fresh(4);
    note(0, 10, 60, 100, 100);        /* 16th grid: to 0 */
    note(0, 170, 62, 100, 100);       /* to 120, by 50%: 145 */
    note(0, 1900, 64, 100, 50);       /* past the last 16th: onto the next bar, 1920 */
    note(0, 2000, 65, 100, 50);       /* outside the range: stays */
    note(1, 150, 48, 100, 90);        /* 8th triplet grid, 160 */
    struct song_range r = range(0, 1920, 1);
    CHECK_EQ(JOB(song_job_quantize(&song, &r, 120, 100, 50, 0)), 3);
    CHECK_EQ(song_event_start(nth_note(0, 0)), 0);
    CHECK_EQ(song_event_start(nth_note(0, 1)), 120);
    CHECK_EQ(song_event_start(nth_note(0, 2)), 1920);
    CHECK_EQ(song_event_start(nth_note(0, 3)), 2000);
    undo_redo();
    CHECK_EQ(song_undo(&undo, &song), 0);
    CHECK_EQ(JOB(song_job_quantize(&song, &r, 120, 50, 50, 0)), 3); /* 10 to 5, 170 to 145, 1900 to 1910 */
    CHECK_EQ(song_event_start(nth_note(0, 0)), 5);
    CHECK_EQ(song_event_start(nth_note(0, 1)), 145);
    CHECK_EQ(song_event_start(nth_note(0, 2)), 1910);
    CHECK_EQ(song_undo(&undo, &song), 0);
    r.tracks = 2;
    CHECK_EQ(JOB(song_job_quantize(&song, &r, 160, 100, 50, 0)), 1);
    CHECK_EQ(song_event_start(nth_note(1, 0)), 160);
    CHECK_EQ(song_undo(&undo, &song), 0);
    /* Swing 66 on 8ths: the off-beat sits at 320 of each 480. */
    fresh(2);
    note(0, 300, 60, 100, 100);
    note(0, 470, 62, 100, 100);
    note(0, 610, 64, 100, 100);       /* 480 is nearer than the swung 796 */
    r = range(0, 3840, 1);
    CHECK_EQ(JOB(song_job_quantize(&song, &r, 240, 100, 66, 0)), 3);
    CHECK_EQ(song_event_start(nth_note(0, 0)), 240 + 76); /* 240 * (132 - 100) / 100 = 76 */
    CHECK_EQ(song_event_start(nth_note(0, 1)), 480);
    CHECK_EQ(song_event_start(nth_note(0, 2)), 480);
    undo_redo();
    /* Gate quantize: the end goes to the grid too. A 7/8 bar ends on its own line. */
    fresh(2);
    CHECK_EQ(song_set_meter(&song, 0, 7, 8), 0);
    note(0, 1250, 60, 100, 200);      /* quarter grid in 7/8: lines 0, 480, 960, 1440, then the bar end 1680 */
    note(0, 1600, 62, 100, 30);       /* nearest: the bar end, 1680 */
    r = range(0, 1680, 1);
    CHECK_EQ(JOB(song_job_quantize(&song, &r, 480, 100, 50, 1)), 2);
    CHECK_EQ(song_event_start(nth_note(0, 0)), 1440);
    CHECK_EQ(nth_note(0, 0)->length, 480);       /* the end met the start: one grid */
    CHECK_EQ(song_event_start(nth_note(0, 1)), 1680);
    undo_redo();
    r = range(10, 10, 1);
    CHECK_EQ(song_job_quantize(&song, &r, 120, 100, 50, 0), -1); /* an empty range */
    /* In the clip editor the clip's end is its top (0.5.1): a note that
     * lands there goes round, with its gate kept. The song's don't. */
    fresh(4);
    note(1, 1900, 66, 100, 50);
    CHECK_EQ(song_clip_open(&song, song_cell(0, 0)), 0);  /* an empty cell: a bar long */
    note(0, 1900, 64, 100, 50);
    note(0, 1000, 65, 100, 50);
    r = range(0, 1920, 3);
    CHECK_EQ(JOB(song_job_quantize(&song, &r, 120, 100, 50, 1)), 3);
    CHECK(has_note(0, 0, 64) && has_note(0, 960, 65));
    CHECK_EQ(nth_note(0, 0)->length, 120);
    CHECK(has_note(1, 1920, 66));
    undo_redo();
    song_clip_close(&song);
}

static void test_velocity_gate(void)
{
    fresh(4);
    for (unsigned i = 0; i < 4; ++i) note(0, i * 480, 60, 40 + i * 25, 240); /* 40 65 90 115 */
    note(0, 960, 64, 70, 240);        /* a chord */
    note(1, 0, 40, 100, 100);
    struct song_range r = range(480, 1920, 1);
    CHECK_EQ(JOB(song_job_velocity(&song, &r, SONG_JOB_SET, 99, 0)), 4);
    CHECK_EQ(nth_note(0, 0)->b, 40);
    CHECK_EQ(nth_note(0, 1)->b, 99);
    undo_redo();
    CHECK_EQ(song_undo(&undo, &song), 0);
    CHECK_EQ(JOB(song_job_velocity(&song, &r, SONG_JOB_RATE, 200, 10)), 4);
    CHECK_EQ(nth_note(0, 1)->b, 127);            /* 65 * 2 + 10, clamped */
    CHECK_EQ(song_undo(&undo, &song), 0);
    CHECK_EQ(JOB(song_job_velocity(&song, &r, SONG_JOB_RATE, 0, -5)), 4);
    CHECK_EQ(nth_note(0, 1)->b, 1);              /* never 0 */
    CHECK_EQ(song_undo(&undo, &song), 0);
    r = range(0, 1920, 1);
    CHECK_EQ(JOB(song_job_crescendo(&song, &r, -40)), 4);
    CHECK_EQ(nth_note(0, 0)->b, 40);             /* 0 at the start */
    CHECK_EQ(nth_note(0, 1)->b, 65 - 10);        /* a quarter of the way */
    CHECK_EQ(nth_note(0, 4)->b, 115 - 30);
    undo_redo();
    CHECK_EQ(song_undo(&undo, &song), 0);
    CHECK_EQ(JOB(song_job_gate(&song, &r, SONG_JOB_LEGATO, 0, 0)), 4);
    CHECK_EQ(nth_note(0, 0)->length, 480);
    CHECK_EQ(nth_note(0, 2)->length, 480);       /* both chord notes, to the next start */
    CHECK_EQ(nth_note(0, 3)->length, 480);
    CHECK_EQ(nth_note(0, 4)->length, 240);       /* the last note: nothing after it */
    undo_redo();
    CHECK_EQ(song_undo(&undo, &song), 0);
    CHECK_EQ(JOB(song_job_gate(&song, &r, SONG_JOB_RATE, 50, -200)), 5);
    CHECK_EQ(nth_note(0, 0)->length, 1);
    CHECK_EQ(song_undo(&undo, &song), 0);
    CHECK_EQ(JOB(song_job_gate(&song, &r, SONG_JOB_SET, 30, 0)), 5);
    CHECK_EQ(nth_note(0, 4)->length, 30);
    undo_redo();
}

static void test_pitch(void)
{
    fresh(2);
    note(0, 0, 60, 100, 100);
    note(0, 0, 66, 100, 100);
    note(0, 480, 126, 100, 100);
    note(1, 0, 36, 100, 100);         /* a drum track */
    struct song_range r = {0, 1920, 3, 2};
    CHECK_EQ(JOB(song_job_transpose(&song, &r, 3, 0, 0, 127)), 3);
    CHECK_EQ(nth_note(0, 0)->a, 63);
    CHECK_EQ(nth_note(0, 2)->a, 127);
    CHECK_EQ(nth_note(1, 0)->a, 36);
    undo_redo();
    CHECK_EQ(song_undo(&undo, &song), 0);
    CHECK_EQ(JOB(song_job_transpose(&song, &r, 12, 0, 61, 100)), 1); /* only inside the note range */
    CHECK(has_note(0, 0, 60) && has_note(0, 0, 78));
    CHECK_EQ(song_undo(&undo, &song), 0);
    song.key_root = 0; song.key_scale = 1; /* C major */
    CHECK_EQ(JOB(song_job_transpose(&song, &r, 2, 1, 0, 127)), 3);
    CHECK(has_note(0, 0, 64) && has_note(0, 0, 69)); /* C up two steps: E. F#: G, A */
    CHECK_EQ(song_undo(&undo, &song), 0);
    song.key_root = 2; song.key_scale = 2; /* D minor: D E F G A Bb C */
    CHECK_EQ(JOB(song_job_fit_scale(&song, &r)), 2);
    CHECK(has_note(0, 0, 60) && has_note(0, 0, 67)); /* C is in, F# goes up to G */
    CHECK(has_note(0, 480, 127));                /* 126 F#: up to G, 127 */
    CHECK_EQ(nth_note(1, 0)->a, 36);
    undo_redo();
    CHECK_EQ(song_undo(&undo, &song), 0);
    song.key_scale = 0;
    CHECK_EQ(song_job_fit_scale(&song, &r), 0);   /* chromatic: nothing moves */
}

static void test_shift_chords(void)
{
    fresh(2);
    note(0, 100, 60, 100, 100);
    note(0, 1800, 62, 100, 100);
    lane(0, 900, 7, 50);
    struct song_range r = range(0, 1920, 1);
    CHECK_EQ(JOB(song_job_shift(&song, &r, 200)), 3);
    CHECK_EQ(song_event_start(nth_note(0, 0)), 300);
    CHECK_EQ(song_event_start(nth_note(0, 1)), 1919); /* kept inside */
    CHECK_EQ(song_lane_at(&song, 0, 7, 1100), 50);
    CHECK_EQ(song_lane_find(&song, 0, 7, 1100) >= 0, 1);
    undo_redo();
    CHECK_EQ(song_undo(&undo, &song), 0);
    CHECK_EQ(JOB(song_job_shift(&song, &r, -300)), 3);
    CHECK_EQ(song_event_start(nth_note(0, 0)), 0);
    undo_redo();
    /* Chords: entered E C G, sorted C E G, down G E C, and undo puts E C G back. */
    fresh(2);
    note(0, 0, 64, 100, 480); note(0, 0, 60, 100, 480); note(0, 0, 67, 100, 480);
    note(0, 960, 50, 100, 480);       /* one note: no chord */
    lane(0, 0, 7, 100);
    r = range(0, 1920, 1);
    CHECK_EQ(JOB(song_job_chord_sort(&song, &r, 0)), 3);
    CHECK(nth_note(0, 0)->a == 60 && nth_note(0, 1)->a == 64 && nth_note(0, 2)->a == 67);
    CHECK_EQ(song_undo(&undo, &song), 0);
    CHECK(nth_note(0, 0)->a == 64 && nth_note(0, 1)->a == 60 && nth_note(0, 2)->a == 67);
    CHECK_EQ(song_redo(&undo, &song), 0);
    CHECK_EQ(song_undo(&undo, &song), 0);
    CHECK_EQ(JOB(song_job_chord_sort(&song, &r, 1)), 3);
    CHECK(nth_note(0, 0)->a == 67 && nth_note(0, 1)->a == 64 && nth_note(0, 2)->a == 60);
    CHECK_EQ(song_job_chord_sort(&song, &r, 1), 0); /* already in order */
    CHECK_EQ(song_undo(&undo, &song), 0);
    /* Strum up by 20: 0, 20, 40, ends held at 480. Notes landing on a later
     * chord's start are not strummed again. */
    note(0, 40, 70, 100, 100); note(0, 40, 72, 100, 100);
    CHECK_EQ(JOB(song_job_strum(&song, &r, 20, 0)), 5);
    unsigned found = 0;
    for (unsigned i = 0; i < song.count[0]; ++i) {
        const struct song_event *e = at_index(0, i);
        if (!song_is_note(e)) continue;
        if (e->a == 60) found += song_event_start(e) == 0 && e->length == 480;
        if (e->a == 64) found += song_event_start(e) == 20 && e->length == 460;
        if (e->a == 67) found += song_event_start(e) == 40 && e->length == 440;
        if (e->a == 70) found += song_event_start(e) == 40 && e->length == 100;
        if (e->a == 72) found += song_event_start(e) == 60 && e->length == 80;
    }
    CHECK_EQ(found, 5);
    undo_redo();
    r = range(1000, 1900, 1);
    CHECK_EQ(song_job_strum(&song, &r, 20, 0), 0); /* nothing there */
}

static void test_events(void)
{
    fresh(4);
    note(0, 0, 60, 100, 100); note(0, 960, 62, 100, 100);
    lane(0, 0, 7, 100); lane(0, 480, 74, 20); lane(0, 960, 74, 40); lane(0, 1440, 74, 60); lane(0, 1700, 74, 80);
    const struct song_event program = song_lane_event(480, SONG_LANE_PROGRAM, 5);
    song_undo_begin(&undo, &song); song_add_event(&song, 0, &program); song_undo_end(&undo, &song);
    struct song_range r = range(0, 1920, 1);
    CHECK_EQ(JOB(song_job_erase(&song, &r, SONG_JOB_PROGRAMS)), 1);
    CHECK_EQ(song.count[0], 7);
    undo_redo();
    CHECK_EQ(song_undo(&undo, &song), 0);
    CHECK_EQ(JOB(song_job_extract(&song, &r, SONG_JOB_CONTROLS, 6)), 5);
    CHECK_EQ(song.count[0], 3);
    CHECK_EQ(song.count[6], 5);
    undo_redo();
    CHECK_EQ(song_undo(&undo, &song), 0);
    CHECK_EQ(JOB(song_job_thin(&song, &r, 74)), 2); /* 480 and 1440 stay */
    CHECK_EQ(song_lane_find(&song, 0, 74, 960), -1);
    CHECK(song_lane_find(&song, 0, 74, 1440) >= 0);
    undo_redo();
    CHECK_EQ(song_undo(&undo, &song), 0);
    /* A CC ramp replaces the lane in the range: 0 to 120, every 120 clocks. */
    struct song_range two = range(0, 1920, 3), bars4 = range(0, 7680, 1);
    CHECK_EQ(JOB(song_job_ramp(&song, &two, 74, 0, 120, 120, 0)), 32);
    CHECK_EQ(song_lane_at(&song, 0, 74, 0), 0);
    CHECK_EQ(song_lane_at(&song, 0, 74, 960), 64);
    CHECK_EQ(song_lane_at(&song, 1, 74, 1800), 120);
    CHECK_EQ(song_lane_at(&song, 0, 74, 1919), 120);
    undo_redo();
    CHECK_EQ(song_undo(&undo, &song), 0);
    CHECK_EQ(JOB(song_job_ramp(&song, &r, 74, 0, 120, 120, 16)), 16); /* slow start: x squared */
    CHECK_EQ(song_lane_at(&song, 0, 74, 960), 34);  /* 120 * (8/15)^2 */
    CHECK_EQ(song_undo(&undo, &song), 0);
    CHECK_EQ(JOB(song_job_ramp(&song, &r, 74, 0, 120, 120, -16)), 16);
    CHECK_EQ(song_lane_at(&song, 0, 74, 960), 94);  /* fast start: 2x - x squared */
    CHECK_EQ(song_undo(&undo, &song), 0);
    /* A tempo ramp, then thinning it. Too many points: refused, unchanged. */
    CHECK_EQ(JOB(song_job_ramp(&song, &bars4, SONG_JOB_TEMPO, 120, 60, 480, 0)), 16);
    CHECK_EQ(song.tempos, 16);
    CHECK_EQ(song.tempo[0].bpm100, 12000);
    CHECK_EQ(song.tempo[15].bpm100, 6000);
    undo_redo();
    CHECK_EQ(JOB(song_job_thin(&song, &bars4, SONG_JOB_TEMPO)), 7);
    CHECK_EQ(song.tempos, 9);
    undo_redo();
    CHECK_EQ(song_undo(&undo, &song), 0);
    CHECK_EQ(song_undo(&undo, &song), 0);
    before = song;
    CHECK_EQ(song_job_ramp(&song, &bars4, SONG_JOB_TEMPO, 120, 60, 30, 0), -1);
    CHECK(same_song(&song, &before));
    /* Whole tracks: copy over, merge, clear. A clip block stays put. */
    const struct song_event stop = {960U | (uint32_t)SONG_FX << 24, SONG_BLOCK_STOP, 0, 240};
    song_undo_begin(&undo, &song); song_add_event(&song, 0, &stop); song_undo_end(&undo, &song);
    note(3, 0, 30, 100, 10);
    CHECK_EQ(JOB(song_job_copy_track(&song, 0, 3)), 8); /* all but the block */
    CHECK_EQ(song.count[3], 8);
    undo_redo();
    CHECK_EQ(JOB(song_job_merge(&song, 3, 4)), 8);
    CHECK(song.count[3] == 0 && song.count[4] == 8);
    undo_redo();
    CHECK_EQ(JOB(song_job_clear(&song, 1 | 16)), 16);
    CHECK_EQ(song.count[0], 1);                  /* the block */
    undo_redo();
    CHECK_EQ(song_job_copy_track(&song, 2, 2), -1);
}

int main(void)
{
    test_quantize();
    test_velocity_gate();
    test_pitch();
    test_shift_chords();
    test_events();
    return report("test_jobs");
}
