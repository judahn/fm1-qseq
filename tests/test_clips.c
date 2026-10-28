/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Clips (phase 7a): the grid's parts, a clip opened in the editor and put
 * back, undo across both, the cell copy, cut, paste and delete, launches
 * landing exactly on the launch quantize, a clip keeping its phase across
 * a song loop and a jump, clean note-offs as a track changes source, swing
 * and glitch on clip notes, and song files with clips, version 1 included. */
#include "harness.h"
#include "clips.h"
#include "glitch.h"
#include "undo.h"
#include "song_file.h"
#include "pool.h"

static struct song song, other;
static struct song_player player;
static struct song_undo undo;
static struct song_clip board;

static void clip_note(unsigned cell, uint32_t clock, unsigned pitch, unsigned length)
{
    song_add_note(&song, song_cell_part(cell), clock, pitch, 100, length);
}
/* Every part back to back, in order, covering the pool. */
static int consistent(const struct song *s)
{
    unsigned at = 0;
    for (unsigned p = 0; p < SONG_PARTS; ++p) {
        if (s->first[p] != at) return 0;
        at += s->count[p];
    }
    return at == s->events;
}
static uint32_t block_at(uint32_t clock) { return exact_block(&song, 0, 0, clock); }
/* Plays block by block until the song clock reaches `clock`. */
static uint32_t run_to(uint32_t block, uint32_t clock)
{
    while (song_player_clock(&player) < clock) run_blocks(&player, block++, 1);
    return block;
}
/* Note-ons of a pitch on a track, and whether each landed on `expect(k)`. */
static unsigned landed(unsigned track, unsigned pitch, uint32_t first, uint32_t every, unsigned *wrong)
{
    unsigned k = 0;
    for (unsigned i = 0; i < heard_count; ++i)
        if (heard[i].velocity && heard[i].track == track && heard[i].pitch == pitch)
            *wrong += heard[i].block != block_at(first + k++ * every);
    return k;
}

static void test_open_close(void)
{
    song_clear(&song, 8, 12000);
    unsigned cell = song_cell(1, 3), n;
    song_add_note(&song, 1, 0, 40, 100, 100);
    song_add_note(&song, 1, 480, 41, 100, 100);
    song.clip_length[cell] = 1920;
    clip_note(cell, 0, 60, 100);
    clip_note(cell, 960, 62, 100);
    song_add_note(&song, 7, 0, 30, 100, 100);
    clip_note(song_cell(7, 0), 0, 31, 100);
    CHECK(consistent(&song));
    song.loop_start = 1920; song.loop_end = 7680; song.loop_on = 0;
    CHECK_EQ(song_clip_open(&song, cell), 0);
    CHECK_EQ(song_clip_open(&song, song_cell(2, 0)), -1); /* one at a time */
    CHECK_EQ(song_editing(&song), 1);
    const struct song_event *e = song_track(&song, 1, &n);
    CHECK(n == 2 && e[0].a == 60 && e[1].a == 62);       /* the clip sits in track 2's part */
    e = song_track(&song, song_cell_part(cell), &n);
    CHECK(n == 2 && e[0].a == 40);
    CHECK(consistent(&song));
    CHECK(song.loop_start == 0 && song.loop_end == 1920 && song.loop_on == 1);
    CHECK_EQ(song_end(&song), 16U * 1920U);
    song_add_note(&song, 1, 480, 61, 100, 100);           /* step entry's way into the clip */
    song.loop_end = 3U * 1920U; song_clip_follow(&song);
    CHECK_EQ(song.clip_length[cell], 5760);
    song.loop_end = 100; song_clip_follow(&song);
    CHECK_EQ(song.clip_length[cell], 480);                /* a beat at least */
    song.loop_end = 40U * 1920U; song_clip_follow(&song);
    CHECK_EQ(song.clip_length[cell], 16U * 1920U);        /* 16 bars at most */
    song.loop_end = 3840;
    song_clip_close(&song);
    CHECK(!song.open && song.clip_length[cell] == 3840);
    CHECK(song.loop_start == 1920 && song.loop_end == 7680 && song.loop_on == 0);
    CHECK_EQ(song_end(&song), 8U * 1920U);
    e = song_track(&song, 1, &n);
    CHECK(n == 2 && e[1].a == 41);
    e = song_track(&song, song_cell_part(cell), &n);
    CHECK(n == 3 && e[1].a == 61);
    e = song_track(&song, 7, &n);
    CHECK(n == 1 && e[0].a == 30);
    CHECK(consistent(&song));
    CHECK_EQ(song_clip_open(&song, song_cell(4, 9)), 0);   /* an empty cell: a new clip a bar long */
    CHECK_EQ(song.clip_length[song_cell(4, 9)], 1920);
    song_clip_close(&song);
    CHECK(consistent(&song));
}

