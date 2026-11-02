/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Step entry's moved notes as the selection (0.6.8). The owner's case that
 * 0.6.7 refused, a sweep moving on past notes of its pitches, now goes,
 * and the same notes stay selected. A note landed on is replaced, put back
 * as the run moves on, and replaced for good where the run stops, undo
 * bringing every one back in one step. The moved notes stay the selection
 * through the knobs, the pitch knob and the glitch layer, and ENV deletes
 * exactly them, until anything that makes the selection again. A sampler
 * pad at two TUNEs at one clock and exact duplicates move together. The
 * run keeps STEP_COVERS notes to put back, and its undo step stays the
 * size it was over a long run back and forth. Playing, notes landed on and
 * put back neither hang nor sound twice. */
#include "harness.h"
#include "step.h"
#include "undo.h"
#include "glitch.h"

static struct song song;
static struct song_step st;
static struct song_undo undo;
static struct song_player player;

static void silent(void *context, unsigned track, unsigned pitch, unsigned velocity)
{
    (void)context; (void)track; (void)pitch; (void)velocity;
}
static const struct song_output quiet = {.note = silent};

static unsigned count(unsigned track) { unsigned n; song_track(&song, track, &n); return n; }
static const struct song_event *at(unsigned track, unsigned i) { return &song.event[song.first[track] + i]; }
/* The velocity of the note of `pitch` at `clock` on the track, or -1. */
static int vel_at(unsigned track, uint32_t clock, unsigned pitch)
{
    for (unsigned i = 0; i < count(track); ++i)
        if (song_event_start(at(track, i)) == clock && at(track, i)->a == pitch) return at(track, i)->b;
    return -1;
}
static unsigned of_pitch(unsigned track, unsigned pitch)
{
    unsigned n = 0;
    for (unsigned i = 0; i < count(track); ++i) n += at(track, i)->a == pitch;
    return n;
}
static void fresh(unsigned bars, unsigned track)
{
    song_clear(&song, bars, 12000);
    song_undo_init(&undo);
    song.undo = &undo;
    step_init(&st, &song, &quiet);
    step_begin(&st, track, 0);
    st.length = STEP_16TH;
}
static void land(uint32_t pointer) { st.pointer = pointer; st.ranged = 0; step_forget(&st); step_land(&st); }
static uint32_t sweeps(int steps) { step_sweep(&st, steps, 0); return st.pointer; }
/* One poll with a MOVE or NUDGE turn, as the panel makes it. */
static int turn(int32_t clocks, unsigned more)
{
    song_undo_begin(&undo, &song);
    song_undo_run(&undo, more);
    int r = step_nudge(&st, clocks, more, NULL);
    song_undo_end(&undo, &song);
    return r;
}
/* One poll with any other edit. */
#define POLL(edit) do { song_undo_begin(&undo, &song); edit; song_undo_end(&undo, &song); } while (0)
static unsigned selected(unsigned track)
{
    unsigned n = 0;
    for (unsigned i = 0; i < count(track); ++i) n += step_chosen(&st, at(track, i));
    return n;
}

/* T8 sampler at 1/16 over a bar: a lower pad on steps 1, 3, 7 and 9, a
 * higher one on 5 and 11 to 13. Swept from 1:1:000 to 1:2:000, the sweep
 * holds the first three. MOVE right, a second time, was NOTE IN THE WAY in
 * 0.6.7, the band reaching step 7's note. Three clicks go now, and the same
 * three notes move, stay selected, and take nothing in. */
