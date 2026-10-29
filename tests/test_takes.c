/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Phase 7b in the player: recording a performance (launches become blocks
 * that play back what was played, note for note, in exact blocks, with the
 * song's loop ignored and the take one undo step), a performance in the
 * grid's view (0.4.0: silent tracks get stop blocks, so the song plays back
 * what was heard, and a clip already playing is written from the take's
 * start), the song loop's wrap as
 * a launch point, a clip open in the editor while the song plays on, and
 * loop recording into it while the other tracks play. And 0.3.0's grid
 * recording, the player's side: a launched clip opening and closing in its
 * pass, unbroken, and a recording's launch waiting for the next beat with
 * the launch quantize off. Blocks themselves are in test_blocks.c. */
#include "harness.h"
#include "blocks.h"
#include "clips.h"
#include "record.h"
#include "undo.h"

static struct song song;
static struct song_player player;
static struct song_take take;
static uint32_t block_at(uint32_t clock) { return exact_block(&song, 0, 0, clock); }
static void clip_note(unsigned cell, uint32_t clock, unsigned pitch, unsigned length)
{
    song_add_note(&song, song_cell_part(cell), clock, pitch, 100, length);
}
/* Plays block by block until the song clock reaches `clock`, the take's
 * log drained after each, as the port does. */
static uint32_t run_to(uint32_t block, uint32_t clock)
{
    while (song_player_transport(&player) - player.shift < clock) {
        run_blocks(&player, block++, 1);
        if (player.perf) song_take_drain(&take, &player);
    }
    return block;
}
static const struct song_event *nth_block(unsigned t, unsigned nth)
{
    unsigned count;
    const struct song_event *e = song_track(&song, t, &count);
    for (unsigned i = 0; i < count; ++i) if (song_is_block(&e[i]) && !nth--) return &e[i];
    return NULL;
}
static unsigned blocks_on(unsigned t)
{
    unsigned n = 0;
    while (nth_block(t, n)) ++n;
    return n;
}
struct on { uint32_t block; uint8_t track, pitch; };
static unsigned notes_of(unsigned tracks, struct on *out)
{
    unsigned n = 0;
    for (unsigned i = 0; i < heard_count; ++i)
        if (heard[i].velocity && (tracks >> heard[i].track & 1U)) out[n++] = (struct on){heard[i].block, heard[i].track, heard[i].pitch};
    return n;
}

/* A performance: armed, PLAY starts it, launches on tracks 1 and 2 at the
 * bar quantize, a stop, a return to the song. The loop (bars 1 to 2) is
 * ignored and the song grows to hold the take. Played back from the top,
 * tracks 1 and 2 play note for note what the take did, in the same blocks,
 * and track 3, left on the song, is untouched. Undo takes it all back. */
