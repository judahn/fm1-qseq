/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Edit jobs that move time or make events (phase 6): stretch and shift
 * played back at exact blocks, normalize heard exactly as the song it came
 * from, pasting N times, a pool near full, and the sound undo record. */
#include "jobs_harness.h"
#include "arrange.h"

static struct heard first_run[CAPTURE];
static unsigned first_count;
static void play_all(uint32_t blocks)
{
    song_player_init(&player, &song, &listener);
    for (unsigned i = 0; i < 8; ++i) player.pads[4][i] = (uint8_t)(36 + i);
    player.pad_count[4] = 8;
    reset_capture();
    song_player_start(&player, 0);
    run_blocks(&player, 0, blocks);
}
/* Every note heard on the block exact arithmetic gives for its clock. */
static unsigned misplaced(void)
{
    unsigned wrong = 0, n = 0;
    for (unsigned t = 0; t < SONG_TRACKS; ++t)
        for (unsigned i = 0; i < song.count[t]; ++i) {
            const struct song_event *e = at_index(t, i);
            if (!song_is_note(e)) continue;
            uint32_t block = exact_block(&song, 0, 0, song_event_start(e)), hits = 0;
            for (unsigned k = 0; k < heard_count; ++k)
                hits += heard[k].velocity && heard[k].track == t && heard[k].pitch == e->a && heard[k].block == block;
            wrong += !hits;
            ++n;
        }
    return n ? wrong : 1000;
}

static void test_stretch(void)
{
    fresh(8);
    note(0, 0, 60, 100, 480);
    note(0, 640, 62, 100, 160);       /* a triplet */
    note(0, 1800, 64, 100, 480);      /* runs past the range end */
    note(0, 1920, 65, 100, 100);      /* after the range */
    note(1, 2000, 40, 100, 100);      /* another track, after */
    song_undo_begin(&undo, &song);
    CHECK_EQ(song_set_tempo(&song, 1440, 9000), 0);
    CHECK_EQ(song_set_tempo(&song, 5760, 15000), 0);
    CHECK_EQ(song_add_marker(&song, 960, "IN"), 0);
    CHECK_EQ(song_add_marker(&song, 3840, "OUT"), 0);
    song_undo_end(&undo, &song);
    struct song_range r = range(0, 1920, 1);
    CHECK_EQ(JOB(song_job_stretch(&song, &r, 150)), 3);
    CHECK(has_note(0, 0, 60) && nth_note(0, 0)->length == 720);
    CHECK(has_note(0, 960, 62) && nth_note(0, 1)->length == 240);
    CHECK(has_note(0, 2700, 64) && nth_note(0, 2)->length == 720);
    CHECK(has_note(0, 2880, 65) && has_note(1, 2960, 40));
    CHECK(song.tempo[1].clock == 2160 && song.tempo[2].clock == 6720);
    CHECK(song.marker[0].clock == 1440 && song.marker[1].clock == 4800);
    CHECK_EQ(song.bars, 9);
    play_all(20000);
    CHECK_EQ(misplaced(), 0);
    CHECK(balanced());
    undo_redo();
    /* Shrinking closes [960, 1920): a note of another track there is in the way. */
    CHECK_EQ(song_undo(&undo, &song), 0);
    note(1, 1000, 41, 100, 100);
    before = song;
    r.to = 1920;
    CHECK_EQ(song_job_stretch(&song, &r, 50), -1);
    CHECK(same_song(&song, &before));
    r.tracks = 3;
    CHECK_EQ(JOB(song_job_stretch(&song, &r, 50)), 4);
    CHECK(has_note(0, 320, 62) && has_note(0, 900, 64) && has_note(1, 500, 41) && has_note(0, 960, 65) && has_note(1, 1040, 40));
    CHECK_EQ(song.tempo[1].clock, 720);
    play_all(20000);
    CHECK_EQ(misplaced(), 0);
    undo_redo();
    /* Whole bars at a bar line: the later time signatures move by bars. */
    fresh(8);
    note(0, 0, 60, 100, 100);
    song_undo_begin(&undo, &song);
    CHECK_EQ(song_set_meter(&song, 4, 7, 8), 0);
    song_undo_end(&undo, &song);
    r = range(0, 3840, 0xff);
    CHECK_EQ(JOB(song_job_stretch(&song, &r, 150)), 1);
    CHECK(song.meter[1].bar == 5 && song.bars == 9);
    undo_redo();
}

