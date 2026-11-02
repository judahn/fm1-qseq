/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Step entry's MOVE and NUDGE, and PRESETS on the step grid (0.6.7). The
 * grid: off it, the first step lands on the nearest line that way, the lines
 * start again at every bar line, in 7/8 and for triplets and dots too, a
 * sweep snaps the same, note to note and entering don't, and a new length
 * leaves the pointer where it is. Moving: the notes at the pointer or in a
 * sweep, whole, the pointer and the anchor going with them, the selection
 * narrowed to their pitches, each refusal leaving everything as it was, a
 * run of turns as one undo step that undoes and redoes whole, and while
 * playing a note moved past the playhead sounding once a pass, at its new
 * place from the next, with nothing left hanging. */
#include "harness.h"
#include "step.h"
#include "undo.h"
#include "glitch.h"
#include "clips.h"
#include <stdlib.h>

static struct song song;
static struct song_step st;
static struct song_undo undo;
static struct song_player player;

static void silent(void *context, unsigned track, unsigned pitch, unsigned velocity)
{
    (void)context; (void)track; (void)pitch; (void)velocity;
}
static const struct song_output quiet = {.note = silent}; /* step entry's previews, kept out of what the player is heard to send */

static unsigned count(unsigned track) { unsigned n; song_track(&song, track, &n); return n; }
static const struct song_event *at(unsigned track, unsigned i) { return &song.event[song.first[track] + i]; }
/* The first note of `pitch` on the track starting at or after `from`. */
static const struct song_event *note_of(unsigned track, unsigned pitch, uint32_t from)
{
    for (unsigned i = 0; i < count(track); ++i)
        if (song_is_note(at(track, i)) && at(track, i)->a == pitch && song_event_start(at(track, i)) >= from) return at(track, i);
    return NULL;
}
static uint32_t where(unsigned track, unsigned pitch, uint32_t from)
{
    const struct song_event *e = note_of(track, pitch, from);
    return e ? song_event_start(e) : ~0U;
}
static void fresh(unsigned bars)
{
    song_clear(&song, bars, 12000);
    song_undo_init(&undo);
    song.undo = &undo;
    step_init(&st, &song, &quiet);
    step_begin(&st, 0, 0);
}
/* The pointer put on a clock, as PRESETS would: a moved selection ends. */
static void land(uint32_t pointer) { st.pointer = pointer; st.ranged = 0; step_forget(&st); step_land(&st); }
static uint32_t moves(int steps) { step_move(&st, steps); return st.pointer; }
static uint32_t sweeps(int steps) { step_sweep(&st, steps, 0); return st.pointer; }
static int picked(unsigned pitch) { return st.picked && (st.pick[pitch / 32U] >> (pitch % 32U) & 1U); }
/* A turn with no undo, the run going on. */
static int nudge(int32_t clocks) { return step_nudge(&st, clocks, 1, NULL); }
/* One poll with a MOVE or NUDGE turn, as the panel makes it: its own
 * transaction, going on with the run when `more`. */
static int turn(int32_t clocks, unsigned more)
{
    song_undo_begin(&undo, &song);
    song_undo_run(&undo, more);
    int r = step_nudge(&st, clocks, more, NULL);
    song_undo_end(&undo, &song);
    return r;
}