static void test_performance(void)
{
    static struct song_undo undo;
    static struct on live[4096], back[4096];
    song_clear(&song, 6, 12000);
    for (unsigned beat = 0; beat < 24; ++beat) {
        song_add_note(&song, 0, beat * 480U, 36, 100, 100);
        song_add_note(&song, 2, beat * 480U + 240U, 50, 100, 100);
    }
    for (unsigned t = 0; t < 2; ++t)
        for (unsigned row = 0; row < 2; ++row) {
            unsigned cell = song_cell(t, row);
            song.clip_length[cell] = (uint16_t)(960 + row * 960 + t * 480);
            for (unsigned k = 0; k < 4; ++k) clip_note(cell, k * 240U, 60 + t * 12 + row * 5 + k, 120);
        }
    song_set_loop(&song, 1920, 3840);
    song.loop_on = 1;
    song_undo_init(&undo);
    song.undo = &undo;
    unsigned events = song.events, bars = song.bars;
    song_undo_begin(&undo, &song);
    song_player_init(&player, &song, &listener);
    reset_capture();
    song_take_begin(&take, &player);
    CHECK_EQ(player.perf, 1);
    song_player_start(&player, 0);
    CHECK_EQ(player.perf, 2);                                /* PLAY starts the take */
    uint32_t block = run_to(0, 1000);
    song_player_launch(&player, 0, 0);                       /* at bar 2 (1920) */
    block = run_to(block, 2500);
    song_player_launch(&player, 1, 1);                       /* at bar 3 (3840): past the loop's end */
    block = run_to(block, 6000);
    song_player_launch(&player, 0, 1);                       /* at 7680 */
    block = run_to(block, 8000);
    song_player_launch(&player, 1, SONG_FROM_SONG);          /* at 9600 */
    block = run_to(block, 10000);
    song_player_launch(&player, 0, SONG_FROM_STOP);          /* at 11520 */
    block = run_to(block, 12500);
    song_player_launch(&player, 0, SONG_FROM_SONG);          /* at 13440, past the song's end */
    block = run_to(block, 14000);
    CHECK(song_player_transport(&player) - player.shift >= 14000); /* the loop and the end went by */
    song_take_end(&take, &player);
    uint32_t ended = player.next;
    song_player_stop(&player);
    CHECK_EQ(song_undo_end(&undo, &song), 0);
    CHECK(balanced());
    unsigned n = notes_of(3U, live);
    /* The blocks. */
    CHECK_EQ(blocks_on(0), 3);
    CHECK_EQ(blocks_on(1), 1);
    CHECK_EQ(blocks_on(2), 0);
    const struct song_event *b = nth_block(0, 0);
    CHECK(b && song_event_start(b) == 1920 && song_block_length(b) == 5760 && b->a == (SONG_BLOCK | 0));
    b = nth_block(0, 1);
    CHECK(b && song_event_start(b) == 7680 && song_block_length(b) == 3840 && b->a == (SONG_BLOCK | 1));
    b = nth_block(0, 2);
    CHECK(b && song_event_start(b) == 11520 && song_block_length(b) == 1920 && b->a == SONG_BLOCK_STOP);
    b = nth_block(1, 0);
    CHECK(b && song_event_start(b) == 3840 && song_block_length(b) == 5760 && b->a == (SONG_BLOCK | 1));
    CHECK(song.bars >= 8 && song_bar_clock(&song, song.bars) >= ended);
    /* Played back from the top, the loop off: the same notes in the same blocks. */
    song.loop_on = 0;
    song_player_init(&player, &song, &listener);
    reset_capture();
    song_player_start(&player, 0);
    run_blocks(&player, 0, block);
    song_player_stop(&player);
    unsigned m = notes_of(3U, back), same = m == n;
    for (unsigned i = 0; same && i < n; ++i) same = !memcmp(&live[i], &back[i], sizeof live[i]);
    CHECK_EQ(m, n);
    CHECK(same);
    printf("performance: %u notes on tracks 1 and 2 played back as taken, 4 blocks\n", n);
    /* One undo step takes the take back: no blocks, the song's length. */
    CHECK_EQ(song_undo(&undo, &song), 0);
    CHECK(blocks_on(0) + blocks_on(1) == 0 && song.events == events && song.bars == bars);
    song.undo = NULL;
    /* Armed and ended without playing: nothing written, the song's length kept. */
    bars = song.bars;
    player.next = song_bar_clock(&song, song.bars) + 9000U; /* a stale place, past the end */
    song_take_begin(&take, &player);
    song_take_end(&take, &player);
    CHECK(song.bars == bars && blocks_on(0) + blocks_on(1) == 0);
    /* Armed while playing: nothing until the first launch, the loop working till then. */
    song_clear(&song, 4, 12000);
    song.clip_length[song_cell(4, 0)] = 1920;
    clip_note(song_cell(4, 0), 0, 70, 100);
    song_set_loop(&song, 0, 3840);
    song.loop_on = 1;
    song_player_init(&player, &song, &listener);
    song_player_start(&player, 0);
    block = run_to(0, 3000);
    song_take_begin(&take, &player);
    block = run_to(block, 3700);
    run_blocks(&player, block, 300);                          /* round the loop: still armed */
    block += 300;
    CHECK(player.perf == 1 && song_player_clock(&player) < 3840);
    song_player_launch(&player, 4, 0);
    run_blocks(&player, block, 600);
    song_take_drain(&take, &player);
    CHECK_EQ(player.perf, 2);
    song_take_end(&take, &player);
    CHECK(blocks_on(4) == 1 && song_event_start(nth_block(4, 0)) == 1920);
    song_player_stop(&player);
}

/* The launch quantize counts on the song's bar lines, and the loop's wrap
 * is a launch point too: with the loop ending mid-bar, a launch after the
 * last bar line before it lands exactly on the wrap. */