enum { LOW = 36, HIGH = 38 };
static void test_his_case(void)
{
    static const uint32_t low[4] = {0, 240, 720, 960}, high[4] = {480, 1200, 1320, 1440};
    static const uint32_t moved_low[4] = {360, 600, 720, 960}, moved_high[4] = {840, 1200, 1320, 1440};
    unsigned wrong = 0;
    fresh(4, 7);
    for (unsigned i = 0; i < 4; ++i) {
        song_add_note(&song, 7, low[i], LOW, 100, 60);
        song_add_note(&song, 7, high[i], HIGH, 100, 60);
    }
    CHECK_EQ(sweeps(4), 480);
    CHECK_EQ(step_count(&st), 3);
    for (unsigned k = 0; k < 3; ++k) {
        CHECK_EQ(turn(120, k > 0), 0);
        CHECK_EQ(step_count(&st), 3);
        CHECK_EQ(selected(7), 3);
    }
    CHECK_EQ(st.anchor, 360); CHECK_EQ(st.pointer, 840);
    CHECK_EQ(count(7), 8);                        /* nothing landed on */
    for (unsigned i = 0; i < 4; ++i) wrong += vel_at(7, moved_low[i], LOW) != 100 || vel_at(7, moved_high[i], HIGH) != 100;
    CHECK_EQ(wrong, 0);
    for (unsigned i = 0; i < count(7); ++i) {     /* exactly the three moved */
        uint32_t a = song_event_start(at(7, i));
        wrong += step_chosen(&st, at(7, i)) != (a == 360 || a == 600 || a == 840);
    }
    CHECK_EQ(wrong, 0);
    CHECK_EQ(song_undo_count(&undo, 0), 1);
    CHECK_EQ(song_undo(&undo, &song), 0);
    for (unsigned i = 0; i < 4; ++i) wrong += vel_at(7, low[i], LOW) != 100 || vel_at(7, high[i], HIGH) != 100;
    CHECK_EQ(wrong, 0);
}

/* Landing replaces the note of its pitch there, the next turn of the run
 * puts it back, and where the run stops it stays replaced. Undo brings back
 * the moved note and every note replaced in one step, and redo replaces
 * them again. */
static void test_overwrite(void)
{
    fresh(4, 0);
    song_add_note(&song, 0, 0, 60, 100, 100);     /* A, moved */
    song_add_note(&song, 0, 480, 60, 50, 50);     /* B */
    song_add_note(&song, 0, 960, 60, 40, 40);     /* C */
    song_add_note(&song, 0, 960, 64, 90, 90);     /* another pitch, left alone */
    land(0);
    CHECK_EQ(turn(480, 0), 0);                    /* onto B */
    CHECK_EQ(vel_at(0, 480, 60), 100); CHECK_EQ(of_pitch(0, 60), 2); CHECK_EQ(st.covers, 1);
    CHECK_EQ(turn(480, 1), 0);                    /* onto C, B back */
    CHECK_EQ(vel_at(0, 480, 60), 50); CHECK_EQ(vel_at(0, 960, 60), 100); CHECK_EQ(of_pitch(0, 60), 2);
    CHECK_EQ(vel_at(0, 960, 64), 90);
    CHECK_EQ(turn(480, 1), 0);                    /* past both: C back */
    CHECK_EQ(vel_at(0, 960, 60), 40); CHECK_EQ(vel_at(0, 1440, 60), 100); CHECK_EQ(of_pitch(0, 60), 3);
    CHECK_EQ(st.covers, 0);
    CHECK_EQ(turn(-480, 1), 0);                   /* back onto C, where the run stops */
    CHECK_EQ(vel_at(0, 960, 60), 100); CHECK_EQ(of_pitch(0, 60), 2);
    CHECK_EQ(song_undo_count(&undo, 0), 1);
    CHECK_EQ(undo.used, 2U + 3U * 12U + 2U);      /* A's first and last, and C taken: B's came and went */
    POLL(CHECK_EQ(step_set_velocity(&st, 1), 0)); /* another control: still A alone */
    CHECK_EQ(vel_at(0, 960, 60), 101); CHECK_EQ(st.ids, 1);
    CHECK_EQ(turn(480, 0), 0);                    /* a new run: C stays replaced */
    CHECK_EQ(vel_at(0, 960, 60), -1); CHECK_EQ(vel_at(0, 1440, 60), 101); CHECK_EQ(of_pitch(0, 60), 2);
    CHECK_EQ(song_undo_count(&undo, 0), 3);
    for (unsigned k = 0; k < 3; ++k) CHECK_EQ(song_undo(&undo, &song), 0);
    CHECK_EQ(vel_at(0, 0, 60), 100); CHECK_EQ(vel_at(0, 480, 60), 50); CHECK_EQ(vel_at(0, 960, 60), 40);
    CHECK_EQ(vel_at(0, 960, 64), 90); CHECK_EQ(count(0), 4);
    CHECK_EQ(song_redo(&undo, &song), 0);
    CHECK_EQ(vel_at(0, 0, 60), -1); CHECK_EQ(vel_at(0, 480, 60), 50); CHECK_EQ(vel_at(0, 960, 60), 100);
    CHECK_EQ(count(0), 3);
}

