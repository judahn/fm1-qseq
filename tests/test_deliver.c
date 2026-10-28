/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Note delivery by method B (song_deliver.c), as the full build runs it,
 * against a model of the FM-1. The engine plays each posted message at the
 * start of the next block it renders. The audio interrupt renders a block,
 * counts it and, while song_isr_mode is 1, wakes the control loop, which
 * runs before the next block. The control loop also runs on its own every
 * few blocks (10 ms is about 3.4 blocks), and here at random extra moments.
 * Every note-on, and every note-off before the song ends, must render in
 * the block exact rational timing puts it in, over the test song and a
 * stress song (8 tracks, chords, 1-clock notes, odd tempo changes, 20 to
 * 300 BPM). A wake the control loop misses makes notes one block late,
 * never lost. Without the wake the same model shows late notes, so the
 * check can fail. Built with -DSONG_DEVICE_FULL=1 (run_host.sh). */
#include "harness.h"
#include "song_port.h"
#include <stdlib.h>
#if SONG_HW_TEST || !SONG_UI
#error "test_deliver is built in the full build's configuration (run_host.sh)"
#endif

static struct song song;
static struct song_player player;

/* The device. */
struct sent { uint32_t block; uint8_t status, pitch, velocity; };
enum { SENT = 32768 };
static struct sent pending[256], rendered_list[SENT];
static unsigned pending_count, rendered_count, overflow;
static uint32_t rendered;   /* blocks rendered so far */
static unsigned strum_waiting;
int fm1_midi_post(const uint8_t *m, size_t bytes)
{
    if (pending_count == 256) { ++overflow; return -2; }
    pending[pending_count++] = (struct sent){0, m[0], m[1], (uint8_t)(bytes > 2 ? m[2] : 0)};
    return 0;
}
uint32_t fm1_audio_render_count(void) { return rendered; }
/* The audio interrupt: block `rendered` plays what was posted. 1: a wake. */
static int interrupt(void)
{
    for (unsigned i = 0; i < pending_count; ++i) {
        if (rendered_count == SENT) { ++overflow; break; }
        rendered_list[rendered_count] = pending[i];
        rendered_list[rendered_count++].block = rendered;
    }
    pending_count = 0;
    ++rendered;
    return song_isr_mode == 1;
}
/* What song_deliver.c needs from the rest of the port. */
struct song_player *song_port_player(void) { return &player; }
void song_port_key(const uint8_t *message, size_t bytes) { (void)message; (void)bytes; }
void song_chord_service(uint32_t block) { (void)block; }
unsigned song_chord_waiting(void) { return strum_waiting; }
void song_perf_end(void) {}
/* As song_port.c's output: a note on its track's channel. */
static void deliver(void *context, unsigned track, unsigned pitch, unsigned velocity)
{
    (void)context;
    const uint8_t m[3] = {(uint8_t)((velocity ? 0x90U : 0x80U) | track), (uint8_t)pitch, (uint8_t)velocity};
    song_deliver_note(m);
}
static const struct song_output output = {.note = deliver};

static uint32_t seed = 12345;
static unsigned rnd(unsigned n) { seed = seed * 1103515245U + 12345U; return (seed >> 16) % n; }
/* Run the device until the song ends (or `blocks`). wake: the control
 * loop answers the wake. pace: it also runs on its own every pace blocks.
 * extra: random extra polls. miss: blocks whose wake the loop misses. */
struct run { unsigned wake, pace, extra; uint32_t miss_every; };
static void play(struct run r, uint32_t blocks)
{
    for (uint32_t b = 0; b < blocks && player.playing; ++b) {
        int woken = interrupt();
        int missed = r.miss_every && rendered % r.miss_every == 0;
        if ((woken && r.wake && !missed) || rendered % r.pace == 0) song_deliver_poll();
        if (r.extra && rnd(4) == 0) song_deliver_poll();
    }
    for (unsigned i = 0; i < 4; ++i) (void)interrupt(); /* the last posts render */
}
static void start(uint32_t at_block)
{
    rendered = at_block;
    pending_count = rendered_count = overflow = 0;
    song_player_init(&player, &song, &output);
    player.playhead = 0;
    song_deliver_start(0);
}