/* Undo finds each part wherever it is: the song's track inside an open
 * clip's part, and a clip's notes after it closes. Shifts move the song only. */
static void test_undo_parts(void)
{
    song_clear(&song, 8, 12000);
    song_undo_init(&undo);
    song.undo = &undo;
    unsigned cell = song_cell(2, 0), n;
    song_undo_begin(&undo, &song); song_add_note(&song, 2, 0, 50, 100, 10); song_undo_end(&undo, &song);
    song_undo_begin(&undo, &song); song_add_note(&song, 2, 480, 51, 100, 10); song_undo_end(&undo, &song);
    song_clip_open(&song, cell);
    song_undo_begin(&undo, &song); song_add_note(&song, 2, 240, 70, 100, 10); song_undo_end(&undo, &song);
    CHECK_EQ(song_undo(&undo, &song), 0);
    CHECK_EQ(song.count[2], 0);
    CHECK_EQ(song_undo(&undo, &song), 0);                  /* the song's note, from inside the clip */
    CHECK_EQ(song.count[song_cell_part(cell)], 1);
    CHECK_EQ(song_redo(&undo, &song), 0);
    CHECK_EQ(song.count[song_cell_part(cell)], 2);
    song_clip_close(&song);
    CHECK_EQ(song_redo(&undo, &song), 0);                  /* the clip's note, with the clip closed */
    const struct song_event *e = song_track(&song, song_cell_part(cell), &n);
    CHECK(n == 1 && e[0].a == 70);
    CHECK_EQ(song.count[2], 2);
    song_clip_open(&song, cell);
    song_undo_begin(&undo, &song); CHECK_EQ(song_shift(&song, 0, 1920), 0); song_undo_end(&undo, &song);
    e = song_track(&song, 2, &n);
    CHECK(n == 1 && song_event_start(&e[0]) == 240);       /* the clip stays */
    song_clip_close(&song);
    e = song_track(&song, 2, &n);
    CHECK(n == 2 && song_event_start(&e[0]) == 1920 && song_event_start(&e[1]) == 2400);
    CHECK_EQ(song_undo(&undo, &song), 0);
    e = song_track(&song, 2, &n);
    CHECK(n == 2 && song_event_start(&e[0]) == 0);
    CHECK(consistent(&song));
    song.undo = NULL;
}

