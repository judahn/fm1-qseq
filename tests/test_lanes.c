/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Automation: what each engine takes, reading and chasing lanes, playback
 * at exact blocks, touch recording, and lanes travelling with arranging,
 * recording, step entry and undo. */
#include "harness.h"
#include "lanes.h"
#include "touch.h"
#include "arrange.h"
#include "record.h"
#include "undo.h"
#include "step.h"

static struct song song;
static struct song_player player;
static struct song_undo undo;
static struct song_clip clip;
struct cc { uint32_t block; uint8_t track, status, a, b; };
static struct cc ccs[4096];
static unsigned cc_count;
static void hear_cc(void *context, unsigned track, unsigned status, unsigned a, unsigned b)
{
    (void)context;
    if (cc_count < 4096) ccs[cc_count++] = (struct cc){block_now, (uint8_t)track, (uint8_t)status, (uint8_t)a, (uint8_t)b};
}
static const struct song_output both = {.note = hear, .control = hear_cc};
static unsigned count(unsigned track) { unsigned n; song_track(&song, track, &n); return n; }
static unsigned lane_count(unsigned track, unsigned lane)
{
    unsigned n, k = 0;
    const struct song_event *e = song_track(&song, track, &n);
    for (unsigned i = 0; i < n; ++i) k += !song_is_note(&e[i]) && song_event_lane(&e[i]) == lane;
    return k;
}
static void cc(unsigned track, uint32_t clock, unsigned number, unsigned value)
{
    const struct song_event e = song_lane_event(clock, number, value);
    CHECK_EQ(song_add_event(&song, track, &e), 0);
}
static void fresh_song(void)
{
    song_clear(&song, 8, 12000);
    song.undo = NULL;
    cc_count = 0;
}

static void test_catalog(void)
{
    const uint8_t *index;
    CHECK_EQ(song_params(SONG_303, &index), 9);
    CHECK(!strcmp(song_param(index[0])->name, "CUTOFF"));
    CHECK(!strcmp(song_param_for(SONG_303, 1)->name, "ENV MOD"));
    CHECK(!strcmp(song_param_for(SONG_DX7, 1)->name, "MOD WHEEL"));
    CHECK(!strcmp(song_param_for(SONG_PSG, 1)->name, "LFO DEPTH"));
    CHECK(song_param_for(SONG_DX7, 74) == NULL);
    CHECK(song_param_for(SONG_808, 74) == NULL);
    CHECK(!strcmp(song_param_for(SONG_PLUCK, 74)->name, "BRIGHT"));
    fresh_song();
    CHECK_EQ(song_knob_cc(&song, SONG_303, 1, 0), 74);
    CHECK_EQ(song_knob_cc(&song, SONG_DX7, 0, 2), 1);
    song.knob_cc[1][0] = 5;  /* slide */
    CHECK_EQ(song_knob_cc(&song, SONG_303, 1, 0), 5);
    CHECK_EQ(song_knob_cc(&song, SONG_PSG, 1, 0), 7); /* not a PSG parameter: the default */
    CHECK_EQ(song_programs(SONG_DX7), 128);
    CHECK_EQ(song_programs(SONG_808), 0);
}