/* Expected: each note-on, and each note-off before the song ends, as
 * (block, status, pitch), sorted, from exact timing. */
static struct sent expected[SENT];
static unsigned expected_count;
static int order(const void *a, const void *b)
{
    const struct sent *x = a, *y = b;
    if (x->block != y->block) return x->block < y->block ? -1 : 1;
    if (x->status != y->status) return x->status < y->status ? -1 : 1;
    return x->pitch < y->pitch ? -1 : x->pitch > y->pitch;
}
static void expect(uint32_t start_block)
{
    uint32_t end = song_end(&song);
    expected_count = 0;
    for (unsigned t = 0; t < SONG_TRACKS; ++t) {
        unsigned count;
        const struct song_event *e = song_track(&song, t, &count);
        for (unsigned i = 0; i < count; ++i) {
            if (!song_is_note(&e[i])) continue;
            uint32_t at = song_event_start(&e[i]);
            expected[expected_count++] = (struct sent){exact_block(&song, (uint64_t)start_block * BLOCK, 0, at),
                                                       (uint8_t)(0x90U | t), e[i].a, 0};
            if (at + e[i].length < end)
                expected[expected_count++] = (struct sent){exact_block(&song, (uint64_t)start_block * BLOCK, 0, at + e[i].length),
                                                           (uint8_t)(0x80U | t), e[i].a, 0};
        }
    }
    qsort(expected, expected_count, sizeof expected[0], order);
}
/* The rendered messages as the expectation counts them: offs only before
 * the song's end, velocity aside. */
static struct sent got[SENT];
static unsigned got_count;
static void collect(uint32_t start_block)
{
    uint32_t end_block = exact_block(&song, (uint64_t)start_block * BLOCK, 0, song_end(&song));
    got_count = 0;
    for (unsigned i = 0; i < rendered_count; ++i) {
        struct sent s = rendered_list[i];
        if ((s.status & 0xf0U) == 0x90U && !s.velocity) s.status = (uint8_t)(0x80U | (s.status & 15U));
        if ((s.status & 0xf0U) == 0x80U && s.block >= end_block) continue; /* offs at the end and at stop */
        s.velocity = 0;
        got[got_count++] = s;
    }
    qsort(got, got_count, sizeof got[0], order);
}
/* How many rendered in their due block, and how many late by 1 and by
 * more. Anything early, missing or extra counts in `wrong`. */
struct tally { unsigned on_time, late1, later, wrong; };
static struct tally compare(void)
{
    struct tally t = {0, 0, 0, 0};
    static uint8_t used[SENT];
    memset(used, 0, sizeof used);
    for (unsigned i = 0; i < expected_count; ++i) {
        const struct sent *x = &expected[i];
        unsigned best = SENT;
        for (unsigned j = 0; j < got_count; ++j) { /* the earliest unused match at or after its due block */
            if (used[j] || got[j].status != x->status || got[j].pitch != x->pitch || got[j].block < x->block) continue;
            if (best == SENT || got[j].block < got[best].block) best = j;
            if (got[j].block == x->block) break;
        }
        if (best == SENT || got[best].block > x->block + 64U) { ++t.wrong; continue; }
        used[best] = 1;
        uint32_t late = got[best].block - x->block;
        if (!late) ++t.on_time; else if (late == 1) ++t.late1; else ++t.later;
    }
    for (unsigned j = 0; j < got_count; ++j) t.wrong += !used[j];
    return t;
}

/* The stress song: 8 tracks, 4 bars at 300 BPM with a long bar at 20 BPM
 * and odd tempos at odd clocks, 1 to 5 clock notes every 19 clocks on each
 * track, offset per track, and 4-note chords on track 1 every 61 clocks.
 * At 300 BPM a clock is 18.375 frames, so a block can hold several notes. */