static void test_cells(void)
{
    song_clear(&song, 20, 12000);
    song_undo_init(&undo);
    song.undo = &undo;
    unsigned a = song_cell(0, 0), b = song_cell(0, 5), c = song_cell(3, 5), n;
    song.clip_length[a] = 3840;
    clip_note(a, 0, 60, 100);
    clip_note(a, 1920, 64, 100);
    song.glitch[3] = song_glitch_turn(0, GLITCH_REPEATS, 2);
    song.event[song.first[song_cell_part(a)] + 1].at |= 3U << SONG_GLITCH_SHIFT;
    clip_note(a, 4000, 66, 100);                            /* past its end: kept, never copied */
    CHECK_EQ(song_clip_copy(&song, &board, a), 0);
    CHECK(board.count == 2 && board.length == 3840 && board.tracks == 1);
    CHECK_EQ(song_clip_copy(&song, &board, song_cell(5, 5)), -1); /* an empty cell */
    song_clip_copy(&song, &board, a);
    song_undo_begin(&undo, &song); CHECK_EQ(song_clip_paste(&song, &board, c), 0); song_undo_end(&undo, &song);
    const struct song_event *e = song_track(&song, song_cell_part(c), &n);
    CHECK(n == 2 && song.clip_length[c] == 3840 && song_glitch_word(&song, &e[1]) == song.glitch[3]);
    CHECK_EQ(song_undo(&undo, &song), 0);
    CHECK(song.count[song_cell_part(c)] == 0 && song.clip_length[c] == 0);
    CHECK_EQ(song_redo(&undo, &song), 0);
    CHECK(song.count[song_cell_part(c)] == 2 && song.clip_length[c] == 3840);
    /* Bars of the song on two tracks: across the row onto their own tracks. */
    song_add_note(&song, 1, 1920, 45, 100, 100);
    song_add_note(&song, 4, 2400, 47, 100, 100);
    song_add_note(&song, 4, 6000, 48, 100, 100);           /* after the selection */
    CHECK_EQ(song_copy(&song, &board, 1U << 1 | 1U << 4, 1920, 5760), 0);
    song_undo_begin(&undo, &song); CHECK_EQ(song_clip_paste(&song, &board, song_cell(6, 7)), 0); song_undo_end(&undo, &song);
    CHECK(song.clip_length[song_cell(1, 7)] == 3840 && song.clip_length[song_cell(4, 7)] == 3840 &&
          !song.clip_length[song_cell(6, 7)]);
    e = song_track(&song, song_cell_part(song_cell(4, 7)), &n);
    CHECK(n == 1 && song_event_start(&e[0]) == 480 && e[0].a == 47);
    CHECK_EQ(song_copy(&song, &board, 1, 0, 17U * 1920U), 0);
    CHECK_EQ(song_clip_paste(&song, &board, b), -1);       /* longer than 16 bars */
    song_undo_begin(&undo, &song); CHECK_EQ(song_clip_cut(&song, &board, a), 0); song_undo_end(&undo, &song);
    CHECK(!song.clip_length[a] && !song.count[song_cell_part(a)] && board.count == 2);
    song_undo_begin(&undo, &song); CHECK_EQ(song_clip_paste(&song, &board, b), 0); song_undo_end(&undo, &song);
    CHECK_EQ(song.count[song_cell_part(b)], 2);
    song_undo_begin(&undo, &song); CHECK_EQ(song_clip_delete(&song, c), 0); song_undo_end(&undo, &song);
    CHECK(!song.clip_length[c] && !song.count[song_cell_part(c)]);
    CHECK_EQ(song_clip_delete(&song, c), -1);
    CHECK_EQ(song_undo(&undo, &song), 0);
    CHECK(song.clip_length[c] == 3840 && song.count[song_cell_part(c)] == 2);
    CHECK(consistent(&song));
    song.undo = NULL;
}

/* A launch at the bar quantize lands exactly on the bar line, at 133 BPM
 * where a bar is no whole number of frames. The song's note still sounding
 * there ends in that block, and the clip loops at its own length. */