static void test_shift_exact(void)
{
    fresh(4);
    for (unsigned i = 0; i < 16; ++i) note(i & 3, i * 457U, 50 + i, 100, 200);
    song_undo_begin(&undo, &song);
    CHECK_EQ(song_set_tempo(&song, 3001, 13377), 0);
    song_undo_end(&undo, &song);
    struct song_range r = range(0, 7680, 0xf);
    CHECK_EQ(JOB(song_job_shift(&song, &r, 7)), 16);
    CHECK(has_note(0, 7, 50) && has_note(3, 15U * 457U + 7U, 65));
    play_all(20000);
    CHECK_EQ(misplaced(), 0);
    undo_redo();
}

/* Normalize: heard the same, block for block, before and after. */
static void note_glitch(unsigned track, uint32_t at, unsigned pitch, unsigned length, uint32_t word)
{
    song_undo_begin(&undo, &song);
    int g = song_glitch_intern(&song, word);
    const struct song_event e = {at | (uint32_t)(g > 0 ? g : 0) << SONG_GLITCH_SHIFT, (uint8_t)pitch, 90, (uint16_t)length};
    CHECK_EQ(song_add_event(&song, track, &e), 0);
    song_undo_end(&undo, &song);
}
static uint32_t w(unsigned field, int value, uint32_t word) { return song_glitch_turn(word, field, value); }
static int heard_cmp(const void *a, const void *b)
{
    const struct heard *x = a, *y = b;
    return x->block != y->block ? (x->block < y->block ? -1 : 1) : x->track != y->track ? x->track - y->track
         : x->pitch != y->pitch ? x->pitch - y->pitch : x->velocity - y->velocity;
}
#include <stdlib.h>
static void test_normalize(void)
{
    fresh(4);
    song_undo_begin(&undo, &song);
    CHECK_EQ(song_set_tempo(&song, 3840, 9753), 0);
    song.swing = 40; song.swing_grid = 1; song.swing_tracks = 1U << 2;
    song.humanize_time[1] = 12; song.humanize_velocity[1] = 15;
    song_undo_end(&undo, &song);
    uint32_t repeats = w(GLITCH_DRIFT, 2, w(GLITCH_FADE, 2, w(GLITCH_REPEATS, 2, 0)));
    for (unsigned i = 0; i < 8; ++i) {
        note_glitch(0, i * 960U, 60 + i, 480, i & 1 ? repeats : w(GLITCH_CHANCE, 4, w(GLITCH_OCTAVE, 2, 0)));
        note_glitch(0, i * 960U + 480U, 72, 200, w(GLITCH_FLAM, 1, w(GLITCH_ACCENT, 1, 0)));
        note(1, i * 960U + 240U, 50 + i, 80, 300);           /* humanized both ways */
        note(1, i * 960U + 250U, 70, 80, 100);
        note(2, i * 960U + 120U, 40 + i, 100, 100);          /* swung 16ths */
        note(2, i * 960U + 240U, 40, 100, 100);
        note(3, i * 960U, 30, 100, 960);                     /* plain */
        note_glitch(4, i * 960U, 36, 100, w(GLITCH_SWAP, 3, w(GLITCH_REPEATS, 1, 0)));
    }
    play_all(40000);
    memcpy(first_run, heard, heard_count * sizeof heard[0]);
    first_count = heard_count;
    struct song_player live;
    song_player_init(&live, &song, &listener);
    for (unsigned i = 0; i < 8; ++i) live.pads[4][i] = (uint8_t)(36 + i);
    live.pad_count[4] = 8;
    unsigned notes = song.events;
    CHECK(JOB(song_job_normalize(&song, 0x1f, &live)) > (int)notes);
    unsigned glitched = 0;
    for (unsigned i = 0; i < song.events; ++i) glitched += song_is_note(&song.event[i]) && song_event_glitch(&song.event[i]);
    CHECK(!glitched && !song.humanize_time[1] && !song.humanize_velocity[1] && !(song.swing_tracks & 4U));
    play_all(40000);
    CHECK_EQ(heard_count, first_count);
    qsort(first_run, first_count, sizeof first_run[0], heard_cmp);
    qsort(heard, heard_count, sizeof heard[0], heard_cmp);
    unsigned differ = 0;
    for (unsigned i = 0; i < heard_count && i < first_count; ++i) differ += heard_cmp(&heard[i], &first_run[i]) != 0;
    CHECK_EQ(differ, 0);
    undo_redo();
}

