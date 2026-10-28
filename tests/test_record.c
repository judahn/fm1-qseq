/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Real-time recording: quantize, replace, overdub, punch, loop passes,
 * requantizing from raw timing, undoing a take, the metronome and the
 * count-in. */
#include "harness.h"
#include "record.h"
#include "undo.h"

static struct song song;
static struct song_player player;
static struct song_recorder rec;
static struct song_undo undo;

static unsigned count(unsigned track) { unsigned n; song_track(&song, track, &n); return n; }
static const struct song_event *at(unsigned track, unsigned i) { unsigned n; return song_track(&song, track, &n) + i; }
static void tap(unsigned pitch, uint32_t on, uint32_t off)
{
    song_rec_note(&rec, pitch, 100, on);
    song_rec_note(&rec, pitch, 0, off);
}

static void test_quantize(void)
{
    song_clear(&song, 8, 12000);
    static const struct { uint8_t setting; uint16_t grid; } grids[] = {
        {1, 30}, {2, 20}, {5, 120}, {6, 80}, {7, 240}, {8, 160}, {13, 1920}, {14, 1280}};
    for (unsigned i = 0; i < sizeof grids / sizeof grids[0]; ++i) {
        song.quantize = grids[i].setting;
        CHECK_EQ(song_quantize_grid(&song), grids[i].grid);
    }
    song.quantize = 5; song.strength = 100; /* 16ths */
    CHECK_EQ(song_quantize(&song, 1919), 1920);
    CHECK_EQ(song_quantize(&song, 175), 120);
    CHECK_EQ(song_quantize(&song, 185), 240);
    song.strength = 50;
    CHECK_EQ(song_quantize(&song, 175), 147); /* halfway: 147.5, halves go to the grid */
    song.strength = 0;
    CHECK_EQ(song_quantize(&song, 175), 175);
    /* The grid starts again at each bar, so 8th triplets fit a 5/8 bar. */
    song_set_meter(&song, 1, 5, 8);
    song.quantize = 8; song.strength = 100;
    CHECK_EQ(song_quantize(&song, 1920 + 1190), 1920 + 1120);
    song.quantize = 0;
    CHECK_EQ(song_quantize(&song, 177), 177);
}

static void with_notes(void)
{
    song_clear(&song, 8, 12000);
    song_undo_init(&undo);
    song.undo = &undo;
    for (unsigned beat = 0; beat < 16; ++beat) song_add_note(&song, 2, beat * 480U, 50 + beat, 90, 240);
    song.quantize = 5; song.strength = 100;
}

static void test_modes(void)
{
    /* Overdub keeps what's there and adds the quantized take. */
    with_notes();
    song_rec_start(&rec, &song, 2, SONG_REC_OVERDUB, 0);
    tap(70, 130, 300);
    tap(71, 1000, 1100);
    song_rec_stop(&rec, 2000);
    CHECK_EQ(count(2), 18);
    CHECK_EQ(song_event_start(at(2, 1)), 120);
    CHECK_EQ(at(2, 1)->length, 170);
    for (unsigned i = 0; i < count(2); ++i) CHECK(!(at(2, i)->at & SONG_FRESH));
    /* Replace: older notes from the start of the take to where playback got
     * are erased, the rest stays. */
    with_notes();
    song_rec_start(&rec, &song, 2, SONG_REC_REPLACE, 960);
    tap(70, 950, 1000);  /* before the take start: ignored */
    tap(71, 1450, 1500);
    song_rec_follow(&rec, 2900);
    song_rec_stop(&rec, 2900);
    CHECK_EQ(count(2), 2 + 1 + 9); /* beats 0,1 + the take + beats 7..15 */
    CHECK_EQ(at(2, 2)->a, 71);
    CHECK_EQ(song_event_start(at(2, 3)), 3360);
    /* Stopped before playback got anywhere: nothing is erased. */
    with_notes();
    song_rec_start(&rec, &song, 2, SONG_REC_REPLACE, 960);
    song_rec_stop(&rec, 960);
    CHECK_EQ(count(2), 16);
    /* Punch: only between the loop points, recorded and erased. */
    with_notes();
    song.loop_start = 1920; song.loop_end = 3840;
    song_rec_start(&rec, &song, 2, SONG_REC_PUNCH, 0);
    tap(70, 500, 600);
    tap(71, 2000, 4500); /* cut at the punch out */
    song_rec_follow(&rec, 4800);
    song_rec_stop(&rec, 4800);
    CHECK_EQ(count(2), 4 + 1 + 8);
    CHECK_EQ(song_event_start(at(2, 4)), 2040);
    CHECK_EQ(at(2, 4)->length, 1800); /* to the punch out */
    /* Loop: passes overdub, a key held over the seam ends there, and each
     * pass keeps its own raw timing. */
    with_notes();
    song.loop_start = 0; song.loop_end = 1920;
    song_rec_start(&rec, &song, 5, SONG_REC_LOOP, 0);
    tap(60, 10, 50);
    song_rec_note(&rec, 62, 100, 1800);
    song_rec_follow(&rec, 1900);
    song_rec_follow(&rec, 100); /* wrapped */
    CHECK_EQ(rec.passes, 1);
    CHECK_EQ(rec.taken, 0);
    tap(64, 490, 600);
    song_rec_stop(&rec, 700);
    CHECK_EQ(count(5), 3);
    CHECK_EQ(at(5, 2)->a, 62);
    CHECK_EQ(at(5, 2)->length, 120); /* 1800 to the loop end, from the quantized 1800 */
    CHECK_EQ(rec.taken, 1);
}