static void test_grid(void)
{
    fresh(8);
    step_pick_length(&st, STEP_16TH);
    CHECK_EQ(moves(1), 120);                      /* on the grid in 4/4, as before */
    CHECK_EQ(moves(3), 480);
    CHECK_EQ(moves(-2), 240);
    st.pointer = 50;                              /* off it: the nearest line that way */
    CHECK_EQ(moves(1), 120);
    st.pointer = 50;
    CHECK_EQ(moves(-1), 0);
    CHECK_EQ(moves(-1), 0);
    st.pointer = 130;
    CHECK_EQ(moves(-1), 120);
    st.pointer = 130;
    CHECK_EQ(moves(2), 360);
    /* A walk at 1/64, then 1/2: the new length leaves the pointer, and the
     * next step lands on the half bar, the one back on the bar line. */
    st.pointer = 1920;
    step_pick_length(&st, STEP_64TH);
    CHECK_EQ(moves(3), 1920 + 90);
    step_pick_length(&st, STEP_HALF);
    CHECK_EQ(st.pointer, 1920 + 90);
    CHECK_EQ(step_offset(&st), 90);
    CHECK_EQ(moves(1), 1920 + 960);
    CHECK_EQ(step_offset(&st), 0);
    CHECK_EQ(moves(-1), 1920);
    st.pointer = 1920 + 90;
    CHECK_EQ(moves(-1), 1920);
    st.pointer = 1920 + 900;
    CHECK_EQ(step_offset(&st), -60);
    step_set_length(&st, -1);                     /* K1, triplet and dots leave it too */
    step_toggle_triplet(&st);
    step_cycle_dots(&st);
    CHECK_EQ(st.pointer, 1920 + 900);
    /* Dotted quarters start again at each bar line. */
    st.length = STEP_QUARTER; st.triplet = 0; st.dots = 1;
    st.pointer = 0;
    CHECK_EQ(moves(1), 720); CHECK_EQ(moves(1), 1440); CHECK_EQ(moves(1), 1920); CHECK_EQ(moves(1), 2640);
    CHECK_EQ(moves(-1), 1920); CHECK_EQ(moves(-1), 1440);
    st.dots = 0;
    /* The end: no further. */
    st.pointer = song_end(&song) - 30;
    CHECK_EQ(moves(1), song_end(&song));
    CHECK_EQ(moves(1), song_end(&song));
    CHECK_EQ(moves(-1), song_end(&song) - 480);
    /* 7/8, a bar of 1680: 1/2 stops on the bar line and 960 into the bar,
     * 1/1 on bar lines only, and 1/8 triplets (160) start again at the bar. */
    song_set_meter(&song, 0, 7, 8);
    step_pick_length(&st, STEP_HALF);
    st.pointer = 0;
    CHECK_EQ(moves(1), 960); CHECK_EQ(moves(1), 1680); CHECK_EQ(moves(1), 2640); CHECK_EQ(moves(1), 3360);
    CHECK_EQ(moves(-1), 2640); CHECK_EQ(moves(-1), 1680); CHECK_EQ(moves(-1), 960); CHECK_EQ(moves(-1), 0);
    st.pointer = 1500;
    CHECK_EQ(step_offset(&st), -180);
    step_pick_length(&st, STEP_WHOLE);
    st.pointer = 0;
    CHECK_EQ(moves(1), 1680); CHECK_EQ(moves(1), 3360); CHECK_EQ(moves(-1), 1680);
    st.pointer = 1000;
    CHECK_EQ(moves(-1), 0);
    st.length = STEP_8TH; st.triplet = 1;
    st.pointer = 1600;
    CHECK_EQ(moves(1), 1680); CHECK_EQ(moves(1), 1840); CHECK_EQ(moves(-1), 1680); CHECK_EQ(moves(-1), 1600);
    st.triplet = 0;
    song_set_meter(&song, 0, 4, 4);
    /* A sweep snaps the same, from where it began. */
    step_pick_length(&st, STEP_16TH);
    st.pointer = 50;
    CHECK_EQ(sweeps(2), 240);
    CHECK_EQ(st.anchor, 50);
    /* Note to note and entering go from wherever the pointer is. */
    song_add_note(&song, 0, 75, 60, 100, 30);
    st.pointer = 50;
    step_jump(&st, 1);
    CHECK_EQ(st.pointer, 75);
    st.length = STEP_16TH; st.triplet = st.dots = 0; /* landing loaded the note's 1/64 */
    st.pointer = 2050;
    step_key(&st, 62);
    CHECK_EQ(step_commit(&st), 0);
    CHECK_EQ(where(0, 62, 0), 2050);
    CHECK_EQ(st.pointer, 2170);
    step_rest(&st);
    CHECK_EQ(st.pointer, 2290);
    CHECK_EQ(step_repeat(&st), 0);
    CHECK_EQ(where(0, 62, 2051), 2290);
    CHECK_EQ(st.pointer, 2410);
}

/* At the pointer: picked or not, the notes move whole and the pointer
 * with them, and they are the selection after, by identity (0.6.8). */