static void test_loop_wrap(void)
{
    song_clear(&song, 8, 13300);
    unsigned cell = song_cell(1, 0), wrong = 0;
    song.clip_length[cell] = 960;
    clip_note(cell, 0, 72, 100);
    song_set_loop(&song, 0, 2880);                           /* a bar and a half */
    song.loop_on = 1;
    song_player_init(&player, &song, &listener);
    reset_capture();
    song_player_start(&player, 0);
    uint32_t block = run_to(0, 2000);
    song_player_launch(&player, 1, 0);                       /* the next bar line, 3840, is past the loop */
    CHECK_EQ(player.launch_at[1], 2880U + player.shift);
    run_blocks(&player, block, 2000);
    CHECK_EQ(player.from[1], 0);
    unsigned first = 0;
    for (unsigned i = 0; i < heard_count && !first; ++i) if (heard[i].velocity && heard[i].pitch == 72) first = heard[i].block;
    CHECK_EQ(first, block_at(2880));                         /* the wrap, exactly */
    unsigned k = 0;
    for (unsigned i = 0; i < heard_count; ++i)               /* then every 960, through the next wraps */
        if (heard[i].velocity && heard[i].pitch == 72) wrong += heard[i].block != block_at(2880 + k++ * 960U);
    CHECK(k > 4);
    CHECK_EQ(wrong, 0);
    song_player_stop(&player);
    CHECK(balanced());
    /* Before the last bar line, the bar line. */
    song_player_init(&player, &song, &listener);
    song_player_start(&player, 0);
    run_to(0, 500);
    song_player_launch(&player, 1, 0);
    CHECK_EQ(player.launch_at[1], 1920U + player.shift);
    song_player_stop(&player);
}

/* A clip open in the editor while the song plays on: the other tracks keep
 * their song time and loop, the clip loops on its track from the playhead,
 * the editor's clock is the clip's, and a jump moves the clip's pass at the
 * bar line. Closed, the track plays the song again where it is. */
static void test_open_running(void)
{
    song_clear(&song, 8, 12000);
    for (unsigned beat = 0; beat < 32; ++beat) song_add_note(&song, 0, beat * 480U, 36, 100, 100);
    for (unsigned beat = 0; beat < 32; ++beat) song_add_note(&song, 1, beat * 480U, 48, 100, 100);
    unsigned cell = song_cell(1, 2), wrong = 0;
    song.clip_length[cell] = 1920;
    clip_note(cell, 0, 72, 100);
    clip_note(cell, 960, 74, 100);
    song_set_loop(&song, 0, 5760);
    song.loop_on = 1;
    song_player_init(&player, &song, &listener);
    CHECK_EQ(song_clip_open(&song, cell), 0);
    song_player_edit(&player, 1, 1);
    reset_capture();
    song_player_start(&player, 0);
    run_blocks(&player, 0, 3500);                            /* 9,752 clocks: past the song loop's wrap */
    unsigned drums = 0, k = 0;
    for (unsigned i = 0; i < heard_count; ++i)               /* the song's track 1, bar 3 and round again */
        if (heard[i].velocity && heard[i].track == 0) wrong += heard[i].block != exact_block(&song, 0, 0, k++ * 480U) && k <= 12, ++drums;
    CHECK(drums >= 18);
    k = 0;
    for (unsigned i = 0; i < heard_count; ++i)               /* the clip, looping at a bar the whole time */
        if (heard[i].velocity && heard[i].track == 1)
            wrong += heard[i].pitch != (k % 2U ? 74 : 72) || heard[i].block != block_at(k * 960U), ++k;
    CHECK(k >= 10);
    CHECK_EQ(wrong, 0);
    for (unsigned i = 0; i < heard_count; ++i) wrong += heard[i].velocity && heard[i].track == 1 && heard[i].pitch == 48;
    CHECK_EQ(wrong, 0);                                       /* the track's own song notes stay quiet */
    uint32_t now = song_player_transport(&player);
    CHECK_EQ(song_player_clock(&player), (now - player.edit_base) % 1920U);
    CHECK(song_player_transport(&player) - player.shift < 5760); /* the song went round its loop */
    /* A jump to 480 in the clip: at the next bar line the clip is at 480. */
    song_player_jump(&player, 480);
    uint32_t bar = player.jump_at + player.shift, block = 3500;  /* the bar line, in transport clocks */
    reset_capture();
    run_blocks(&player, block, 1000);
    block += 1000;
    CHECK(!player.jumping);
    unsigned first74 = 0;
    for (unsigned i = 0; i < heard_count && !first74; ++i)
        if (heard[i].velocity && heard[i].pitch == 74 && heard[i].block >= block_at(bar)) first74 = heard[i].block;
    CHECK_EQ(first74, block_at(bar + 480U));                 /* 480 on from the bar line: the clip was at 480 there */
    CHECK_EQ(song_player_clock(&player), (song_player_transport(&player) - player.edit_base) % 1920U);
    /* Closed while playing: the track plays its song from where the song is. */
    song_player_cut(&player, 1);
    song_clip_close(&song);
    song_player_edit(&player, 1, 0);
    reset_capture();
    run_blocks(&player, block, 300);
    unsigned own = 0, clip = 0;
    for (unsigned i = 0; i < heard_count; ++i)
        if (heard[i].velocity && heard[i].track == 1) { own += heard[i].pitch == 48; clip += heard[i].pitch >= 72; }
    CHECK(own > 0 && !clip);
    song_player_stop(&player);
    /* Stopped: the playhead is the clip's while open, and the song's again after. */
    player.playhead = 2000;
    song_clip_open(&song, cell);
    song_player_edit(&player, 1, 1);
    CHECK(player.playhead == 0 && player.song_at == 2000);
    song_player_jump(&player, 960);
    CHECK_EQ(song_player_clock(&player), 960);
    song_clip_close(&song);
    song_player_edit(&player, 1, 0);
    CHECK_EQ(player.playhead, 2000);
}