static void test_launch_quantize(void)
{
    song_clear(&song, 16, 13300);
    for (unsigned beat = 0; beat < 64; ++beat) song_add_note(&song, 0, beat * 480U, 60, 100, 600);
    unsigned cell = song_cell(0, 2), wrong = 0;
    song.clip_length[cell] = 1920;
    clip_note(cell, 0, 72, 240);
    clip_note(cell, 960, 74, 240);
    song_player_init(&player, &song, &listener);
    reset_capture();
    song_player_start(&player, 0);
    uint32_t block = run_to(0, 2600);
    song_player_launch(&player, 0, 2);
    CHECK_EQ(player.launch_at[0], 3840);
    CHECK_EQ(player.waiting[0], 2);
    run_blocks(&player, block, 4000);
    CHECK_EQ(player.from[0], 2);
    CHECK_EQ(landed(0, 72, 3840, 1920, &wrong), (unsigned)((song_player_clock(&player) - 3840U) / 1920U + 1U));
    CHECK(landed(0, 74, 4800, 1920, &wrong) > 3);
    CHECK_EQ(landed(0, 60, 0, 480, &wrong), 8);              /* the song's notes before bar 3, none after */
    CHECK_EQ(wrong, 0);
    unsigned last = 0; /* the note from 3360 ends at the switch, not at 3960 */
    for (unsigned i = 0; i < heard_count; ++i)
        if (heard[i].track == 0 && heard[i].pitch == 60 && !heard[i].velocity) last = heard[i].block;
    CHECK_EQ(last, block_at(3840));
    /* Beat, two and four bars, and off: each from where playing is. */
    song.launch_q = 1; song_player_launch(&player, 0, 3);
    uint32_t at = player.next, beat = (at + 479U) / 480U * 480U;
    CHECK_EQ(player.launch_at[0], beat);
    song.launch_q = 3; song_player_launch(&player, 0, 3);
    CHECK_EQ(player.launch_at[0] % 3840U, 0);
    CHECK(player.launch_at[0] >= at && player.launch_at[0] < at + 3840U);
    song.launch_q = 4; song_player_launch(&player, 0, 3);
    CHECK(player.launch_at[0] % 7680U == 0 && player.launch_at[0] >= at && player.launch_at[0] < at + 7680U);
    song.launch_q = 0; song_player_launch(&player, 0, 3);
    CHECK_EQ(player.launch_at[0], at);
    song_player_launch(&player, 0, 2);                     /* the clip it plays: launched again */
    CHECK_EQ(player.waiting[0], 2);
    song_player_stop(&player);
    CHECK(player.from[0] == SONG_FROM_SONG && player.waiting[0] == SONG_FROM_NONE);
    CHECK(balanced());
}

/* A clip keeps its phase on the transport through song loops and a marker
 * jump, and launched while stopped it starts with playing. */
static void test_phase(void)
{
    song_clear(&song, 8, 12000);
    song_add_note(&song, 0, 0, 60, 100, 100);
    unsigned cell = song_cell(1, 0), wrong = 0;
    song.clip_length[cell] = 3840;
    clip_note(cell, 0, 50, 100);
    clip_note(cell, 1920, 52, 100);
    song_set_loop(&song, 0, 5760);
    song.loop_on = 1;
    song_player_init(&player, &song, &listener);
    reset_capture();
    song_player_launch(&player, 1, 0);
    song_player_start(&player, 0);
    run_blocks(&player, 0, 8400);                          /* 23,405 clocks: the 3-bar loop's fifth pass */
    CHECK_EQ(landed(1, 50, 0, 3840, &wrong), 7);
    CHECK_EQ(landed(1, 52, 1920, 3840, &wrong), 6);
    CHECK_EQ(landed(0, 60, 0, 5760, &wrong), 5);
    CHECK_EQ(wrong, 0);
    song_player_stop(&player);
    CHECK(balanced());
    /* A jump to bar 9 while playing: the clip's next pass still comes 3 bars after its last. */
    song_clear(&song, 16, 12000);
    song.clip_length[song_cell(2, 0)] = 5760;
    clip_note(song_cell(2, 0), 0, 45, 100);
    song_add_marker(&song, 8U * 1920U, "J");
    song_player_init(&player, &song, &listener);
    reset_capture();
    song_player_launch(&player, 2, 0);
    song_player_start(&player, 0);
    uint32_t block = run_to(0, 2400);
    song_player_jump(&player, 8U * 1920U);
    run_blocks(&player, block, 5000 - block);              /* 13,932 clocks, short of the song's end */
    CHECK(song_player_clock(&player) > 8U * 1920U);
    CHECK_EQ(song_player_transport(&player) - song_player_clock(&player), 3840U - 8U * 1920U); /* the shift */
    CHECK_EQ(landed(2, 45, 0, 5760, &wrong), 3);
    CHECK_EQ(wrong, 0);
    song_player_stop(&player);
    CHECK(balanced());
}