static void chased_lane(void *context, unsigned track, unsigned lane, unsigned value)
{
    (void)context;
    hear_cc(NULL, track, lane, value, 0);
}
static void test_reading(void)
{
    fresh_song();
    (void)song_add_note(&song, 0, 0, 60, 100, 480);
    cc(0, 480, 74, 10); cc(0, 960, 74, 20); cc(0, 1920, 74, 30);
    cc(0, 1440, 7, 90);
    const struct song_event program = song_lane_event(960, SONG_LANE_PROGRAM, 5);
    song_add_event(&song, 0, &program);
    CHECK_EQ(song_lane_at(&song, 0, 74, 0), 10);    /* before the first: the first */
    CHECK_EQ(song_lane_at(&song, 0, 74, 960), 20);
    CHECK_EQ(song_lane_at(&song, 0, 74, 1919), 20);
    CHECK_EQ(song_lane_at(&song, 0, 11, 1919), -1);
    CHECK_EQ(song_lane_at(&song, 0, SONG_LANE_PROGRAM, 2000), 5);
    CHECK_EQ(song_lane_find(&song, 0, 74, 960), 2);
    CHECK_EQ(song_lane_next(&song, 0, 74, 960, 1), 1920);
    CHECK_EQ(song_lane_next(&song, 0, 74, 960, -1), 480);
    CHECK_EQ(song_lane_next(&song, 0, 7, 1440, 1), -1);
    /* Chase at 960: 74 had 10 before, and volume's first change comes
     * later. The program change and 74's 20 at 960 play anyway. */
    song_chase(&song, 0, 960, chased_lane, NULL);
    CHECK_EQ(cc_count, 2);
    CHECK(ccs[0].status == 7 && ccs[0].a == 90);
    CHECK(ccs[1].status == 74 && ccs[1].a == 10);
    /* With the program the song was saved with, chase puts it back before
     * the first program change. */
    song.base_program[0] = 7 + 1;
    cc_count = 0;
    song_chase(&song, 0, 480, chased_lane, NULL);
    CHECK(cc_count == 2 && ccs[1].status == SONG_LANE_PROGRAM && ccs[1].a == 7); /* and volume, first value */
    song.base_program[0] = 0;
    CHECK_EQ(song_lane_set(&song, 0, 74, 960, 25, 0), 0);
    CHECK_EQ(song_lane_at(&song, 0, 74, 960), 25);
    CHECK_EQ(lane_count(0, 74), 3);
    CHECK_EQ(song_lane_clear(&song, 1, 74, 480, 1920), 2);
    CHECK_EQ(lane_count(0, 74), 1);
    CHECK_EQ(count(0), 4);
    CHECK_EQ(song_remove_tempo(&song, 0), -1); /* the first stays */
    song_set_tempo(&song, 1920, 9000);
    CHECK_EQ(song_remove_tempo(&song, 1920), 0);
    CHECK_EQ(song.tempos, 1);
    song_set_meter(&song, 2, 3, 4);
    CHECK_EQ(song_remove_meter(&song, 2), 0);
    CHECK_EQ(song.meters, 1);
}

/* Lane changes play at exact blocks, and chase covers start, jump and
 * loop wraps. A lane a knob is recording stays out. */
static void test_playback(void)
{
    fresh_song();
    song.tempo[0].bpm100 = 13300;
    song_tempo_refresh(&song);
    for (unsigned i = 0; i < 16; ++i) cc(1, 240 + i * 60, 74, i * 8);
    cc(1, 3840, 74, 127);
    const struct song_event program = song_lane_event(1920, SONG_LANE_PROGRAM, 3);
    song_add_event(&song, 2, &program);
    (void)song_add_note(&song, 1, 0, 40, 100, 100);
    song_player_init(&player, &song, &both);
    reset_capture(); cc_count = 0;
    song_player_start(&player, 0);
    run_blocks(&player, 0, 1400);
    unsigned bad = 0, k = 0;
    for (unsigned i = 0; i < cc_count; ++i) {
        const struct cc *c = &ccs[i];
        if (c->status == 0xb0) {
            uint32_t clock = k < 16 ? 240 + k * 60 : 3840;
            if (i == 0) { bad += c->block != 0 || c->b != 0; continue; } /* chased first value */
            bad += c->block != exact_block(&song, 0, 0, clock) || c->b != (k < 16 ? k * 8 : 127);
            ++k;
        } else bad += c->status != 0xc0 || c->track != 2 || c->a != 3 || c->block != exact_block(&song, 0, 0, 1920);
    }
    CHECK_EQ(k, 17);
    CHECK_EQ(bad, 0);
    song_player_stop(&player);
    /* A jump while stopped, then play: the lanes chase to where it starts. */
    song_player_jump(&player, 960);
    cc_count = 0;
    block_now = 700;
    song_player_start(&player, 700 * 128);
    CHECK_EQ(cc_count, 1); /* the program change at 1920 is still ahead */
    CHECK(ccs[0].track == 1 && ccs[0].a == 74 && ccs[0].b == 11 * 8 && ccs[0].block == 700);
    song_player_stop(&player);
    /* Looping bars 2 and 3 of the sweep's lane: each wrap chases back to
     * the value at the loop start, unless a knob holds the lane. */
    song.loop_start = 1920; song.loop_end = 3840; song.loop_on = 1;
    player.playhead = 1920;
    cc_count = 0;
    block_now = 0;
    song_player_start(&player, 0);
    run_blocks(&player, 0, 2000);
    unsigned wraps = 0;
    for (unsigned i = 1; i < cc_count; ++i) wraps += ccs[i].status == 0xb0 && ccs[i].b == 15 * 8;
    CHECK(wraps >= 3);
    player.touch_track = 1;
    player.touch_lane[0] = 74; player.touch_lane[1] = player.touch_lane[2] = player.touch_lane[3] = 0xff;
    cc_count = 0;
    run_blocks(&player, 2000, 1300);
    unsigned leaked = 0;
    for (unsigned i = 0; i < cc_count; ++i) leaked += ccs[i].track == 1;
    CHECK_EQ(leaked, 0);
    CHECK(cc_count > 0); /* track 3's program change still plays each pass */
    song_player_stop(&player);
}