static void test_pointer(void)
{
    uint32_t word = song_glitch_turn(song_glitch_turn(0, GLITCH_TUNE, 3), GLITCH_REPEATS, 2);
    fresh(4);
    step_pick_length(&st, STEP_16TH);
    song_add_note(&song, 0, 480, 60, 90, 100);
    song_add_note(&song, 0, 480, 64, 80, 110);
    song_add_note(&song, 0, 480, 67, 70, 120);
    song_add_note(&song, 0, 960, 60, 100, 100);
    song_add_note(&song, 0, 965, 62, 100, 100);
    land(480);
    step_pick(&st, 64);                           /* E alone */
    CHECK_EQ(nudge(STEP_NUDGE), 0);
    CHECK_EQ(where(0, 64, 0), 485);
    CHECK_EQ(where(0, 60, 0), 480); CHECK_EQ(where(0, 67, 0), 480);
    CHECK_EQ(st.pointer, 485);
    CHECK_EQ(step_count(&st), 1);
    CHECK_EQ(nudge(STEP_NUDGE), 0);
    CHECK_EQ(nudge((int32_t)step_clocks(&st)), 0); /* MOVE: a 16th */
    CHECK_EQ(where(0, 64, 0), 610);
    CHECK_EQ(st.pointer, 610);
    CHECK(picked(64) && !picked(60) && !picked(67));
    CHECK_EQ(note_of(0, 64, 0)->b, 80); CHECK_EQ(note_of(0, 64, 0)->length, 110);
    /* No pick: the two left at 480 go, and stay the selection with no pick
     * made, as 0.6.7 did. */
    land(480);
    CHECK_EQ(step_count(&st), 2);
    CHECK_EQ(nudge(-STEP_NUDGE), 0);
    CHECK_EQ(where(0, 60, 0), 475); CHECK_EQ(where(0, 67, 0), 475);
    CHECK_EQ(st.pointer, 475);
    CHECK(!st.picked);
    CHECK_EQ(st.ids, 2); CHECK_EQ(step_count(&st), 2);
    /* Onto the C at 960: it is replaced, and back when they move on. */
    CHECK_EQ(nudge(485), 0);
    CHECK_EQ(where(0, 60, 0), 960); CHECK_EQ(where(0, 60, 961), ~0U); CHECK_EQ(where(0, 67, 0), 960);
    CHECK_EQ(st.covers, 1); CHECK_EQ(step_count(&st), 2);
    CHECK_EQ(nudge(STEP_NUDGE), 0);
    CHECK_EQ(where(0, 60, 0), 960); CHECK_EQ(where(0, 60, 961), 965); CHECK_EQ(st.covers, 0);
    CHECK_EQ(nudge(-490), 0);
    CHECK_EQ(where(0, 60, 0), 475); CHECK_EQ(where(0, 67, 0), 475); CHECK_EQ(where(0, 60, 476), 960);
    uint32_t revision;
    /* Another pitch there doesn't join: E onto the D at 965, and on alone. */
    land(610);
    CHECK_EQ(nudge(355), 0);
    CHECK_EQ(where(0, 64, 0), 965);
    CHECK_EQ(step_count(&st), 1);
    CHECK_EQ(nudge(STEP_NUDGE), 0);
    CHECK_EQ(where(0, 62, 0), 965); CHECK_EQ(where(0, 64, 0), 970);
    /* Picks choose what the first turn takes and stay as they were. A
     * picked pitch with no note here takes none in where the notes go. */
    land(475);
    step_pick(&st, 60); step_pick(&st, 62);
    CHECK_EQ(step_count(&st), 1);
    CHECK_EQ(nudge(490), 0);                      /* onto the D at 965 */
    CHECK_EQ(step_count(&st), 1);
    CHECK(picked(60) && picked(62) && !picked(67));
    CHECK_EQ(where(0, 60, 0), 960); CHECK_EQ(where(0, 60, 961), 965); CHECK_EQ(where(0, 62, 0), 965);
    CHECK_EQ(where(0, 67, 0), 475);
    /* An empty spot: nothing. */
    land(1500);
    revision = song.revision;
    CHECK_EQ(nudge(STEP_NUDGE), -1);
    CHECK_EQ(st.pointer, 1500); CHECK_EQ(song.revision, revision); CHECK(!st.picked);
    /* Not before clock 0. */
    song_add_note(&song, 0, 20, 70, 100, 10);
    land(20);
    CHECK_EQ(nudge(-25), STEP_START);
    CHECK_EQ(where(0, 70, 0), 20); CHECK_EQ(st.pointer, 20); CHECK(!st.picked);
    CHECK_EQ(nudge(-20), 0);
    CHECK_EQ(where(0, 70, 0), 0);
    /* The glitch word goes with the note, TUNE and all. */
    int g = song_glitch_intern(&song, word);
    const struct song_event tuned = {2000 | (uint32_t)g << SONG_GLITCH_SHIFT, 72, 33, 77};
    CHECK(g > 0 && !song_add_event(&song, 0, &tuned));
    land(2000);
    CHECK_EQ(nudge(120), 0);
    const struct song_event *e = note_of(0, 72, 0);
    CHECK_EQ(song_event_start(e), 2120); CHECK_EQ(e->b, 33); CHECK_EQ(e->length, 77);
    CHECK_EQ(song_glitch_word(&song, e), word);
    CHECK_EQ(song_glitch_tune(song_glitch_word(&song, e)), 3);
}

/* The song's end: as far as entering a note could go, the song growing as
 * it would for one, and at the most bars the end itself. A clip: inside
 * its length. */