/* Song to clip, back to the song, then stopped: each change ends what the
 * track was sounding in the block it happens, and the song picks up where
 * it is. Swing and glitch apply to clip notes, timed from each pass. */
static void test_switching(void)
{
    song_clear(&song, 8, 12000);
    song_add_note(&song, 3, 0, 40, 100, 7680);
    song_add_note(&song, 3, 3840, 41, 100, 600);
    song_add_note(&song, 3, 5000, 42, 100, 2000);
    unsigned cell = song_cell(3, 1), wrong = 0;
    song.clip_length[cell] = 1920;
    clip_note(cell, 0, 70, 3000);                          /* rings past its pass */
    song_player_init(&player, &song, &listener);
    reset_capture();
    song_player_start(&player, 0);
    uint32_t block = run_to(0, 1000);
    song_player_launch(&player, 3, 1);
    block = run_to(block, 2500);
    song_player_launch(&player, 3, SONG_FROM_SONG);
    block = run_to(block, 4500);
    song_player_launch(&player, 3, SONG_FROM_STOP);
    run_blocks(&player, block, 2000);
    CHECK_EQ(player.from[3], SONG_FROM_STOP);
    unsigned offs[128] = {0};
    for (unsigned i = 0; i < heard_count; ++i) /* when each pitch last ended */
        if (heard[i].track == 3 && !heard[i].velocity) offs[heard[i].pitch] = heard[i].block + 1U;
    CHECK_EQ(offs[40], block_at(1920) + 1U);              /* the song's long note, at the launch */
    CHECK_EQ(offs[70], block_at(3840) + 1U);              /* the clip's, back to the song */
    CHECK_EQ(offs[42], block_at(5760) + 1U);              /* the song's, at the stop */
    CHECK_EQ(landed(3, 70, 1920, 1920, &wrong), 1);
    CHECK_EQ(landed(3, 41, 3840, 1920, &wrong), 1);       /* the song again, from where it is */
    CHECK_EQ(landed(3, 42, 5000, 1920, &wrong), 1);
    CHECK_EQ(wrong, 0);
    unsigned after = 0;
    for (unsigned i = 0; i < heard_count; ++i) after += heard[i].track == 3 && heard[i].velocity && heard[i].block > block_at(5760);
    CHECK_EQ(after, 0);
    song_player_stop(&player);
    CHECK(balanced());
    /* Swing: an 8th off-beat in the clip, 50% late, every pass. Glitch: two repeats. */
    song_clear(&song, 8, 12000);
    song.swing = 50;
    cell = song_cell(5, 0);
    song.clip_length[cell] = 1920;
    clip_note(cell, 240, 66, 100);
    clip_note(cell, 960, 67, 480);
    song.glitch[1] = song_glitch_turn(0, GLITCH_REPEATS, 1);
    song.event[song.first[song_cell_part(cell)] + 1].at |= 1U << SONG_GLITCH_SHIFT;
    song_player_init(&player, &song, &listener);
    reset_capture();
    song_player_launch(&player, 5, 0);
    song_player_start(&player, 0);
    run_blocks(&player, 0, 2800);                          /* four passes */
    CHECK_EQ(landed(5, 66, 360, 1920, &wrong), 4);
    unsigned k = 0;
    for (unsigned i = 0; i < heard_count; ++i)
        if (heard[i].velocity && heard[i].pitch == 67) wrong += heard[i].block != block_at(960 + k / 2U * 1920U + k % 2U * 240U), ++k;
    CHECK_EQ(k, 8);
    CHECK_EQ(wrong, 0);
    song_player_stop(&player);
    CHECK(balanced());
}

