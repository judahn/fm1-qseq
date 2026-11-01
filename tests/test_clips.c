/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Clips (phase 7a), the model and saving: the grid's parts, a clip opened in
 * the editor and put back, undo across both, the cell copy, cut, paste and
 * delete, a clip emptied for REPLACE (0.4.0), and song files with clips,
 * version 1 included, and the recording mode in them (0.4.0). The player's
 * side is in test_clips_play.c. */
#include "harness.h"
#include "clips.h"
#include "glitch.h"
#include "undo.h"
#include "record.h"
#include "song_file.h"
#include "pool.h"

static struct song song, other;
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

/* A cell's length set through the journal comes and goes with undo and
 * redo, the way a grid take's new clip does. */
static void test_resize(void)
{
    song_clear(&song, 8, 12000);
    song_undo_init(&undo);
    song.undo = &undo;
    unsigned cell = song_cell(5, 9);
    song_undo_begin(&undo, &song);
    clip_note(cell, 0, 60, 100);
    song_clip_resize(&song, cell, 3840);
    song_undo_end(&undo, &song);
    CHECK_EQ(song.clip_length[cell], 3840);
    CHECK_EQ(song_undo(&undo, &song), 0);
    CHECK(song.clip_length[cell] == 0 && song.count[song_cell_part(cell)] == 0);
    CHECK_EQ(song_redo(&undo, &song), 0);
    CHECK(song.clip_length[cell] == 3840 && song.count[song_cell_part(cell)] == 1);
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

/* REC in REPLACE on a cell with a clip (0.4.0): the clip is emptied first,
 * notes past its end too, its length kept, in the take's undo step, so one
 * undo gives back the clip as it was. An empty cell is refused. Open in the
 * editor, where the clip's events sit on its track, it empties there. */
static void test_empty(void)
{
    song_clear(&song, 8, 12000);
    song_undo_init(&undo);
    song.undo = &undo;
    unsigned cell = song_cell(2, 4);
    song_add_note(&song, 2, 0, 40, 100, 100);
    song.clip_length[cell] = 3840;
    clip_note(cell, 0, 60, 100);
    clip_note(cell, 960, 62, 100);
    clip_note(cell, 4000, 64, 100);
    unsigned events = song.events;
    song_undo_begin(&undo, &song);
    CHECK_EQ(song_clip_empty(&song, cell), 0);
    clip_note(cell, 480, 67, 100);                          /* the take's note, in the same step */
    song_undo_end(&undo, &song);
    CHECK(song.clip_length[cell] == 3840 && song.count[song_cell_part(cell)] == 1 && song.count[2] == 1);
    CHECK_EQ(song.event[song.first[song_cell_part(cell)]].a, 67);
    CHECK_EQ(song_undo_count(&undo, 0), 1);
    CHECK_EQ(song_undo(&undo, &song), 0);
    CHECK(song.count[song_cell_part(cell)] == 3 && song.events == events && song.clip_length[cell] == 3840);
    CHECK_EQ(song.event[song.first[song_cell_part(cell)] + 2].a, 64);
    CHECK_EQ(song_clip_empty(&song, song_cell(2, 5)), -1);
    song_clip_open(&song, cell);
    CHECK_EQ(song_clip_empty(&song, cell), 0);
    CHECK(song.count[2] == 0 && song.count[song_cell_part(cell)] == 1); /* the parts traded while it's open */
    song_clip_close(&song);
    CHECK(song.count[song_cell_part(cell)] == 0 && song.count[2] == 1 && song.clip_length[cell] == 3840);
    CHECK(consistent(&song));
    song.undo = NULL;
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
    CHECK_EQ(first[0], SONG_FILE_VERSION);
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
    first[0] = 1;                                          /* version 4's chord and 5's chromatic bytes go too */
    CHECK_EQ(song_file_read(&other, first, length - 3U - 3U * SONG_TRACKS, &block, &n), 0);
    CHECK(other.launch_q == 2 && other.events == 8 && consistent(&other));
    for (unsigned c = 0; c < SONG_CELLS; ++c) CHECK_EQ(other.clip_length[c], 0);
    CHECK_EQ(song_file_check(first, length, &block, &n), -1); /* version 1 with the grid's bytes after */
    first[0] = SONG_FILE_VERSION + 1;
    CHECK_EQ(song_file_check(first, length, &block, &n), -1);
    /* Cells out of order, and a clip with no length, are refused. */
    song.clip_length[song_cell(1, 0)] = 480;
    song.clip_length[song_cell(1, 1)] = 480;
    data = write_song(&song, &length);
    memcpy(first, data, length);
    CHECK_EQ(song_file_check(first, length, &block, &n), 0);
    const uint32_t last = length - 5U - 3U * SONG_TRACKS;   /* the last clip (cell, length, count 0), then the chords and chromatic */
    first[last] = (uint8_t)song_cell(1, 0);
    CHECK_EQ(song_file_check(first, length, &block, &n), -1);
    first[last] = (uint8_t)song_cell(1, 1);
    first[last + 1U] = first[last + 2U] = 0;
    CHECK_EQ(song_file_check(first, length, &block, &n), -1);
}

/* A file from before 0.3.0 with stutters on the FX lane (track 1, kind FX,
 * `a` below 0x80): it loads, they are dropped, and everything else, a clip
 * block included, comes in as it was, so saving it again leaves them out. */
static void test_fx_lane_file(void)
{
    static uint8_t old[BYTES];
    const uint8_t *data, *block;
    uint32_t length, n;
    song_clear(&song, 8, 12000);
    for (unsigned t = 0; t < SONG_TRACKS; ++t) song_add_note(&song, t, t * 10U, 40 + t, 100, 100);
    song.clip_length[song_cell(0, 2)] = 1920;
    clip_note(song_cell(0, 2), 0, 60, 100);
    const struct song_event stop = {3840U | (uint32_t)SONG_FX << 24, SONG_BLOCK_STOP, 0, 960};
    song_add_event(&song, 1, &stop);
    struct song_event stutter = {1920U | (uint32_t)SONG_FX << 24, 1, 66, 240}; /* a 1/8 slice, drift +2 */
    data = write_song(&song, &length);
    const uint32_t clean = song_file_crc(&song, sounds, sizeof sounds), events = song.events;
    CHECK(data != NULL);
    song_add_event(&song, 0, &stutter);
    stutter.at = 2400U | (uint32_t)SONG_FX << 24;
    stutter.a = 1 | 8; /* a triplet slice */
    song_add_event(&song, 0, &stutter);
    data = write_song(&song, &length);
    memcpy(old, data, length);
    CHECK_EQ(song_file_check(old, length, &block, &n), 0);
    CHECK_EQ(song_file_read(&other, old, length, &block, &n), 0);
    CHECK_EQ(other.events, events);
    CHECK_EQ(other.count[0], 1);                          /* track 1's note, the stutters gone */
    CHECK(consistent(&other));
    unsigned count, blocks = 0, fx = 0;
    for (unsigned p = 0; p < SONG_PARTS; ++p) {
        const struct song_event *e = song_track(&other, p, &count);
        for (unsigned i = 0; i < count; ++i) { blocks += song_is_block(&e[i]); fx += song_event_kind(&e[i]) == SONG_FX; }
    }
    CHECK(blocks == 1 && fx == 1);                         /* the block stays */
    CHECK_EQ(other.count[song_cell_part(song_cell(0, 2))], 1);
    CHECK_EQ(song_file_crc(&other, sounds, sizeof sounds), clean); /* as if they had never been there */
}

/* The recording mode (0.4.0): a new song's is OVERDUB, each live mode saves
 * and loads as it was, and STEP, the default before 0.4.0, loads as
 * OVERDUB, since a song's mode is now always a live one. */
static void test_rec_mode_file(void)
{
    const uint8_t *data, *block;
    uint32_t length, n;
    song_clear(&song, 4, 12000);
    CHECK_EQ(song.rec_mode, SONG_REC_OVERDUB);
    for (unsigned m = SONG_REC_STEP; m < SONG_REC_MODES; ++m) {
        song.rec_mode = (uint8_t)m;
        data = write_song(&song, &length);
        CHECK(data != NULL);
        CHECK_EQ(song_file_read(&other, data, length, &block, &n), 0);
        CHECK_EQ(other.rec_mode, m == SONG_REC_STEP ? SONG_REC_OVERDUB : m);
    }
}

int main(void)
{
    test_open_close();
    test_undo_parts();
    test_resize();
    test_cells();
    test_empty();
    test_file();
    test_fx_lane_file();
    test_rec_mode_file();
    return report("test_clips");
}