static void test_edges(void)
{
    fresh(4);
    song_add_note(&song, 0, 7200, 60, 100, 100);
    land(7200);
    CHECK_EQ(nudge(480), 0);      /* onto the end: a bar more */
    CHECK_EQ(where(0, 60, 0), 7680); CHECK_EQ(song.bars, 5); CHECK_EQ(st.pointer, 7680);
    CHECK_EQ(nudge(1920), 0);
    CHECK_EQ(where(0, 60, 0), 9600); CHECK_EQ(song.bars, 6);
    CHECK_EQ(nudge(1925), STEP_END);
    CHECK_EQ(where(0, 60, 0), 9600); CHECK_EQ(song.bars, 6); CHECK_EQ(st.pointer, 9600);
    fresh(SONG_MAX_BARS);
    uint32_t end = song_end(&song);
    song_add_note(&song, 0, end - 5, 60, 100, 100);
    land(end - 5);
    CHECK_EQ(nudge(5), 0);
    CHECK_EQ(where(0, 60, 0), end); CHECK_EQ(song.bars, SONG_MAX_BARS); CHECK_EQ(st.pointer, end);
    CHECK_EQ(nudge(5), STEP_END);
    CHECK_EQ(where(0, 60, 0), end);
    /* A clip a bar long, in the editor. */
    fresh(4);
    CHECK_EQ(song_clip_open(&song, song_cell(0, 0)), 0);
    step_begin(&st, 0, 0);
    song_add_note(&song, 0, 1910, 60, 100, 10);
    song_add_note(&song, 0, 0, 62, 100, 10);
    land(1910);
    CHECK_EQ(nudge(5), 0);
    CHECK_EQ(where(0, 60, 0), 1915);
    CHECK_EQ(nudge(5), STEP_END);
    CHECK_EQ(where(0, 60, 0), 1915); CHECK_EQ(st.pointer, 1915);
    land(0);
    CHECK_EQ(nudge(-5), STEP_START);
    CHECK_EQ(nudge(5), 0);
    CHECK_EQ(where(0, 62, 0), 5);
    st.length = STEP_HALF; st.triplet = st.dots = 0; /* its bars count from its start */
    st.pointer = 90;
    CHECK_EQ(moves(1), 960);
    song_clip_close(&song);
    CHECK_EQ(song.count[song_cell_part(song_cell(0, 0))], 2);
}

/* Two bars of 16th hats on track 8, kicks on 1 and 3, snares on 2 and 4. */
static void kit(void)
{
    fresh(8);
    for (unsigned i = 0; i < 32; ++i) song_add_note(&song, 7, i * 120U, 42, i % 4 ? 70 : 120, 60);
    for (unsigned b = 0; b < 8; ++b) song_add_note(&song, 7, b * 480U, b % 2 ? 38 : 36, 100, 120);
    step_begin(&st, 7, 0);
    st.length = STEP_16TH;
}
/* A sweep: the band goes with its notes, and other notes in it stay out of
 * the selection, those of its pitches it comes to too (0.6.8). */
static void test_sweep(void)
{
    uint32_t from, to;
    kit();
    CHECK_EQ(sweeps(4), 480);
    step_pick(&st, 42);
    CHECK_EQ(step_count(&st), 5);
    CHECK_EQ(nudge(STEP_NUDGE), 0);
    CHECK_EQ(st.anchor, 5); CHECK_EQ(st.pointer, 485);
    CHECK(step_range(&st, &from, &to)); CHECK_EQ(from, 5); CHECK_EQ(to, 486);
    CHECK_EQ(step_count(&st), 5);
    CHECK_EQ(where(7, 42, 0), 5); CHECK_EQ(where(7, 42, 6), 125); CHECK_EQ(where(7, 42, 486), 600);
    CHECK_EQ(where(7, 36, 0), 0); CHECK_EQ(where(7, 38, 0), 480);
    /* A 16th more brings the hat at 600 into the band, not the selection. */
    CHECK_EQ(nudge(120), 0);
    CHECK_EQ(st.pointer, 605); CHECK_EQ(step_count(&st), 5);
    CHECK_EQ(where(7, 42, 486), 600); CHECK_EQ(where(7, 42, 601), 605);
    CHECK_EQ(nudge(-120), 0);
    CHECK_EQ(nudge(-STEP_NUDGE), 0);
    CHECK_EQ(step_count(&st), 5);                 /* the kick and snare in the band stay out */
    /* No pick: every note in the band goes, and stays the selection with
     * nothing picked. */
    kit();
    sweeps(4);
    CHECK_EQ(step_count(&st), 7);
    CHECK_EQ(nudge(-STEP_NUDGE), STEP_START); /* the notes at 0 */
    CHECK_EQ(nudge(60), 0);
    CHECK(!st.picked);
    CHECK_EQ(step_count(&st), 7);
    CHECK_EQ(where(7, 36, 0), 60); CHECK_EQ(where(7, 38, 0), 540); CHECK_EQ(where(7, 36, 61), 960);
    /* Backwards from the anchor too. */
    kit();
    st.pointer = 1440;
    sweeps(-4);
    step_pick(&st, 38);
    CHECK_EQ(step_count(&st), 1);                 /* 960 to 1440: the snare at 1440 */
    CHECK_EQ(nudge(30), 0);
    CHECK_EQ(st.anchor, 1470); CHECK_EQ(st.pointer, 990);
    CHECK_EQ(where(7, 38, 481), 1470);
    /* The anchor stops at 0 with the notes inside: the snare at 480 back to 0. */
    kit();
    sweeps(4);
    step_pick(&st, 38);
    CHECK_EQ(step_count(&st), 1);
    CHECK_EQ(nudge(-480), 0);
    CHECK_EQ(where(7, 38, 0), 0); CHECK_EQ(st.pointer, 0); CHECK_EQ(st.anchor, 0);
    CHECK_EQ(step_count(&st), 1);                 /* the kick at 0 stays out */
}