/* Loop recording into the open clip while the song plays on, from the
 * clip's next pass: notes land in the clip at the clip's clocks, a key held
 * over the clip's end ends there, and the song's other tracks never stop. */
static void test_record_clip(void)
{
    static struct song_recorder rec;
    song_clear(&song, 8, 12000);
    for (unsigned beat = 0; beat < 32; ++beat) song_add_note(&song, 0, beat * 480U, 36, 100, 100);
    unsigned cell = song_cell(3, 0);
    song.clip_length[cell] = 1920;
    song_player_init(&player, &song, &listener);
    song_clip_open(&song, cell);
    song_player_edit(&player, 3, 1);
    reset_capture();
    song_player_start(&player, 0);
    uint32_t block = 0, last = 0;
    while (song_player_clock(&player) < 1500) { run_blocks(&player, block++, 1); last = song_player_clock(&player); }
    while (song_player_clock(&player) >= last) { last = song_player_clock(&player); run_blocks(&player, block++, 1); } /* the next pass */
    song_rec_start(&rec, &song, 3, SONG_REC_LOOP, 0);
    static const uint16_t at[3] = {240, 960, 1800};
    for (unsigned k = 0; k < 3; ++k) {
        while (song_player_clock(&player) < at[k]) run_blocks(&player, block++, 1);
        uint32_t clock = song_player_clock(&player);
        song_rec_note(&rec, 60 + k, 100, clock);
        if (k < 2) {
            while (song_player_clock(&player) < clock + 100U) run_blocks(&player, block++, 1);
            song_rec_note(&rec, 60 + k, 0, song_player_clock(&player));
        }
        song_rec_follow(&rec, song_player_clock(&player));
    }
    while (song_player_clock(&player) >= 1800) { song_rec_follow(&rec, song_player_clock(&player)); run_blocks(&player, block++, 1); }
    song_rec_follow(&rec, song_player_clock(&player));      /* round: the held key ends at the clip's end */
    run_blocks(&player, block, 600);
    song_rec_stop(&rec, song_player_clock(&player));
    song_player_stop(&player);
    unsigned count, drums = 0;
    const struct song_event *e = song_track(&song, 3, &count);  /* the open clip is in track 4's part */
    CHECK_EQ(count, 3);
    for (unsigned k = 0; k < 3 && count == 3; ++k) CHECK(song_event_start(&e[k]) >= at[k] && song_event_start(&e[k]) < at[k] + 40U && e[k].a == 60 + k);
    CHECK(count == 3 && song_event_start(&e[2]) + e[2].length == 1920);
    for (unsigned i = 0; i < heard_count; ++i) drums += heard[i].velocity && heard[i].track == 0;
    CHECK(drums >= 10);                                       /* the song played on throughout */
    CHECK(balanced());
    song_clip_close(&song);
    CHECK_EQ(song.count[song_cell_part(cell)], 3);           /* in the clip */
}

/* A 2-bar clip launched on track 3, opened in the editor halfway through
 * its pass: the pass goes on (no note cut, its notes on their blocks), the
 * editor's clock is the clip's there, and closed with the track left on
 * that pass (as the grid's takes leave it), it plays on unbroken, launched. */