/* A clip open in the editor plays as its track's song, looped to the clip,
 * while other tracks' clips play on and the song's tracks wait. While clips
 * play the song goes round from the top at its end. */
static void test_editing_and_end(void)
{
    song_clear(&song, 8, 12000);
    for (unsigned beat = 0; beat < 32; ++beat) song_add_note(&song, 0, beat * 480U, 60, 100, 100);
    unsigned edited = song_cell(0, 4), wrong = 0;
    song.clip_length[song_cell(1, 0)] = 1920;
    clip_note(song_cell(1, 0), 0, 50, 100);
    song.clip_length[edited] = 960;
    clip_note(edited, 480, 72, 100);
    song_clip_open(&song, edited);
    song_player_init(&player, &song, &listener);
    reset_capture();
    song_player_launch(&player, 1, 0);
    song_player_start(&player, 0);
    run_blocks(&player, 0, 2700);                          /* four bars */
    CHECK_EQ(landed(0, 72, 480, 960, &wrong), 8);
    CHECK_EQ(landed(0, 60, 0, 480, &wrong), 0);
    CHECK_EQ(landed(1, 50, 0, 1920, &wrong), 4);
    CHECK_EQ(wrong, 0);
    song_player_stop(&player);
    song_clip_close(&song);
    CHECK(balanced());
    song_clear(&song, 2, 12000);
    song_add_note(&song, 0, 0, 60, 100, 100);
    song.clip_length[song_cell(1, 0)] = 5760;
    clip_note(song_cell(1, 0), 0, 50, 100);
    song_player_init(&player, &song, &listener);
    reset_capture();
    song_player_launch(&player, 1, 0);
    song_player_start(&player, 0);
    run_blocks(&player, 0, 3000);
    CHECK(player.playing);
    CHECK_EQ(landed(0, 60, 0, 3840, &wrong), 3);           /* the 2-bar song round again */
    CHECK_EQ(landed(1, 50, 0, 5760, &wrong), 2);
    CHECK_EQ(wrong, 0);
    song_player_launch(&player, 1, SONG_FROM_SONG);
    run_blocks(&player, 3000, 2000);
    CHECK(!player.playing);                                 /* no clips now: the end stops it */
    CHECK(balanced());
}

/* Files: clips, row names and the launch quantize round trip byte for byte,
 * open or not, and a version 1 file loads with an empty grid. */