/* A run of turns is one undo step, kept the size of one turn, that undoes
 * and redoes whole. Anything between two turns makes two. */
static void test_undo(void)
{
    fresh(4);
    song_add_note(&song, 0, 480, 60, 100, 100);
    song_add_note(&song, 0, 480, 64, 100, 100);
    song_add_note(&song, 0, 960, 67, 100, 100);
    land(480);
    CHECK_EQ(turn(STEP_NUDGE, 0), 0);
    for (unsigned i = 0; i < 9; ++i) CHECK_EQ(turn(STEP_NUDGE, 1), 0);
    CHECK_EQ(turn(120, 1), 0);
    CHECK_EQ(where(0, 60, 0), 650); CHECK_EQ(where(0, 64, 0), 650);
    CHECK_EQ(song_undo_count(&undo, 0), 1);
    CHECK_EQ(undo.used, 2U + 4U * 12U + 2U);      /* each note's first state and last, a DEL and an ADD */
    CHECK_EQ(song_undo(&undo, &song), 0);
    CHECK_EQ(where(0, 60, 0), 480); CHECK_EQ(where(0, 64, 0), 480); CHECK_EQ(count(0), 3);
    CHECK_EQ(song_redo(&undo, &song), 0);
    CHECK_EQ(where(0, 60, 0), 650); CHECK_EQ(where(0, 64, 0), 650); CHECK_EQ(count(0), 3);
    /* After a redo a turn starts a step of its own, even told the run goes on. */
    CHECK_EQ(turn(STEP_NUDGE, 1), 0);
    CHECK_EQ(song_undo_count(&undo, 0), 2);
    /* A step between: velocity. Then two more turns, one step. */
    song_undo_begin(&undo, &song);
    CHECK_EQ(step_set_velocity(&st, 3), 0);
    song_undo_end(&undo, &song);
    CHECK_EQ(turn(STEP_NUDGE, 1), 0);
    CHECK_EQ(turn(STEP_NUDGE, 1), 0);
    CHECK_EQ(song_undo_count(&undo, 0), 4);
    CHECK_EQ(where(0, 60, 0), 665);
    song_undo(&undo, &song);
    CHECK_EQ(where(0, 60, 0), 655); CHECK_EQ(note_of(0, 60, 0)->b, 103);
    song_undo(&undo, &song);
    CHECK_EQ(note_of(0, 60, 0)->b, 100);
    song_undo(&undo, &song);
    CHECK_EQ(where(0, 60, 0), 650);
    song_undo(&undo, &song);
    CHECK_EQ(where(0, 60, 0), 480); CHECK_EQ(where(0, 64, 0), 480);
    CHECK_EQ(song_undo(&undo, &song), -1);
    /* The panel ends a run at any other control: without `more`, two steps. */
    land(480);
    CHECK_EQ(turn(STEP_NUDGE, 0), 0);
    CHECK_EQ(turn(STEP_NUDGE, 0), 0);
    CHECK_EQ(song_undo_count(&undo, 0), 2);
    /* A refused turn in a run changes nothing, and the run goes on. */
    CHECK_EQ(turn(-1000, 1), STEP_START);
    CHECK_EQ(turn(STEP_NUDGE, 1), 0);
    CHECK_EQ(song_undo_count(&undo, 0), 2);
    song_undo(&undo, &song);
    CHECK_EQ(where(0, 60, 0), 485);
    /* Pushed past the end bar by bar: the song grows, and the step keeps
     * the bars it had before the run once. */
    fresh(4);
    song_add_note(&song, 0, 5760, 60, 100, 100);
    land(5760);
    CHECK_EQ(turn(1920, 0), 0);
    CHECK_EQ(turn(1920, 1), 0);
    CHECK_EQ(turn(1920, 1), 0);
    CHECK_EQ(where(0, 60, 0), 11520); CHECK_EQ(song.bars, 7);
    CHECK_EQ(song_undo_count(&undo, 0), 1);
    CHECK_EQ(undo.used, 2U + 2U * 12U + sizeof(struct song_maps) + 4U + 2U);
    song_undo(&undo, &song);
    CHECK_EQ(where(0, 60, 0), 5760); CHECK_EQ(song.bars, 4);
    song_redo(&undo, &song);
    CHECK_EQ(where(0, 60, 0), 11520); CHECK_EQ(song.bars, 7);
}