static void test_launched_pass(void)
{
    song_clear(&song, 8, 12000);
    unsigned cell = song_cell(2, 3), wrong = 0, offs = 0;
    song.clip_length[cell] = 3840;
    for (unsigned k = 0; k < 8; ++k) clip_note(cell, k * 480U, 60 + k, 400);
    song_player_init(&player, &song, &listener);
    reset_capture();
    song_player_start(&player, 0);
    uint32_t block = run_to(0, 100);
    song_player_launch(&player, 2, 3);
    block = run_to(block, 1920 + 1000);                      /* launched at bar 2, halfway through its first pass */
    CHECK_EQ(player.from[2], 3);
    uint32_t base = player.base[2];
    CHECK_EQ(base - player.shift, 1920);
    for (unsigned i = 0; i < heard_count; ++i) offs += !heard[i].velocity && heard[i].track == 2;
    CHECK_EQ(song_clip_open(&song, cell), 0);
    song_player_edit(&player, 2, 1);
    unsigned cut = 0;
    for (unsigned i = 0; i < heard_count; ++i) cut += !heard[i].velocity && heard[i].track == 2;
    CHECK_EQ(cut, offs);                                     /* nothing cut as it opened */
    CHECK_EQ(player.edit_base, base);
    CHECK_EQ(song_player_clock(&player), song_player_transport(&player) - base);
    block = run_to(block, 1920 + 3840 + 1000);               /* on into its second pass, open */
    player.from[2] = 3;                                      /* closed on its pass, as a grid take leaves it */
    player.base[2] = player.edit_base;
    offs = 0;
    for (unsigned i = 0; i < heard_count; ++i) offs += !heard[i].velocity && heard[i].track == 2;
    song_clip_close(&song);
    song_player_edit(&player, 2, 0);
    cut = 0;
    for (unsigned i = 0; i < heard_count; ++i) cut += !heard[i].velocity && heard[i].track == 2;
    CHECK_EQ(cut, offs);                                     /* nothing cut as it closed */
    run_to(block, 1920 + 3 * 3840);
    unsigned k = 0;
    for (unsigned i = 0; i < heard_count; ++i)               /* every note of every pass on its block */
        if (heard[i].velocity && heard[i].track == 2) {
            uint32_t pass = 1920 + (k / 8U) * 3840U, at = pass + (k % 8U) * 480U;
            wrong += heard[i].block != block_at(at) || heard[i].pitch != 60 + k % 8U;
            ++k;
        }
    CHECK(k >= 20);
    CHECK_EQ(wrong, 0);
    song_player_stop(&player);
    CHECK(balanced());
}

/* With the launch quantize off a launch takes over at once, but a
 * recording's launch waits for the next beat, or the beat it is on. */
static void test_launch_rec(void)
{
    song_clear(&song, 8, 12000);
    unsigned cell = song_cell(4, 0);
    song.clip_length[cell] = 1920;
    song.launch_q = 0;
    song_player_init(&player, &song, &listener);
    song_player_start(&player, 0);
    run_to(0, 700);
    song_player_launch(&player, 4, 0);
    CHECK_EQ(player.launch_at[4] - player.shift, player.next);
    song_player_launch_rec(&player, 4, 0);
    CHECK_EQ(player.launch_at[4] - player.shift, 960);       /* the next beat after 700 */
    song.launch_q = 2;
    song_player_launch_rec(&player, 4, 0);
    CHECK_EQ(player.launch_at[4] - player.shift, 1920);      /* with a quantize, the quantize */
    song_player_stop(&player);
}

/* A performance in the grid's view (0.4.0), where every track plays a clip
 * or is silent. Armed, PLAY starts it with track 1's clip launched, track 2
 * is launched at bar 2 and stopped at bar 3, and the rest stay silent. The
 * song gets what was heard: track 1's clip from the top, track 2 silent,
 * its clip, then silent, and the rest stop blocks over the whole take.
 * Played back in the song view's, every track sounds as it did in the take,
 * the song's own notes under the blocks quiet. Then, armed while track 1's
 * clip plays, the take starts with track 2's launch at bar 2, and track 1's
 * clip is written from there. */
