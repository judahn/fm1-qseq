/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Step entry's swept selection (0.5.0): K4 with LFO held sweeps from an
 * anchor, every note starting from it to the pointer is selected, keys
 * toggle pitches in the sweep, and SEL, velocity, length, gate, glitch and
 * the pitch knob act on all of it, each one undo step, or refuse when undo
 * couldn't hold it. The selection goes when the pointer moves without LFO,
 * a note goes in, or step entry starts again. */
#include "harness.h"
#include "step.h"
#include "undo.h"
#include "glitch.h"

static struct song song;
static struct song_step st;
static struct song_undo undo;

static unsigned count(unsigned track) { unsigned n; song_track(&song, track, &n); return n; }
static const struct song_event *at(unsigned track, unsigned i) { return &song.event[song.first[track] + i]; }
/* Two bars of 16ths on track 8: a hi-hat (42) on every 16th, accented on
 * the beat, a kick (36) on beats 1 and 3, a snare (38) on 2 and 4, and a
 * kick in bar 3, outside the sweep. */
static void drums(void)
{
    song_clear(&song, 8, 12000);
    song_undo_init(&undo);
    song.undo = &undo;
    for (unsigned i = 0; i < 32; ++i) song_add_note(&song, 7, i * 120U, 42, i % 4 ? 70 : 120, 60);
    for (unsigned b = 0; b < 8; ++b) song_add_note(&song, 7, b * 480U, b % 2 ? 38 : 36, 100, 120);
    song_add_note(&song, 7, 3840, 36, 100, 120);
    step_init(&st, &song, &listener);
    step_begin(&st, 7, 0);
    st.length = STEP_16TH; /* the entry's, leaving the notes at the pointer alone */
}
static unsigned with(unsigned pitch, unsigned *velocity_sum)
{
    unsigned n = 0;
    if (velocity_sum) *velocity_sum = 0;
    for (unsigned i = 0; i < count(7); ++i)
        if (at(7, i)->a == pitch && song_event_start(at(7, i)) < 3840) { ++n; if (velocity_sum) *velocity_sum += at(7, i)->b; }
    return n;
}

static void test_sweep(void)
{
    uint32_t from, to;
    drums();
    CHECK(!step_range(&st, &from, &to));
    step_sweep(&st, 15, 0);                       /* a bar of 16ths, the pointer on the last */
    CHECK(step_range(&st, &from, &to));
    CHECK_EQ(from, 0); CHECK_EQ(to, 1801);
    CHECK_EQ(st.pointer, 1800);
    CHECK_EQ(step_count(&st), 16 + 4);
    step_sweep(&st, 1, 0);                        /* on the next bar line: its notes are in */
    CHECK_EQ(step_count(&st), 16 + 4 + 2);
    step_sweep(&st, 15, 0);
    CHECK_EQ(step_count(&st), 32 + 8);
    /* A key picks its pitch alone, another adds its own, and again takes it out. */
    step_pick(&st, 42);
    CHECK_EQ(step_count(&st), 32);
    step_pick(&st, 38);
    CHECK_EQ(step_count(&st), 36);
    step_pick(&st, 38);
    CHECK_EQ(step_count(&st), 32);
    /* Backwards from the anchor, both ends in. */
    step_begin(&st, 7, 1920);
    step_sweep(&st, -4, 0);
    CHECK(step_range(&st, &from, &to));
    CHECK_EQ(from, 1440); CHECK_EQ(to, 1921);
    CHECK_EQ(step_count(&st), 5 + 2);
    /* Note to note with SHIFT, and back to the anchor: no sweep. */
    step_sweep(&st, 1, 1);
    CHECK_EQ(st.pointer, 1560);
    step_sweep(&st, 3, 0);
    CHECK_EQ(st.pointer, 1920);
    CHECK(!step_range(&st, &from, &to));
    CHECK_EQ(step_count(&st), 2);                 /* the pointer's notes again */
    /* Moving without LFO, entering a note, or starting again ends it. */
    step_sweep(&st, 2, 0);
    CHECK(step_range(&st, &from, &to));
    step_move(&st, 1);
    CHECK(!step_range(&st, &from, &to));
    step_sweep(&st, 2, 0);
    step_jump(&st, 1);
    CHECK(!step_range(&st, &from, &to));
    step_sweep(&st, 2, 0);
    step_key(&st, 50); step_commit(&st);
    CHECK(!step_range(&st, &from, &to));
    step_sweep(&st, 2, 0);
    step_begin(&st, 7, 0);
    CHECK(!step_range(&st, &from, &to));
}