/* The journal full as a run goes on: room() drops the oldest steps from
 * under the run's step, moving it, between two notes of one turn, and the
 * turn still finds the rest of its notes where the run left them. */
static void set_velocity(unsigned track, unsigned i, unsigned velocity)
{
    struct song_event e = *at(track, i);
    e.b = (uint8_t)velocity;
    song_retouch(&song, track, i, &e);
}
static void test_undo_room(void)
{
    unsigned run = 300, filler = (SONG_UNDO_BYTES - 10U * 28U - 4U - (run * 24U + 4U) - 1U) / 24U, wrong = 0;
    fresh(64);
    for (unsigned i = 0; i < filler; ++i) song_add_note(&song, 1, i * 10U, 40, 100, 5);
    for (unsigned i = 0; i <= run; ++i) song_add_note(&song, 0, i * 10U, 60, 100, 5);
    for (unsigned k = 0; k < 10; ++k) { /* ten small steps, the oldest */
        song_undo_begin(&undo, &song); set_velocity(1, 0, 101 + k); song_undo_end(&undo, &song);
    }
    song_undo_begin(&undo, &song);
    for (unsigned i = 0; i < filler; ++i) set_velocity(1, i, 90);
    song_undo_end(&undo, &song);
    song_undo_begin(&undo, &song);
    song_undo_run(&undo, 0);
    for (unsigned i = 0; i < run; ++i) set_velocity(0, i, 99);
    song_undo_end(&undo, &song);
    CHECK_EQ(song_undo_count(&undo, 0), 12);
    CHECK(SONG_UNDO_BYTES - undo.used < 24U);     /* no room for a DEL and an ADD more */
    song_undo_begin(&undo, &song);
    song_undo_run(&undo, 1);
    for (unsigned i = 0; i < run / 2U; ++i) set_velocity(0, i, 98);
    set_velocity(0, run, 98);                     /* new to the run: two records, and room drops a small step */
    for (unsigned i = run / 2U; i < run; ++i) set_velocity(0, i, 98);
    song_undo_end(&undo, &song);
    CHECK_EQ(song_undo_count(&undo, 0), 11);
    CHECK(undo.used <= SONG_UNDO_BYTES);
    CHECK_EQ(song_undo(&undo, &song), 0);
    for (unsigned i = 0; i <= run; ++i) wrong += at(0, i)->b != 100;
    CHECK_EQ(song_redo(&undo, &song), 0);
    for (unsigned i = 0; i <= run; ++i) wrong += at(0, i)->b != 98;
    CHECK_EQ(wrong, 0);
    CHECK_EQ(song_undo(&undo, &song), 0);
    CHECK_EQ(song_undo(&undo, &song), 0);         /* the filler */
    for (unsigned i = 0; i < filler; ++i) wrong += at(1, i)->b != (i ? 100 : 110);
    CHECK_EQ(wrong, 0);
}

/* Too big for undo: refused whole. One less note fits, and a long run of
 * it still fits, as one step. */
static void test_too_big(void)
{
    unsigned room = (unsigned)((SONG_UNDO_BYTES - 512U - sizeof(struct song_maps)) / 24U), moved = 0;
    fresh(64);
    for (unsigned i = 0; i <= room; ++i) song_add_note(&song, 0, 120U + i * 30U, 60 + i % 2, 100, 30);
    step_pick_length(&st, STEP_64TH);
    st.pointer = 120;
    sweeps((int)room);
    CHECK(step_too_big(&st));
    CHECK_EQ(turn(STEP_NUDGE, 0), STEP_TOO_BIG);
    CHECK_EQ(where(0, 60, 0), 120); CHECK_EQ(st.pointer, 120U + room * 30U);
    sweeps(-1);
    CHECK(!step_too_big(&st));
    CHECK_EQ(turn(STEP_NUDGE, 0), 0);
    for (unsigned i = 0; i < 4; ++i) CHECK_EQ(turn(STEP_NUDGE, 1), 0);
    for (unsigned i = 0; i < room; ++i) moved += song_event_start(at(0, i)) == 145U + i * 30U;
    CHECK_EQ(moved, room);
    CHECK_EQ(song_event_start(at(0, room)), 120U + room * 30U);
    CHECK_EQ(song_undo_count(&undo, 0), 1);
    CHECK(undo.used <= room * 24U + 4U);
    CHECK_EQ(song_undo(&undo, &song), 0);
    moved = 0;
    for (unsigned i = 0; i <= room; ++i) moved += song_event_start(at(0, i)) == 120U + i * 30U;
    CHECK_EQ(moved, room + 1);
    CHECK_EQ(song_redo(&undo, &song), 0);
    CHECK_EQ(song_event_start(at(0, 0)), 145);
}