static void test_touch(void)
{
    fresh_song();
    cc(3, 0, 74, 40); cc(3, 1000, 74, 50); cc(3, 2000, 74, 60); cc(3, 4000, 74, 70);
    struct song_touch t = {0};
    /* Moves inside one 32nd keep one change, the latest. */
    song_touch_move(&song, 3, &t, 74, 80, 1210);
    song_touch_move(&song, 3, &t, 74, 81, 1230);
    song_touch_move(&song, 3, &t, 74, 82, 1250);
    CHECK_EQ(song_lane_find(&song, 3, 74, 1200), 2);
    CHECK_EQ(song_lane_at(&song, 3, 74, 1200), 82);
    for (uint32_t clock = 1260; clock < 1800; clock += 30) song_touch_move(&song, 3, &t, 74, 82 + (clock - 1260) / 60, clock);
    CHECK_EQ(lane_count(3, 74), 4 + 9);
    /* The same value again is no change. */
    song_touch_move(&song, 3, &t, 74, 90, 1800);
    CHECK_EQ(lane_count(3, 74), 4 + 9);
    /* Released at 2100: the older 2000 change in the span goes, and the
     * lane returns to its older value (60, from 2000) at the release. */
    CHECK_EQ(song_touch_release(&song, 3, &t, 2100), 60);
    CHECK_EQ(song_lane_find(&song, 3, 74, 2000), -1);
    CHECK_EQ(song_lane_at(&song, 3, 74, 2100), 60);
    CHECK_EQ(song_lane_at(&song, 3, 74, 1000), 50); /* before the touch: kept */
    CHECK_EQ(song_lane_at(&song, 3, 74, 4000), 70);
    unsigned n, thin = 1;
    const struct song_event *e = song_track(&song, 3, &n);
    for (unsigned i = 1; i < n; ++i) thin &= song_event_start(&e[i]) > song_event_start(&e[i - 1]);
    CHECK(thin);
    /* A lane with nothing before keeps the knob's value. */
    struct song_touch u = {0};
    song_touch_move(&song, 3, &u, 7, 30, 500);
    CHECK_EQ(song_touch_release(&song, 3, &u, 700), -1);
    CHECK_EQ(song_lane_at(&song, 3, 7, 5000), 30);
}