static void test_requantize_and_undo(void)
{
    with_notes();
    uint64_t events = song.events;
    song_undo_begin(&undo, &song); /* the panel opens one step for the take */
    song_rec_start(&rec, &song, 2, SONG_REC_REPLACE, 0);
    tap(80, 133, 200);
    tap(81, 701, 900);
    song_rec_follow(&rec, 960);
    song_rec_stop(&rec, 960);
    song_undo_end(&undo, &song);
    CHECK_EQ(song_event_start(at(2, 0)), 120);
    /* Strength 0 brings back exactly what was played. */
    song.strength = 0;
    song_undo_begin(&undo, &song);
    CHECK_EQ(song_rec_requantize(&rec), 2);
    song_undo_end(&undo, &song);
    CHECK_EQ(song_event_start(at(2, 0)), 133);
    CHECK_EQ(song_event_start(at(2, 1)), 701);
    song.strength = 100;
    song_undo_begin(&undo, &song);
    song_rec_requantize(&rec);
    song_undo_end(&undo, &song);
    CHECK_EQ(song_event_start(at(2, 1)), 720);
    /* Undo takes each requantize back, then the whole take. */
    song_undo(&undo, &song);
    CHECK_EQ(song_event_start(at(2, 0)), 133);
    song_undo(&undo, &song);
    CHECK_EQ(song_event_start(at(2, 0)), 120);
    song_undo(&undo, &song);
    CHECK_EQ(song.events, events);
    CHECK_EQ(at(2, 0)->a, 50);
}

/* The player keeps older notes quiet in the take window, clicks on every
 * beat, and counts in before the song starts, all at exact frames. */
static void test_player_side(void)
{
    with_notes();
    song.tempo[0].bpm100 = 13300;
    song_tempo_refresh(&song);
    song_player_init(&player, &song, &listener);
    player.mute_track = 2; player.mute_from = 960; player.mute_to = 2880;
    player.click = 1;
    reset_capture();
    song_player_start(&player, 0);
    run_blocks(&player, 0, 1700);
    unsigned quiet_bad = 0, clicks = 0, click_bad = 0;
    for (unsigned i = 0; i < heard_count; ++i) {
        const struct heard *h = &heard[i];
        if (!h->velocity) continue;
        if (h->track == 2) quiet_bad += h->pitch >= 52 && h->pitch < 56;
        if (h->track == 7) {
            uint32_t beat = clicks * 480U;
            click_bad += h->block != exact_block(&song, 0, 0, beat) || h->velocity != (beat % 1920 ? 80 : 127);
            ++clicks;
        }
    }
    CHECK_EQ(quiet_bad, 0);
    CHECK(clicks >= 10);
    CHECK_EQ(click_bad, 0);
    song_player_stop(&player);
    CHECK(balanced());
    /* A one-bar count-in from bar 3: four clicks, then bar 3's first note
     * exactly four beats later. */
    song_player_init(&player, &song, &listener);
    player.playhead = 1920;
    reset_capture();
    song_player_count_in(&player, 128U * 10U, 1);
    run_blocks(&player, 10, 900);
    struct exact beat = exact_frame(&song, 480);
    unsigned k = 0, bad = 0;
    for (unsigned i = 0; i < heard_count && k < 5; ++i) {
        if (!heard[i].velocity) continue;
        if (k < 4) {
            bad += heard[i].track != 7 || heard[i].block != block_of(sum((struct exact){1280, 1}, scaled(beat, k)));
        } else {
            wide pre = (wide)4 * 480 * (wide)SONG_UNITS_PER_CLOCK / song_rate(13300); /* whole frames of count-in */
            bad += heard[i].track != 2 || heard[i].block != exact_block(&song, 1280 + (uint64_t)pre, 1920, 1920);
        }
        ++k;
    }
    CHECK_EQ(k, 5);
    CHECK_EQ(bad, 0);
    /* clock_at reads back the clock playing at an earlier frame. */
    uint32_t now = song_player_clock(&player);
    CHECK(song_player_clock_at(&player, player.frame - 128U * 50U) < now);
    song_player_stop(&player);
    CHECK(balanced());
}

int main(void)
{
    test_quantize();
    test_modes();
    test_requantize_and_undo();
    test_player_side();
    return report("test_record");
}