static void stress_song(void)
{
    song_clear(&song, 8, 30000);
    (void)song_set_tempo(&song, 1920U + 7U, 2000);
    (void)song_set_tempo(&song, 2U * 1920U + 333U, 28713);
    (void)song_set_tempo(&song, 3U * 1920U + 1001U, 17321);
    (void)song_set_tempo(&song, 4U * 1920U + 5U, 30000);
    for (unsigned t = 0; t < SONG_TRACKS; ++t)
        for (unsigned i = 0; i < 380; ++i)
            (void)song_add_note(&song, t, i * 19U + t * 2U, 36U + (i % 48U), 90, 1U + (i + t) % 5U);
    for (uint32_t at = 3; at < 7000; at += 61)
        for (unsigned k = 0; k < 4; ++k) (void)song_add_note(&song, 0, at, 100U + k * 3U, 80, 7);
    song.loop_on = 0;
}

static void test_song_on_time(void)
{
    song_build_test(&song);
    song.loop_on = 0;
    start(1000);
    CHECK_EQ(song_isr_mode, 1); /* playing: the interrupt wakes the loop */
    play((struct run){1, 4, 1, 0}, 200000);
    CHECK(!player.playing);
    CHECK_EQ(song_isr_mode, 0); /* the song ended: no more wakes */
    CHECK_EQ(overflow, 0);
    expect(1000);
    collect(1000);
    struct tally t = compare();
    CHECK(expected_count > 1000);
    CHECK_EQ(t.on_time, expected_count);
    CHECK_EQ(t.late1 + t.later + t.wrong, 0);
    printf("test song by B: %u notes on and off, %u in their block, %u late, %u wrong\n", expected_count, t.on_time,
           t.late1 + t.later, t.wrong);
}
static void test_stress_on_time(void)
{
    stress_song();
    start(77);
    play((struct run){1, 3, 1, 0}, 400000);
    CHECK(!player.playing);
    CHECK_EQ(overflow, 0);
    expect(77);
    collect(77);
    struct tally t = compare();
    CHECK(expected_count > 6000);
    CHECK_EQ(t.on_time, expected_count);
    CHECK_EQ(t.late1 + t.later + t.wrong, 0);
    /* Several notes share blocks, so this is per block, not per note. */
    unsigned shared = 0;
    for (unsigned i = 1; i < expected_count; ++i) shared += expected[i].block == expected[i - 1].block;
    CHECK(shared > 2000);
    printf("stress song by B: %u notes on and off (%u sharing a block), %u in their block, %u late, %u wrong\n",
           expected_count, shared, t.on_time, t.late1 + t.later, t.wrong);
}
/* A wake the control loop misses (it was busy past the next block): those
 * notes come one block late, and none is lost or doubled. */
static void test_missed_wake(void)
{
    stress_song();
    start(5);
    play((struct run){1, 1000000, 0, 97}, 400000);
    expect(5);
    collect(5);
    struct tally t = compare();
    CHECK(t.late1 > 0);
    CHECK_EQ(t.later + t.wrong, 0);
    CHECK_EQ(t.on_time + t.late1, expected_count);
    printf("a wake missed every 97 blocks: %u in their block, %u one late, %u later or wrong\n", t.on_time, t.late1,
           t.later + t.wrong);
}
/* The control loop alone, every 3 or 4 blocks: late notes. This is the
 * model's own check that it can see lateness. */
static void test_without_wake(void)
{
    song_build_test(&song);
    song.loop_on = 0;
    start(0);
    play((struct run){0, 4, 0, 0}, 200000);
    expect(0);
    collect(0);
    struct tally t = compare();
    CHECK(t.late1 + t.later > 100);
    CHECK_EQ(t.wrong, 0);
    printf("no wake, the loop every 4 blocks: %u in their block, %u one late, %u later\n", t.on_time, t.late1, t.later);
}
/* The wake: on while playing or while a strum waits, off otherwise. */
static void test_wake_flag(void)
{
    song_build_test(&song);
    start(10);
    CHECK_EQ(song_isr_mode, 1);
    song_port_stop();
    CHECK_EQ(song_isr_mode, 0);
    strum_waiting = 3;
    song_deliver_poll();
    CHECK_EQ(song_isr_mode, 1);
    strum_waiting = 0;
    song_deliver_poll();
    CHECK_EQ(song_isr_mode, 0);
}

int main(void)
{
    test_song_on_time();
    test_stress_on_time();
    test_missed_wake();
    test_without_wake();
    test_wake_flag();
    return report("test_deliver");
}