enum { SECTORS = 16, BYTES = SECTORS * POOL_SECTOR };
static uint8_t flash[BYTES];
static int f_read(uint32_t at, void *data, size_t n) { if (at + n > BYTES) return -1; memcpy(data, flash + at, n); return 0; }
static int f_write(uint32_t at, const void *data, size_t n) { if (at + n > BYTES) return -1; memcpy(flash + at, data, n); return 0; }
static int f_erase(uint32_t at) { if (at >= BYTES) return -1; memset(flash + at, 0xff, POOL_SECTOR); return 0; }
static const uint8_t *f_map(uint32_t at, size_t n) { return at + n <= BYTES ? flash + at : NULL; }
static const struct song_flash ram = {f_read, f_write, f_erase, f_map};
static struct song_pool pool;
static uint8_t sounds[16];
static const uint8_t *write_song(const struct song *s, uint32_t *bytes)
{
    struct pool_writer w;
    memset(flash, 0xff, sizeof flash);
    pool_scan(&pool, &ram, 0, SECTORS, SECTORS, SECTORS);
    *bytes = song_file_bytes(s, sizeof sounds);
    if (pool_begin(&w, &pool, POOL_SONG, 1, "S", *bytes)) return NULL;
    song_file_write(&w, s, sounds, sizeof sounds);
    if (pool_end(&w)) return NULL;
    return f_map(pool_address(pool_find(&pool, POOL_SONG, 1)), *bytes);
}
static void test_file(void)
{
    static uint8_t first[BYTES];
    const uint8_t *data, *block;
    uint32_t bytes, n, length;
    song_clear(&song, 8, 12000);
    for (unsigned t = 0; t < SONG_TRACKS; ++t) song_add_note(&song, t, t * 10U, 40 + t, 100, 100);
    unsigned cells[4] = {song_cell(0, 0), song_cell(2, 7), song_cell(5, 15), song_cell(7, 15)};
    for (unsigned i = 0; i < 4; ++i) {
        song.clip_length[cells[i]] = (uint16_t)(960 * (i + 1));
        for (unsigned k = 0; k <= i; ++k) clip_note(cells[i], k * 120U, 60 + k, 50);
    }
    memcpy(song.row_name[3], "VERSE", 5);
    memcpy(song.row_name[15], "OUTRO 16", 8);
    song.launch_q = 4;
    song.loop_start = 1920; song.loop_end = 3840;
    data = write_song(&song, &length);
    CHECK(data != NULL);
    memcpy(first, data, length);
    CHECK_EQ(first[0], 2);
    song_clip_open(&song, cells[1]);                       /* open in the editor: the same bytes */
    data = write_song(&song, &bytes);
    CHECK(bytes == length && !memcmp(data, first, length));
    song_clip_close(&song);
    CHECK_EQ(song_file_read(&other, first, length, &block, &n), 0);
    CHECK(n == sizeof sounds && other.launch_q == 4 && !memcmp(other.row_name, song.row_name, sizeof song.row_name));
    CHECK(!memcmp(other.clip_length, song.clip_length, sizeof song.clip_length) && other.loop_start == 1920);
    CHECK(other.events == song.events && !memcmp(other.event, song.event, song.events * sizeof song.event[0]));
    CHECK(consistent(&other) && !memcmp(other.count, song.count, sizeof song.count));
    CHECK_EQ(song_file_crc(&other, sounds, sizeof sounds), song_file_crc(&song, sounds, sizeof sounds));
    /* Version 1: no grid. The same song without clips, version byte 1, the last three bytes gone. */
    memset(song.clip_length, 0, sizeof song.clip_length);
    memset(song.row_name, 0, sizeof song.row_name);
    for (unsigned i = 0; i < 4; ++i) while (song.count[song_cell_part(cells[i])]) song_remove(&song, song_cell_part(cells[i]), 0);
    data = write_song(&song, &length);
    memcpy(first, data, length);
    first[0] = 1;
    CHECK_EQ(song_file_read(&other, first, length - 3U, &block, &n), 0);
    CHECK(other.launch_q == 2 && other.events == 8 && consistent(&other));
    for (unsigned c = 0; c < SONG_CELLS; ++c) CHECK_EQ(other.clip_length[c], 0);
    CHECK_EQ(song_file_check(first, length, &block, &n), -1); /* version 1 with the grid's bytes after */
    first[0] = 3;
    CHECK_EQ(song_file_check(first, length, &block, &n), -1);
    /* Cells out of order, and a clip with no length, are refused. */
    song.clip_length[song_cell(1, 0)] = 480;
    song.clip_length[song_cell(1, 1)] = 480;
    data = write_song(&song, &length);
    memcpy(first, data, length);
    CHECK_EQ(song_file_check(first, length, &block, &n), 0);
    first[length - 5U] = (uint8_t)song_cell(1, 0);          /* the last clip: cell, length, count 0 */
    CHECK_EQ(song_file_check(first, length, &block, &n), -1);
    first[length - 5U] = (uint8_t)song_cell(1, 1);
    first[length - 4U] = first[length - 3U] = 0;
    CHECK_EQ(song_file_check(first, length, &block, &n), -1);
}

int main(void)
{
    test_open_close();
    test_undo_parts();
    test_cells();
    test_launch_quantize();
    test_phase();
    test_switching();
    test_editing_and_end();
    test_file();
    return report("test_clips");
}