/* Velocity changes every selected note by the same amount, so accents
 * survive, and the knob shows the first. Length and gate scale each note
 * as the entry's length scales. Each poll's change is one undo step. */
static void test_edits(void)
{
    unsigned sum, before;
    drums();
    step_sweep(&st, 31, 0);
    step_pick(&st, 42);
    with(42, &before);
    song_undo_begin(&undo, &song);
    CHECK_EQ(step_set_velocity(&st, 10), 0);
    song_undo_end(&undo, &song);
    with(42, &sum);
    CHECK_EQ(sum, before + 32 * 10 - 8 * 3);      /* the accents stop at 127 */
    CHECK_EQ(at(7, 0)->b, 127);                   /* an accent, the first selected */
    CHECK_EQ(st.velocity, 127);
    CHECK_EQ(with(36, &sum), 4); CHECK_EQ(sum, 400); /* kicks untouched */
    song_undo(&undo, &song);
    with(42, &sum);
    CHECK_EQ(sum, before);
    /* 1/16 to 1/8: every hi-hat 60 clocks long becomes 120, and gate 90 to
     * 100% (216 to 240 clocks) makes it 133. Each is one undo step. */
    song_undo_begin(&undo, &song);
    CHECK_EQ(step_set_length(&st, 1), 0);
    song_undo_end(&undo, &song);
    CHECK_EQ(st.length, STEP_8TH);
    for (unsigned i = 0; i < count(7); ++i)
        if (at(7, i)->a == 42) CHECK_EQ(at(7, i)->length, 120);
    song_undo_begin(&undo, &song);
    CHECK_EQ(step_set_gate(&st, 2), 0);
    song_undo_end(&undo, &song);
    for (unsigned i = 0; i < count(7); ++i) CHECK_EQ(at(7, i)->length, at(7, i)->a == 42 ? 133 : 120);
    song_undo(&undo, &song);
    for (unsigned i = 0; i < count(7); ++i) CHECK_EQ(at(7, i)->length, 120);
    song_undo(&undo, &song);
    for (unsigned i = 0; i < count(7); ++i) CHECK_EQ(at(7, i)->length, at(7, i)->a == 42 ? 60 : 120);
    /* Glitch on every selected note. */
    drums();
    step_sweep(&st, 7, 0);
    CHECK_EQ(step_set_glitch(&st, GLITCH_REPEATS, 2), 0);
    unsigned glitched = 0;
    for (unsigned i = 0; i < count(7); ++i) glitched += song_event_glitch(at(7, i)) != 0;
    CHECK_EQ(glitched, 8 + 2);
    CHECK_EQ(song_glitch_repeats(st.glitch), song_glitch_repeats(song_glitch_word(&song, at(7, 0))));
}

/* SEL deletes the selection as one undo step. The pitch knob moves picked
 * pads along the kit, the picks following, and a pad already taken at a
 * clock stops a note there. */