/* Random turns, picks, sweeps, pointer moves and other edits, then every
 * step undone gives the song back as it was, and redone as it ended. */
static unsigned sorted(unsigned track, struct song_event *out)
{
    unsigned n = count(track);
    memcpy(out, at(track, 0), n * sizeof *out);
    for (unsigned i = 1; i < n; ++i)
        for (unsigned j = i; j > 0 && memcmp(&out[j - 1], &out[j], sizeof *out) > 0; --j) {
            struct song_event x = out[j]; out[j] = out[j - 1]; out[j - 1] = x;
        }
    return n;
}
/* One note to a pitch and clock: a moved note replaces the one it lands on. */
static int unique(unsigned track)
{
    for (unsigned i = 0; i < count(track); ++i)
        for (unsigned j = i + 1; j < count(track) && song_event_start(at(track, j)) == song_event_start(at(track, i)); ++j)
            if (at(track, j)->a == at(track, i)->a) return 0;
    return 1;
}
/* The moved notes the selection holds are on the track, as many as it
 * names, and all inside the sweep or at the pointer. */
static int held_exactly(void)
{
    uint32_t from, to;
    unsigned n = 0;
    if (!st.ids) return 1;
    if (!step_range(&st, &from, &to)) { from = st.pointer; to = from + 1U; }
    for (unsigned i = 0; i < count(st.track); ++i)
        if (step_chosen(&st, at(st.track, i))) {
            uint32_t a = song_event_start(at(st.track, i));
            if (a < from || a >= to) return 0;
            ++n;
        }
    return n == st.ids;
}
static int same(const struct song_event *a, unsigned na, uint16_t bars)
{
    static struct song_event b[256];
    return song.bars == bars && sorted(7, b) == na && !memcmp(a, b, na * sizeof *a);
}
static void test_random(void)
{
    static struct song_event first[256], last[256];
    unsigned bad = 0, unsound = 0, covered = 0;
    for (uint32_t seed = 1; seed <= 40; ++seed) {
        uint32_t x = seed * 2654435761U;
        kit();
        for (unsigned i = 0; i < count(7); ++i) song.event[song.first[7] + i].b = (uint8_t)(1 + (i * 37U + seed) % 127U);
        unsigned n0 = sorted(7, first), steps = 0, more = 0;
        uint16_t bars0 = song.bars;
        for (unsigned poll = 0; poll < 400 && steps < 50; ++poll) {
            x ^= x << 13; x ^= x >> 17; x ^= x << 5;
            unsigned op = x % 10, arg = x >> 8;
            unsigned before = song_undo_count(&undo, 0);
            song_undo_begin(&undo, &song);
            if (op < 5) {
                song_undo_run(&undo, more);
                (void)step_nudge(&st, op < 3 ? (arg & 1 ? 5 : -5) : (arg & 1 ? 1 : -1) * (int32_t)(30U << (arg >> 1) % 5U), more, NULL);
            } else if (op == 5) step_move(&st, (int)(arg % 5) - 2);
            else if (op == 6) step_sweep(&st, (int)(arg % 7) - 3, 0);
            else if (op == 7) step_pick(&st, (arg & 1) ? 42 : (arg & 2) ? 36 : 38);
            else if (op == 8) (void)step_set_velocity(&st, (int)(arg % 5) - 2);
            else st.length = (uint8_t)(arg % STEP_LENGTHS);
            song_undo_end(&undo, &song);
            more = op < 5;
            steps += song_undo_count(&undo, 0) != before;
            for (unsigned i = 1; i < count(7); ++i) unsound += song_event_start(at(7, i)) < song_event_start(at(7, i - 1));
            unsound += !unique(7) || !held_exactly();
            covered += st.covers;
        }
        unsigned n1 = sorted(7, last);
        uint16_t bars1 = song.bars;
        while (!song_undo(&undo, &song)) {}
        bad += !same(first, n0, bars0);
        while (!song_redo(&undo, &song)) {}
        bad += !same(last, n1, bars1);
    }
    CHECK_EQ(bad, 0);
    CHECK_EQ(unsound, 0);
    CHECK(covered > 0);                           /* notes were landed on, and put back */
}