/* After a turn the moved notes are what K1 to K3, the glitch layer, TUNE
 * and the pitch knob change, and the pitch knob takes the selection along.
 * ENV deletes exactly them. Then each control that makes the selection
 * again ends it. */
static void kit(void)
{
    fresh(4, 7);
    for (unsigned i = 0; i < 16; ++i) song_add_note(&song, 7, i * 120U, 42, 70, 60);
    song_add_note(&song, 7, 0, 36, 100, 120);
    song_add_note(&song, 7, 480, 38, 100, 120);
}
static unsigned hats(unsigned pitch, unsigned velocity, unsigned length)
{
    unsigned n = 0;
    for (unsigned i = 0; i < count(7); ++i) n += at(7, i)->a == pitch && at(7, i)->b == velocity && at(7, i)->length == length;
    return n;
}
static void test_edits(void)
{
    static const uint8_t pads[4] = {36, 38, 42, 46};
    kit();
    sweeps(4);
    step_pick(&st, 42);
    CHECK_EQ(turn(60, 0), 0);                     /* hats to 60..540: the snare at 480 in the band */
    CHECK_EQ(step_count(&st), 5);
    POLL(CHECK_EQ(step_set_velocity(&st, 5), 0));
    CHECK_EQ(hats(42, 75, 60), 5); CHECK_EQ(hats(42, 70, 60), 11); CHECK_EQ(vel_at(7, 480, 38), 100);
    POLL(CHECK_EQ(step_set_length(&st, 1), 0));  /* 1/16 to 1/8: lengths double */
    CHECK_EQ(hats(42, 75, 120), 5); CHECK_EQ(hats(42, 70, 60), 11);
    POLL(CHECK_EQ(step_set_glitch(&st, GLITCH_TUNE, 2), 0));
    unsigned tuned = 0;
    for (unsigned i = 0; i < count(7); ++i) tuned += song_glitch_tune(song_glitch_word(&song, at(7, i))) == 2;
    CHECK_EQ(tuned, 5);
    CHECK_EQ(st.ids, 5);
    POLL(CHECK_EQ(step_transpose_pads(&st, pads, 4, 1), 5)); /* 42 to 46, the selection with them */
    CHECK_EQ(hats(46, 75, 120), 5); CHECK_EQ(step_count(&st), 5); CHECK_EQ(selected(7), 5);
    CHECK_EQ(turn(60, 0), 0);                     /* and they move on as 46s */
    CHECK_EQ(vel_at(7, 120, 46), 75); CHECK_EQ(vel_at(7, 600, 46), 75); CHECK_EQ(vel_at(7, 600, 42), 70);
    POLL(CHECK_EQ(step_delete(&st), 0));          /* ENV: exactly them */
    CHECK_EQ(of_pitch(7, 46), 0); CHECK_EQ(of_pitch(7, 42), 11); CHECK_EQ(of_pitch(7, 38), 1);
    CHECK_EQ(st.ids, 0);
    /* What ends it. Each starts from a turn at the pointer. */
    unsigned ended = 0, tries = 0;
    for (unsigned way = 0; way < 11; ++way) {
        kit();
        land(1200);
        CHECK_EQ(turn(STEP_NUDGE, 0), 0);
        CHECK_EQ(st.ids, 1);
        switch (way) {
        case 0: step_move(&st, 1); break;          /* PRESETS */
        case 1: step_jump(&st, 1); break;          /* SHIFT+PRESETS */
        case 2: step_sweep(&st, 1, 0); break;      /* SEL+PRESETS */
        case 3: step_pick(&st, 42); break;         /* SEL and a key */
        case 4: step_key(&st, 50); (void)step_commit(&st); break;
        case 5: (void)step_rest(&st); break;
        case 6: (void)step_tie(&st); break;
        case 7: (void)step_repeat(&st); break;
        case 8: (void)step_delete(&st); break;     /* ENV */
        case 9: (void)step_back_delete(&st); break; /* LFO */
        default: step_begin(&st, 6, st.pointer); break; /* another track */
        }
        ended += !st.ids && !st.covers;
        ++tries;
    }
    CHECK_EQ(ended, tries);
}

/* A sampler pad at two TUNEs at one clock moves as one place, both notes
 * selected, and a note of the pad it lands on goes whatever its TUNE. The
 * same note twice moves and lands the same way. */