static void test_delete_and_pitch(void)
{
    drums();
    step_sweep(&st, 31, 0);
    step_pick(&st, 38);
    song_undo_begin(&undo, &song);
    CHECK_EQ(step_delete(&st), 0);
    song_undo_end(&undo, &song);
    CHECK_EQ(with(38, NULL), 0);
    CHECK_EQ(with(42, NULL), 32);
    CHECK_EQ(count(7), 32 + 4 + 1);
    song_undo(&undo, &song);
    CHECK_EQ(with(38, NULL), 4);
    CHECK_EQ(count(7), 41);
    static const uint8_t pads[4] = {36, 38, 42, 46};
    step_pick(&st, 38);                           /* the snares out: none picked */
    CHECK_EQ(step_count(&st), 0);
    step_pick(&st, 42);                           /* the hats in */
    step_pick(&st, 36);                           /* and the kicks */
    CHECK_EQ(step_count(&st), 36);
    step_pick(&st, 38);                           /* every pad */
    CHECK_EQ(step_count(&st), 40);
    step_pick(&st, 38); step_pick(&st, 36);       /* the hats alone again */
    CHECK_EQ(step_count(&st), 32);
    CHECK_EQ(step_transpose_pads(&st, pads, 4, 1), 32);
    CHECK_EQ(with(46, NULL), 32);
    CHECK_EQ(with(42, NULL), 0);
    CHECK_EQ(step_count(&st), 32);                /* the pick moved with them */
    CHECK_EQ(step_transpose_pads(&st, pads, 4, -2), 28); /* 46 to 38, but not where a snare is */
    CHECK_EQ(with(38, NULL), 4 + 28);
    CHECK_EQ(with(46, NULL), 4);
    CHECK_EQ(step_count(&st), 36);                /* both pitches picked now, the snares' too */
}

/* Too big for undo: refused whole, and nothing changes, the entry
 * settings included. One less note fits. */
static void test_too_big(void)
{
    unsigned room = (unsigned)((SONG_UNDO_BYTES - 512U - sizeof(struct song_maps)) / 24U);
    song_clear(&song, 64, 12000);
    song_undo_init(&undo);
    song.undo = &undo;
    for (unsigned i = 0; i <= room; ++i) song_add_note(&song, 0, i * 30U, 60 + i % 2, 100, 30);
    step_init(&st, &song, &listener);
    step_begin(&st, 0, 0);
    step_pick_length(&st, STEP_64TH);
    step_sweep(&st, (int)room, 0);
    CHECK_EQ(step_count(&st), room + 1);
    CHECK(step_too_big(&st));
    unsigned length = st.length, velocity = st.velocity;
    CHECK_EQ(step_delete(&st), STEP_TOO_BIG);
    CHECK_EQ(step_set_velocity(&st, 5), STEP_TOO_BIG);
    CHECK_EQ(step_set_length(&st, 2), STEP_TOO_BIG);
    CHECK_EQ(step_set_gate(&st, 1), STEP_TOO_BIG);
    CHECK_EQ(step_pick_length(&st, STEP_QUARTER), STEP_TOO_BIG);
    CHECK_EQ(step_set_glitch(&st, GLITCH_REPEATS, 1), STEP_TOO_BIG);
    CHECK_EQ(step_transpose(&st, 1, 0), STEP_TOO_BIG);
    CHECK_EQ(count(0), room + 1);
    CHECK_EQ(st.length, length); CHECK_EQ(st.velocity, velocity);
    CHECK_EQ(at(0, 0)->b, 100); CHECK_EQ(at(0, 0)->a, 60);
    step_sweep(&st, -1, 0);
    CHECK_EQ(step_count(&st), room);
    CHECK(!step_too_big(&st));
    song_undo_begin(&undo, &song);
    CHECK_EQ(step_set_velocity(&st, 5), 0);
    CHECK_EQ(song_undo_end(&undo, &song), 0);    /* undo kept it */
    CHECK_EQ(at(0, 0)->b, 105);
    CHECK_EQ(song_undo(&undo, &song), 0);
    CHECK_EQ(at(0, 0)->b, 100);
    CHECK_EQ(at(0, (unsigned)room - 1)->b, 100);
}

int main(void)
{
    test_sweep();
    test_edits();
    test_delete_and_pitch();
    test_too_big();
    return report("test_step_range");
}