static void test_grid_performance(void)
{
    static struct on live[4096], back[4096];
    song_clear(&song, 4, 12000);
    for (unsigned t = 0; t < SONG_TRACKS; ++t)
        for (unsigned beat = 0; beat < 16; ++beat) song_add_note(&song, t, beat * 480U, 30 + t, 100, 100);
    for (unsigned t = 0; t < 2; ++t) {
        unsigned cell = song_cell(t, 0);
        song.clip_length[cell] = 1920;
        for (unsigned k = 0; k < 4; ++k) clip_note(cell, k * 480U + 120U, 60 + t * 12 + k, 120);
    }
    song_player_init(&player, &song, &listener);
    song_player_view(&player, 1);
    reset_capture();
    song_take_begin(&take, &player);
    song_player_launch(&player, 0, 0);                       /* as playing starts */
    song_player_start(&player, 0);
    CHECK_EQ(player.perf, 2);
    uint32_t block = run_to(0, 1000);
    song_player_launch(&player, 1, 0);                       /* at 1920 */
    block = run_to(block, 2500);
    song_player_launch(&player, 1, SONG_FROM_STOP);          /* at 3840 */
    block = run_to(block, 4500);
    song_take_end(&take, &player);
    uint32_t ended = player.next, last = block_at(ended);
    song_player_stop(&player);
    CHECK(balanced());
    const struct song_event *b = nth_block(0, 0);
    CHECK(blocks_on(0) == 1 && b && song_event_start(b) == 0 && song_block_length(b) == ended && b->a == (SONG_BLOCK | 0));
    CHECK_EQ(blocks_on(1), 3);
    b = nth_block(1, 0);
    CHECK(b && song_event_start(b) == 0 && song_block_length(b) == 1920 && b->a == SONG_BLOCK_STOP);
    b = nth_block(1, 1);
    CHECK(b && song_event_start(b) == 1920 && song_block_length(b) == 1920 && b->a == (SONG_BLOCK | 0));
    b = nth_block(1, 2);
    CHECK(b && song_event_start(b) == 3840 && song_block_length(b) == ended - 3840 && b->a == SONG_BLOCK_STOP);
    unsigned stops = 0;
    for (unsigned t = 2; t < SONG_TRACKS; ++t)
        stops += blocks_on(t) == 1 && (b = nth_block(t, 0)) && song_event_start(b) == 0 && song_block_length(b) == ended &&
                 b->a == SONG_BLOCK_STOP;
    CHECK_EQ(stops, SONG_TRACKS - 2);
    unsigned n = notes_of(0xff, live), m;
    while (n && live[n - 1].block >= last) --n;
    CHECK_EQ(n, 10 + 4);                                      /* track 1's clip two passes and a half, track 2's once */
    /* Played back from the top in the song view's: the same, every track. */
    song_player_init(&player, &song, &listener);
    reset_capture();
    song_player_start(&player, 0);
    run_blocks(&player, 0, last);
    song_player_stop(&player);
    m = notes_of(0xff, back);
    while (m && back[m - 1].block >= last) --m;
    unsigned same = m == n;
    for (unsigned i = 0; same && i < n; ++i) same = !memcmp(&live[i], &back[i], sizeof live[i]);
    CHECK_EQ(m, n);
    CHECK(same);
    /* Armed while track 1's clip plays: the take starts at track 2's launch. */
    song_clear(&song, 4, 12000);
    for (unsigned t = 0; t < 2; ++t) {
        song.clip_length[song_cell(t, 0)] = 1920;
        clip_note(song_cell(t, 0), 0, 60 + t, 120);
    }
    song_player_init(&player, &song, &listener);
    song_player_view(&player, 1);
    song_player_launch(&player, 0, 0);
    song_player_start(&player, 0);
    block = run_to(0, 1000);
    song_take_begin(&take, &player);
    song_player_launch(&player, 1, 0);                       /* at 1920 */
    block = run_to(block, 2500);
    song_take_end(&take, &player);
    ended = player.next;
    song_player_stop(&player);
    b = nth_block(0, 0);
    CHECK(blocks_on(0) == 1 && b && song_event_start(b) == 1920 && song_block_length(b) == ended - 1920 && b->a == (SONG_BLOCK | 0));
    b = nth_block(1, 0);
    CHECK(blocks_on(1) == 1 && b && song_event_start(b) == 1920 && b->a == (SONG_BLOCK | 0));
    b = nth_block(5, 0);
    CHECK(blocks_on(5) == 1 && b && song_event_start(b) == 1920 && b->a == SONG_BLOCK_STOP);
}

int main(void)
{
    test_performance();
    test_grid_performance();
    test_loop_wrap();
    test_open_running();
    test_record_clip();
    test_launched_pass();
    test_launch_rec();
    return report("test_takes");
}