/* Recording, arranging, step entry and undo all keep automation right. */
static void test_integration(void)
{
    fresh_song();
    song_undo_init(&undo);
    song.undo = &undo;
    for (unsigned beat = 0; beat < 16; ++beat) song_add_note(&song, 2, beat * 480U, 50 + beat, 90, 240);
    cc(2, 0, 74, 10); cc(2, 1920, 74, 20); cc(2, 2400, 74, 30); cc(2, 4800, 74, 40);
    /* Replace erases the older notes it passes over, never the lanes. */
    static struct song_recorder rec;
    song_rec_start(&rec, &song, 2, SONG_REC_REPLACE, 0);
    song_rec_follow(&rec, 3000);
    song_rec_stop(&rec, 3000);
    CHECK_EQ(lane_count(2, 74), 4);
    CHECK_EQ(count(2), 4 + 9);
    /* Copy from 2000 to bar 3: the clip starts with the 20 in effect, then
     * 30 at 400 and the note. Paste inserting at bar 2. */
    CHECK_EQ(song_copy(&song, &clip, 1U << 2, 2000, 3840), 0);
    CHECK_EQ(clip.count, 3);
    CHECK_EQ(song_event_start(&clip.event[0]), 0);
    CHECK_EQ(clip.event[0].b, 20);
    song_undo_begin(&undo, &song);
    CHECK_EQ(song_paste(&song, &clip, 1920, 2, 1), 0);
    song_undo_end(&undo, &song);
    CHECK_EQ(lane_count(2, 74), 6);
    CHECK_EQ(song_lane_at(&song, 2, 74, 1920), 20);
    CHECK_EQ(song_lane_at(&song, 2, 74, 2320), 30);
    CHECK(song_lane_find(&song, 2, 74, 1920 + 1840) >= 0); /* the old ones moved */
    CHECK_EQ(song_lane_at(&song, 2, 74, 4800 + 1840), 40);
    song_undo(&undo, &song);
    CHECK_EQ(lane_count(2, 74), 4);
    CHECK(song_lane_find(&song, 2, 74, 4800) >= 0);
    /* Deleting bar 2 takes its changes and moves later ones back. */
    song_undo_begin(&undo, &song);
    song_delete_time(&song, 1920, 3840);
    song_undo_end(&undo, &song);
    CHECK_EQ(lane_count(2, 74), 2);
    CHECK_EQ(song_lane_at(&song, 2, 74, 2880), 40);
    song_undo(&undo, &song);
    CHECK_EQ(lane_count(2, 74), 4);
    /* Step entry: back-delete takes the notes, not the lane change there. */
    static struct song_step st;
    step_init(&st, &song, &listener);
    step_begin(&st, 2, 1920 + 120);
    step_back_delete(&st);
    CHECK_EQ(song_lane_find(&song, 2, 74, 1920) >= 0, 1);
    unsigned index[STEP_CHORD];
    st.pointer = 2400;
    CHECK_EQ(step_selection(&st, index), 0); /* only the lane change sits at 2400 */
}

/* A tempo change before the playhead, made while playing, held and
 * resumed: playing goes on from the same clock and point, at the tempo
 * that is now in effect there. */
static void test_hold(void)
{
    fresh_song();
    for (unsigned i = 0; i < 32; ++i) song_add_note(&song, 0, i * 240U, 60, 100, 100);
    song_player_init(&player, &song, &both);
    block_now = 0;
    song_player_start(&player, 0);
    run_blocks(&player, 0, 500);
    struct song_place place;
    song_player_hold(&player, &place);
    uint32_t clock = song_player_clock(&player);
    song_set_tempo(&song, 480, 9000);
    song_player_resume(&player, &place);
    CHECK_EQ(song_player_clock(&player), clock);
    CHECK_EQ(player.segment, 1);
    uint32_t rate = song_rate(9000), next = (clock / 240U + 1U) * 240U;
    unsigned __int128 frames = (unsigned __int128)player.frame * rate + song_units(&song, 1, next) - player.pos;
    reset_capture();
    run_blocks(&player, 500, 400);
    CHECK(heard_count > 0);
    CHECK_EQ(heard[0].velocity ? heard[0].block : heard[1].block, (uint32_t)(frames / ((unsigned __int128)rate * BLOCK)));
    song_player_stop(&player);
}

int main(void)
{
    test_catalog();
    test_reading();
    test_playback();
    test_touch();
    test_integration();
    test_hold();
    return report("test_lanes");
}