static uint32_t tune(int semitones) { return song_glitch_turn(0, GLITCH_TUNE, semitones); }
static void note(uint32_t clock, unsigned pitch, unsigned velocity, uint32_t word)
{
    int g = word ? song_glitch_intern(&song, word) : 0;
    const struct song_event e = {clock | (uint32_t)(g > 0 ? g : 0) << SONG_GLITCH_SHIFT, (uint8_t)pitch, (uint8_t)velocity, 60};
    CHECK_EQ(song_add_event(&song, 4, &e), 0);
}
static int tunes_at(uint32_t clock, int a, int b)
{
    int seen = 0;
    for (unsigned i = 0; i < count(4); ++i)
        if (song_event_start(at(4, i)) == clock) {
            int t = song_glitch_tune(song_glitch_word(&song, at(4, i)));
            seen |= t == a ? 1 : t == b ? 2 : 4;
        }
    return seen == 3;
}
static void test_chromatic(void)
{
    fresh(4, 4);
    note(0, 36, 100, tune(2));
    note(0, 36, 100, tune(-3));
    note(240, 36, 90, tune(5));
    note(960, 40, 80, 0); note(960, 40, 80, 0);   /* the same note twice */
    note(1200, 40, 80, 0);
    land(0);
    CHECK_EQ(step_count(&st), 2);
    CHECK_EQ(turn(120, 0), 0);
    CHECK_EQ(st.ids, 2); CHECK_EQ(step_count(&st), 2); CHECK(tunes_at(120, 2, -3));
    CHECK_EQ(turn(120, 1), 0);                    /* onto the pad at TUNE +5: replaced */
    CHECK(tunes_at(240, 2, -3)); CHECK_EQ(count(4), 5); CHECK_EQ(st.covers, 1);
    CHECK_EQ(turn(120, 1), 0);                    /* and back */
    CHECK(tunes_at(360, 2, -3)); CHECK_EQ(vel_at(4, 240, 36), 90); CHECK_EQ(count(4), 6);
    land(960);
    CHECK_EQ(step_count(&st), 2);
    CHECK_EQ(turn(240, 0), 0);                    /* the pair onto the note at 1200 */
    CHECK_EQ(st.ids, 2); CHECK_EQ(step_count(&st), 2); CHECK_EQ(of_pitch(4, 40), 2); CHECK_EQ(st.covers, 1);
    CHECK_EQ(turn(120, 1), 0);
    CHECK_EQ(of_pitch(4, 40), 3); CHECK_EQ(vel_at(4, 1200, 40), 80); CHECK_EQ(step_count(&st), 2);
    while (!song_undo(&undo, &song)) {}
    CHECK(tunes_at(0, 2, -3)); CHECK_EQ(vel_at(4, 240, 36), 90); CHECK_EQ(of_pitch(4, 40), 3); CHECK_EQ(count(4), 6);
}

/* More notes landed on than a run keeps: the first STEP_COVERS go back as
 * the run moves on, the rest stay replaced, and undo brings every one back. */
static void test_capacity(void)
{
    unsigned n = STEP_COVERS + 6, back = 0;
    fresh(8, 0);
    for (unsigned i = 0; i < n; ++i) song_add_note(&song, 0, i * 30U, 60, 100, 20);
    for (unsigned i = 0; i < n; ++i) song_add_note(&song, 0, 2400U + i * 30U, 60, 50, 20);
    st.length = STEP_64TH;
    sweeps((int)n - 1);
    CHECK_EQ(step_count(&st), n);
    CHECK_EQ(turn(2400, 0), 0);
    CHECK_EQ(st.covers, STEP_COVERS); CHECK_EQ(count(0), n);
    CHECK_EQ(turn(-2400, 1), 0);
    CHECK_EQ(st.covers, 0); CHECK_EQ(count(0), n + STEP_COVERS);
    CHECK_EQ(song_undo_count(&undo, 0), 1);
    CHECK_EQ(song_undo(&undo, &song), 0);
    for (unsigned i = 0; i < count(0); ++i) back += at(0, i)->b == 50;
    CHECK_EQ(back, n); CHECK_EQ(count(0), 2 * n);
}

/* A long run back and forth over notes of its pitch: each landed on and
 * put back folds out of the undo step, which stays the size it was. */