/* Playing: a note moved on past the playhead sounded already, so it waits
 * for the next pass, moved again too. One moved back past it sounds from
 * the next pass, one ahead of it all along at its new place this pass, and
 * one sounding as it moves ends when it was due. */
static unsigned ons_of(unsigned pitch, unsigned from, unsigned to, uint32_t *block)
{
    unsigned n = 0;
    for (unsigned i = from; i < to; ++i)
        if (heard[i].velocity && heard[i].pitch == pitch) { if (!n++ && block) *block = heard[i].block; }
    return n;
}
static void test_playing(void)
{
    uint32_t b = 0, c = 0, d = 0, e = 0, low = 0;
    fresh(4);
    CHECK_EQ(song_set_loop(&song, 0, 1920), 0);
    song.loop_on = 1;
    song_add_note(&song, 0, 240, 60, 100, 120);
    song_add_note(&song, 0, 1200, 62, 100, 120);
    song_add_note(&song, 0, 1500, 64, 100, 120);
    song_add_note(&song, 0, 0, 48, 100, 1800);
    song_player_init(&player, &song, &listener);
    reset_capture();
    song_player_start(&player, 0);
    while (player.next < 700) run_blocks(&player, b++, 1);
    land(240);
    CHECK_EQ(step_nudge(&st, 600, 1, &player), 0);   /* C on past the playhead, to 840 */
    CHECK_EQ(step_nudge(&st, STEP_NUDGE, 1, &player), 0); /* and 845 */
    land(1200);
    CHECK_EQ(step_nudge(&st, -600, 1, &player), 0);  /* D back past it, to 600 */
    land(1500);
    CHECK_EQ(step_nudge(&st, 120, 1, &player), 0);   /* E ahead of it, to 1620 */
    land(0);
    CHECK_EQ(step_nudge(&st, STEP_NUDGE, 1, &player), 0); /* the low C, sounding */
    uint32_t pass = player.pass;
    while (player.pass == pass) run_blocks(&player, b++, 1);
    unsigned first = heard_count;
    CHECK_EQ(ons_of(60, 0, first, &c), 1); CHECK_EQ(c, exact_block(&song, 0, 0, 240));
    CHECK_EQ(ons_of(62, 0, first, NULL), 0);
    CHECK_EQ(ons_of(64, 0, first, &e), 1); CHECK_EQ(e, exact_block(&song, 0, 0, 1620));
    CHECK_EQ(ons_of(48, 0, first, NULL), 1);
    pass = player.pass;
    while (player.pass == pass) run_blocks(&player, b++, 1);
    CHECK_EQ(ons_of(60, first, heard_count, &c), 1); CHECK_EQ(c, exact_block(&song, 0, 0, 1920 + 845));
    CHECK_EQ(ons_of(62, first, heard_count, &d), 1); CHECK_EQ(d, exact_block(&song, 0, 0, 1920 + 600));
    CHECK_EQ(ons_of(64, first, heard_count, &e), 1); CHECK_EQ(e, exact_block(&song, 0, 0, 1920 + 1620));
    CHECK_EQ(ons_of(48, first, heard_count, &low), 1); CHECK_EQ(low, exact_block(&song, 0, 0, 1920 + 5));
    song_player_stop(&player);
    CHECK(balanced());
    CHECK_EQ(player.moved_count, 0);
    /* A clip open in the editor plays pass after pass on its own: the same
     * there, a pass being the clip's length. */
    fresh(4);
    CHECK_EQ(song_clip_open(&song, song_cell(0, 0)), 0);
    step_begin(&st, 0, 0);
    song_add_note(&song, 0, 240, 60, 100, 120);
    song_player_init(&player, &song, &listener);
    reset_capture();
    b = 0;
    song_player_start(&player, 0);
    while (player.next < 700) run_blocks(&player, b++, 1);
    land(240);
    CHECK_EQ(step_nudge(&st, 600, 1, &player), 0);
    while (player.next < 2 * 1920) run_blocks(&player, b++, 1);
    CHECK_EQ(ons_of(60, 0, heard_count, &c), 2);
    CHECK_EQ(c, exact_block(&song, 0, 0, 240));
    unsigned second = 0;
    for (unsigned i = 0; i < heard_count; ++i) if (heard[i].velocity) second = heard[i].block;
    CHECK_EQ(second, exact_block(&song, 0, 0, 1920 + 840));
    song_player_stop(&player);
    song_clip_close(&song);
    CHECK(balanced());
}

int main(void)
{
    test_grid();
    test_pointer();
    test_edges();
    test_sweep();
    test_undo();
    test_undo_room();
    test_too_big();
    test_random();
    test_playing();
    return report("test_step_move");
}