static struct song_clip clip;
static void test_paste_and_full(void)
{
    fresh(4);
    note(0, 0, 60, 100, 100); note(0, 480, 62, 100, 100); note(0, 960, 64, 100, 100);
    CHECK_EQ(song_copy(&song, &clip, 1, 0, 1920), 0);
    note(1, 4000, 99, 100, 100);       /* under the first copy: replaced */
    CHECK_EQ(JOB(song_job_paste(&song, &clip, 3840, 1, 3, 2, 0)), 9);
    CHECK(!has_note(1, 4000, 99) && has_note(1, 3840, 62) && has_note(1, 5760 + 480, 66) && has_note(1, 7680 + 960, 70));
    CHECK_EQ(song.bars, 5);           /* grown to fit */
    undo_redo();
    CHECK_EQ(song_undo(&undo, &song), 0);
    CHECK_EQ(JOB(song_job_paste(&song, &clip, 3840, 1, 2, 5, 2)), 6); /* a drum track: no transpose */
    CHECK(has_note(1, 3840, 60) && has_note(1, 5760, 60));
    CHECK_EQ(song_undo(&undo, &song), 0);
    /* A pool two short of full: anything that adds more is refused whole. */
    song.undo = NULL;
    while (song.events < SONG_EVENTS - 8) CHECK_EQ(song_add_note(&song, 7, song.events * 2U, 40, 100, 1), 0);
    song.undo = &undo;
    for (unsigned i = 0; i < 6; ++i) note(5, i * 100U, 50, 100, 10);
    before = song;
    struct song_range r = range(0, 1920, 1);
    CHECK_EQ(song_job_paste(&song, &clip, 3840, 1, 3, 0, 0), -1);
    CHECK_EQ(song_job_ramp(&song, &r, 7, 0, 127, 120, 0), -1);
    CHECK_EQ(song_job_copy_track(&song, 7, 6), -1);
    CHECK(same_song(&song, &before));
    note_glitch(6, 0, 60, 480, w(GLITCH_REPEATS, 3, 0)); /* the last free event: four hits to bake */
    before = song;
    struct song_player live;
    song_player_init(&live, &song, &listener);
    CHECK_EQ(song_job_normalize(&song, 1U << 6, &live), -1);
    CHECK(same_song(&song, &before));
    CHECK_EQ(JOB(song_job_ramp(&song, &r, 7, 0, 127, 1920, 0)), 1); /* one point still fits */
    undo_redo();
}

/* Copy Track's sound: the record swaps the setups through the port's hook. */
static uint8_t sound_now[SONG_TRACKS][SONG_SOUND_BYTES];
static void put_sound(unsigned track, const uint8_t *setup) { memcpy(sound_now[track], setup, SONG_SOUND_BYTES); }
static void test_sound_record(void)
{
    fresh(2);
    undo.sound = put_sound;
    uint8_t a[SONG_SOUND_BYTES], b[SONG_SOUND_BYTES];
    memset(a, 1, sizeof a); memset(b, 2, sizeof b);
    memcpy(sound_now[3], b, sizeof b);
    song_undo_begin(&undo, &song);
    song_undo_step(&undo);
    song_undo_sound(&undo, 3, a, b);
    CHECK_EQ(song_undo_end(&undo, &song), 0);
    CHECK_EQ(song_undo(&undo, &song), 0);
    CHECK(!memcmp(sound_now[3], a, sizeof a));
    CHECK_EQ(song_redo(&undo, &song), 0);
    CHECK(!memcmp(sound_now[3], b, sizeof b));
    song_undo_init(&undo);
    CHECK(undo.sound == put_sound);
}

int main(void)
{
    test_stretch();
    test_shift_exact();
    test_normalize();
    test_paste_and_full();
    test_sound_record();
    return report("test_jobs_time");
}