static void test_bounds(void)
{
    uint32_t most = 0;
    unsigned bad = 0;
    fresh(8, 0);
    song_add_note(&song, 0, 0, 60, 100, 50);
    for (unsigned i = 1; i <= 16; ++i) song_add_note(&song, 0, i * 120U, 60, 40 + i, 50);
    land(0);
    CHECK_EQ(turn(120, 0), 0);
    for (unsigned k = 0; k < 600; ++k) {
        bad += turn((k / 15U) % 2U ? -120 : 120, 1) != 0;
        if (undo.used > most) most = undo.used;
    }
    CHECK_EQ(bad, 0);
    CHECK_EQ(most, 2U + 3U * 12U + 2U);          /* the moved note's first and last, and the note it is on */
    CHECK_EQ(song_undo_count(&undo, 0), 1);
    CHECK_EQ(song_undo(&undo, &song), 0);
    CHECK_EQ(count(0), 17); CHECK_EQ(vel_at(0, 0, 60), 100);
    for (unsigned i = 1; i <= 16; ++i) bad += vel_at(0, i * 120U, 60) != (int)(40 + i);
    CHECK_EQ(bad, 0);
}

/* Playing: a note landed on ahead of the playhead stays quiet while it is
 * covered and sounds where it is when put back ahead of it. One landed on
 * as it sounds ends when it was due, and put back behind the playhead it
 * sounds from the next pass. Nothing hangs or sounds twice. */
static unsigned ons_in(unsigned pitch, unsigned from, unsigned to, uint32_t *blocks)
{
    unsigned n = 0;
    for (unsigned i = from; i < to; ++i)
        if (heard[i].velocity && heard[i].pitch == pitch) { if (n < 2) blocks[n] = heard[i].block; ++n; }
    return n;
}
static void test_playing(void)
{
    uint32_t b = 0, got[2] = {0, 0};
    fresh(4, 0);
    CHECK_EQ(song_set_loop(&song, 0, 1920), 0);
    song.loop_on = 1;
    song_add_note(&song, 0, 0, 60, 100, 100);     /* A, moved on past the playhead */
    song_add_note(&song, 0, 960, 60, 50, 100);    /* B, landed on and put back ahead */
    song_add_note(&song, 0, 120, 62, 100, 1400);  /* D, landed on as it sounds */
    song_add_note(&song, 0, 960, 62, 60, 100);    /* E, moved back onto D */
    song_player_init(&player, &song, &listener);
    reset_capture();
    song_player_start(&player, 0);
    while (player.next < 600) run_blocks(&player, b++, 1);
    land(0); step_pick(&st, 60);
    CHECK_EQ(step_nudge(&st, 960, 0, &player), 0); /* A onto B, ahead */
    CHECK_EQ(step_nudge(&st, 480, 1, &player), 0); /* on to 1440: B back at 960 */
    land(960); step_pick(&st, 62);
    CHECK_EQ(step_nudge(&st, -840, 0, &player), 0); /* E onto D, sounding */
    CHECK_EQ(step_nudge(&st, 240, 1, &player), 0);  /* on to 360: D back at 120, behind */
    uint32_t pass = player.pass;
    while (player.pass == pass) run_blocks(&player, b++, 1);
    unsigned first = heard_count;
    CHECK_EQ(ons_in(60, 0, first, got), 2);
    CHECK_EQ(got[0], exact_block(&song, 0, 0, 0)); CHECK_EQ(got[1], exact_block(&song, 0, 0, 960));
    CHECK_EQ(ons_in(62, 0, first, got), 1);
    CHECK_EQ(got[0], exact_block(&song, 0, 0, 120));
    pass = player.pass;
    while (player.pass == pass) run_blocks(&player, b++, 1);
    CHECK_EQ(ons_in(60, first, heard_count, got), 2);
    CHECK_EQ(got[0], exact_block(&song, 0, 0, 1920 + 960)); CHECK_EQ(got[1], exact_block(&song, 0, 0, 1920 + 1440));
    CHECK_EQ(ons_in(62, first, heard_count, got), 2);
    CHECK_EQ(got[0], exact_block(&song, 0, 0, 1920 + 120)); CHECK_EQ(got[1], exact_block(&song, 0, 0, 1920 + 360));
    song_player_stop(&player);
    CHECK(balanced());
}

int main(void)
{
    test_his_case();
    test_overwrite();
    test_edits();
    test_chromatic();
    test_capacity();
    test_bounds();
    test_playing();
    return report("test_step_ids");
}
