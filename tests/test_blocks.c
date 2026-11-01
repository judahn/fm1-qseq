/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Clip blocks in the song (phase 7b, design 8f): a block against the
 * track's own notes at its edges, each in its exact audio block, a stop,
 * a loop inside a block, bar edits carrying blocks, the Expand job playing
 * note for note what the blocks played, and song files version 1 to 3.
 * Performance takes and clips in the editor are in test_takes.c. */
#include "harness.h"
#include "blocks.h"
#include "clips.h"
#include "lanes.h"
#include "song_file.h"
#include "undo.h"
#include <stdlib.h>

static struct song song, other;
static struct song_player player;
static uint8_t sounds[16];
static uint32_t block_at(uint32_t clock) { return exact_block(&song, 0, 0, clock); }
static void clip_note(unsigned cell, uint32_t clock, unsigned pitch, unsigned length)
{
    song_add_note(&song, song_cell_part(cell), clock, pitch, 100, length);
}
/* Note-ons of a pitch on a track, and whether each landed on first + k * every. */
static unsigned landed(unsigned track, unsigned pitch, uint32_t first, uint32_t every, unsigned *wrong)
{
    unsigned k = 0;
    for (unsigned i = 0; i < heard_count; ++i)
        if (heard[i].velocity && heard[i].track == track && heard[i].pitch == pitch)
            *wrong += heard[i].block != block_at(first + k++ * every);
    return k;
}
static uint32_t off_block(unsigned track, unsigned pitch, unsigned nth)
{
    for (unsigned i = 0; i < heard_count; ++i)
        if (!heard[i].velocity && heard[i].track == track && heard[i].pitch == pitch && !nth--) return heard[i].block;
    return 0xffffffffU;
}
static unsigned blocks_on(const struct song *s, unsigned t)
{
    unsigned n = 0, count;
    const struct song_event *e = song_track(s, t, &count);
    for (unsigned i = 0; i < count; ++i) n += song_is_block(&e[i]);
    return n;
}
static const struct song_event *nth_block(const struct song *s, unsigned t, unsigned nth)
{
    unsigned count;
    const struct song_event *e = song_track(s, t, &count);
    for (unsigned i = 0; i < count; ++i) if (song_is_block(&e[i]) && !nth--) return &e[i];
    return NULL;
}

/* A clip block over the track's own notes: they go quiet from its start,
 * a note sounding into it ends there, the clip loops from the block's
 * start, a clip note ringing past its end ends there, and the track's own
 * notes come back at the end, all in their exact blocks, at 133 BPM. */
static void test_edges(void)
{
    song_clear(&song, 8, 13300);
    for (unsigned beat = 0; beat < 32; ++beat) song_add_note(&song, 2, beat * 480U, 60, 100, 400);
    song_add_note(&song, 2, 3000, 61, 100, 2000);           /* sounds into the block */
    unsigned cell = song_cell(2, 3), wrong = 0;
    song.clip_length[cell] = 1920;
    clip_note(cell, 0, 72, 100);
    clip_note(cell, 960, 74, 2000);                         /* rings past each pass, and past the block */
    CHECK_EQ(song_block_put(&song, 2, 3840, 8640, 3), 0);
    CHECK(blocks_on(&song, 2) == 1 && song_block_length(nth_block(&song, 2, 0)) == 4800);
    /* A stop on track 4 over bar 2. */
    for (unsigned beat = 0; beat < 16; ++beat) song_add_note(&song, 4, beat * 480U, 40, 100, 100);
    CHECK_EQ(song_block_put(&song, 4, 1920, 3840, SONG_BLOCK_STOP), 0);
    song_player_init(&player, &song, &listener);
    reset_capture();
    song_player_start(&player, 0);
    run_blocks(&player, 0, 6000);                           /* past bar 6 */
    unsigned before = 0, inside = 0, after = 0;
    for (unsigned i = 0; i < heard_count; ++i)
        if (heard[i].velocity && heard[i].track == 2 && heard[i].pitch == 60) {
            if (heard[i].block < block_at(3840)) ++before;
            else if (heard[i].block < block_at(8640)) ++inside;
            else ++after;
        }
    CHECK_EQ(before, 8);
    CHECK_EQ(inside, 0);                                    /* the note right at its start is quiet too */
    CHECK(after >= 4);
    for (unsigned i = 0, k = 0, j = 0; i < heard_count; ++i) /* in step before it, and from 8640 on */
        if (heard[i].velocity && heard[i].track == 2 && heard[i].pitch == 60)
            wrong += heard[i].block != block_at(heard[i].block < block_at(3840) ? k++ * 480U : 8640U + j++ * 480U);
    CHECK_EQ(off_block(2, 61, 0), block_at(3840));          /* ends where the block starts */
    CHECK_EQ(landed(2, 72, 3840, 1920, &wrong), 3);         /* 3840, 5760, 7680 */
    CHECK_EQ(landed(2, 74, 4800, 1920, &wrong), 2);         /* not at 8640: the block's over */
    CHECK_EQ(off_block(2, 74, 1), block_at(8640));          /* the last one cut where it ends */
    unsigned first_after = 0;
    for (unsigned i = 0; i < heard_count && !first_after; ++i)
        if (heard[i].velocity && heard[i].track == 2 && heard[i].pitch == 60 && heard[i].block >= block_at(8640))
            first_after = heard[i].block;
    CHECK_EQ(first_after, block_at(8640));
    unsigned stopped = 0, around = 0;
    for (unsigned i = 0; i < heard_count; ++i)
        if (heard[i].velocity && heard[i].track == 4) {
            stopped += heard[i].block >= block_at(1920) && heard[i].block < block_at(3840);
            ++around;
        }
    CHECK_EQ(stopped, 0);
    CHECK_EQ(around, 12);
    CHECK_EQ(wrong, 0);
    song_player_stop(&player);
    CHECK(balanced());
    /* A loop starting inside the block: the clip goes on from the block's
     * start, so the loop's start (960 into a pass) plays the 74. */
    song_set_loop(&song, 4800, 7680);
    song.loop_on = 1;
    song_player_init(&player, &song, &listener);
    reset_capture();
    player.playhead = 4800;
    song_player_start(&player, 0);
    run_blocks(&player, 0, 3000);                           /* three passes of 1.5 bars */
    unsigned n74 = 0, n72 = 0, n60 = 0;
    for (unsigned i = 0; i < heard_count; ++i)
        if (heard[i].velocity) { n74 += heard[i].pitch == 74; n72 += heard[i].pitch == 72; n60 += heard[i].track == 2 && heard[i].pitch == 60; }
    CHECK(n74 >= 6 && n72 >= 3 && n60 == 0);
    CHECK(nth_on(0)->pitch == 74 || nth_on(0)->track == 4);
    song_player_stop(&player);
    CHECK(balanced());
}

/* Bars carry blocks as they carry notes: inserting and deleting bars, copy
 * and paste, duplicate (joined into one block), stretch, and a clip copied
 * in the grid pasted as a block, which must be this song's clip. */
static void test_bar_edits(void)
{
    static struct song_clip board;
    song_clear(&song, 16, 12000);
    unsigned cell = song_cell(1, 0);
    song.clip_length[cell] = 1920;
    clip_note(cell, 0, 50, 100);
    CHECK_EQ(song_block_put(&song, 1, 3840, 7680, 0), 0);
    CHECK_EQ(song_insert_time(&song, 1920, 1920), 0);
    CHECK(song_event_start(nth_block(&song, 1, 0)) == 5760 && song_block_length(nth_block(&song, 1, 0)) == 3840);
    CHECK_EQ(song_delete_time(&song, 0, 1920), 0);
    CHECK_EQ(song_event_start(nth_block(&song, 1, 0)), 3840);
    CHECK_EQ(song_copy(&song, &board, 1U << 1, 3840, 5760), 0); /* half of it: cut at the copy's end */
    CHECK(board.count == 1 && song_block_length(&board.event[0]) == 1920 && !board.cell);
    CHECK_EQ(song_paste(&song, &board, 13440, 1, 0), 0);
    CHECK(blocks_on(&song, 1) == 2 && song_event_start(nth_block(&song, 1, 1)) == 13440);
    song_add_note(&song, 1, 4000, 60, 100, 100);             /* a note of the track's own under it */
    uint32_t from = 3840, to = 7680;
    CHECK_EQ(song_duplicate(&song, 1U << 1, &from, &to), 0);
    CHECK_EQ(blocks_on(&song, 1), 3);
    song_blocks_join(&song, 1U << 1);
    CHECK(blocks_on(&song, 1) == 2 && song_block_length(nth_block(&song, 1, 0)) == 7680);
    const struct song_range all = {0, 40000, 1U << 1, 0};
    CHECK(song_job_stretch(&song, &all, 200) > 0);
    CHECK(song_event_start(nth_block(&song, 1, 0)) == 7680 && song_block_length(nth_block(&song, 1, 0)) == 15360);
    /* The grid's copy, as a block at a clock, its clip's length long. */
    CHECK_EQ(song_clip_copy(&song, &board, cell), 0);
    CHECK_EQ(board.cell, cell + 1U);
    CHECK_EQ(song_block_paste(&song, &board, 1920), 0);
    const struct song_event *b = nth_block(&song, 1, 0);
    CHECK(song_event_start(b) == 1920 && song_block_length(b) == 1920 && b->a == (SONG_BLOCK | 0));
    clip_note(cell, 480, 51, 100);                          /* the clip changed: no longer the copy */
    CHECK_EQ(song_block_paste(&song, &board, 0), -2);
    board.cell = 0;
    CHECK_EQ(song_block_paste(&song, &board, 0), -1);
    /* A block put over others: those starting inside go, one reaching in ends there. */
    song_clear(&song, 16, 12000);
    song_block_put(&song, 0, 0, 7680, 1);
    song_block_put(&song, 0, 5760, 9600, 2);
    song_block_put(&song, 0, 3840, 6000, SONG_BLOCK_STOP);
    CHECK_EQ(blocks_on(&song, 0), 2);
    CHECK(song_block_length(nth_block(&song, 0, 0)) == 3840 && nth_block(&song, 0, 1)->a == SONG_BLOCK_STOP);
}

/* Expand: a song full of blocks (passes cut short, a stop, blocks back to
 * back, notes and controllers of the track's own inside and across the
 * edges) plays, then plays again expanded: the same notes in the same
 * blocks. A song that can't hold the result is left as it was. */
struct note_heard { uint32_t block; uint8_t track, pitch, velocity; };
static int by_block(const void *a, const void *b)
{
    const struct note_heard *x = a, *y = b;
    if (x->block != y->block) return x->block < y->block ? -1 : 1;
    if (x->track != y->track) return x->track - y->track;
    if (x->pitch != y->pitch) return x->pitch - y->pitch;
    return x->velocity - y->velocity;
}
static int by_event(const void *a, const void *b) { return memcmp(a, b, sizeof(struct song_event)); }
static unsigned played(struct note_heard *out, unsigned blocks)
{
    song_player_init(&player, &song, &listener);
    reset_capture();
    song_player_start(&player, 0);
    run_blocks(&player, 0, blocks);
    song_player_stop(&player);
    for (unsigned i = 0; i < heard_count; ++i) out[i] = (struct note_heard){heard[i].block, heard[i].track, heard[i].pitch, heard[i].velocity};
    qsort(out, heard_count, sizeof *out, by_block);
    return heard_count;
}
static void build_blocks(void)
{
    song_clear(&song, 12, 12000);
    for (unsigned t = 0; t < 4; ++t) {
        for (unsigned i = 0; i < 40; ++i) song_add_note(&song, t, i * 540U + t * 7U, 48 + t * 5 + i % 7, 90, 300 + (i % 3) * 400);
        const struct song_event cc = song_lane_event(100 + t, 74, 10 + t), cc2 = song_lane_event(9000, 74, 99);
        song_add_event(&song, t, &cc);
        song_add_event(&song, t, &cc2);
        for (unsigned row = 0; row < 2; ++row) {
            unsigned cell = song_cell(t, row);
            song.clip_length[cell] = (uint16_t)(1440 + row * 1920 + t * 240);
            for (unsigned k = 0; k < 6; ++k) clip_note(cell, k * 300U + row * 20U, 70 + k + row, 150 + k * 120);
            const struct song_event c = song_lane_event(600, 74, 64 + row);
            song_add_event(&song, song_cell_part(cell), &c);
            clip_note(cell, song.clip_length[cell] + 100U, 100, 50); /* past its end: never plays */
        }
    }
    song_block_put(&song, 0, 1920, 7000, 0);                 /* 3.5 passes */
    song_block_put(&song, 0, 7000, 11000, 1);                /* back to back */
    song_block_put(&song, 1, 3840, 5760, SONG_BLOCK_STOP);
    song_block_put(&song, 1, 9000, 15000, 1);
    song_block_put(&song, 2, 0, 2000, 1);
    song_block_put(&song, 3, 5000, 23040, 0);                /* to the song's end */
}
static void test_expand(void)
{
    static struct note_heard a[CAPTURE], b[CAPTURE];
    build_blocks();
    unsigned blocks = exact_block(&song, 0, 0, song_end(&song)) + 1U;
    unsigned n = played(a, blocks);
    const struct song_range all = {0, song_end(&song), 0xff, 0};
    unsigned before = song.events;
    int added = song_job_expand(&song, &all);
    CHECK(added > 0);
    unsigned left = 0;
    for (unsigned t = 0; t < SONG_TRACKS; ++t) left += blocks_on(&song, t);
    CHECK_EQ(left, 0);
    CHECK(song.events > before);
    unsigned m = played(b, blocks);
    CHECK_EQ(m, n);
    unsigned same = n == m;
    for (unsigned i = 0; same && i < n; ++i) same = !memcmp(&a[i], &b[i], sizeof a[i]);
    CHECK(same);
    if (!same)
        for (unsigned i = 0; i < n && i < m; ++i)
            if (memcmp(&a[i], &b[i], sizeof a[i])) {
                printf("expand differs at %u: block %u t%u p%u v%u, expanded block %u t%u p%u v%u\n", i, a[i].block, a[i].track,
                       a[i].pitch, a[i].velocity, b[i].block, b[i].track, b[i].pitch, b[i].velocity);
                break;
            }
    printf("expand: %u notes on and off, the same with the blocks expanded (%d events added)\n", n, added);
    /* Only blocks starting in the range. */
    build_blocks();
    const struct song_range some = {6000, 9500, 1U << 0 | 1U << 1, 0};
    CHECK(song_job_expand(&song, &some) > 0);
    CHECK(blocks_on(&song, 0) == 1 && blocks_on(&song, 1) == 1 && blocks_on(&song, 3) == 1);
    /* No room: nothing changes. */
    build_blocks();
    while (song.events < SONG_EVENTS - 20U) song_add_note(&song, 7, song.events, 60, 1, 1);
    static struct song_event kept[SONG_EVENTS];
    unsigned events = song.events;
    memcpy(kept, song.event, sizeof kept);
    CHECK_EQ(song_job_expand(&song, &all), -1);
    CHECK(song.events == events && !memcmp(kept, song.event, events * sizeof kept[0]));
    /* Undo takes it back whole. */
    static struct song_undo undo;
    build_blocks();
    song_undo_init(&undo);
    song.undo = &undo;
    memcpy(kept, song.event, sizeof kept);
    events = song.events;
    song_undo_begin(&undo, &song);
    CHECK(song_job_expand(&song, &all) > 0);
    CHECK_EQ(song_undo_end(&undo, &song), 0);
    CHECK_EQ(song_undo(&undo, &song), 0);
    /* Back as it was. Events on one clock may come back in another order. */
    static struct song_event now[SONG_EVENTS];
    memcpy(now, song.event, sizeof now);
    for (unsigned t = 0; t < SONG_TRACKS; ++t) {
        qsort(kept + song.first[t], song.count[t], sizeof kept[0], by_event);
        qsort(now + song.first[t], song.count[t], sizeof now[0], by_event);
    }
    CHECK(song.events == events && !memcmp(kept, now, events * sizeof kept[0]));
    song.undo = NULL;
}

/* Song files: version 3 keeps the blocks, versions 1 and 2 still load, and
 * a block where it can't be is refused. */
enum { SECTORS = 16, BYTES = SECTORS * POOL_SECTOR };
static uint8_t flash[BYTES], first[BYTES];
enum { CHORD_BYTES = 3 * SONG_TRACKS }; /* version 4's chords and 5's chromatic, at the end */
static int f_read(uint32_t at, void *data, size_t n) { if (at + n > BYTES) return -1; memcpy(data, flash + at, n); return 0; }
static int f_write(uint32_t at, const void *data, size_t n) { if (at + n > BYTES) return -1; memcpy(flash + at, data, n); return 0; }
static int f_erase(uint32_t at) { if (at >= BYTES) return -1; memset(flash + at, 0xff, POOL_SECTOR); return 0; }
static const uint8_t *f_map(uint32_t at, size_t n) { return at + n <= BYTES ? flash + at : NULL; }
static const struct song_flash ram = {f_read, f_write, f_erase, f_map};
static struct song_pool pool;
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
/* The file's offset of track t's part: its count, then its events. */
static uint32_t part_at(const uint8_t *data, unsigned t)
{
    uint32_t at = 1 + 2 + sizeof sounds + 12 + 2 + 8 + 2;
    at += 1 + data[at] * 6U;               /* tempos */
    at += 1 + data[at] * 4U;               /* meters */
    at += 1 + data[at] * 12U;              /* markers */
    at += 9 + 10 + 32;                     /* loop, settings, knobs */
    at += 1 + data[at] * 5U;               /* glitch entries */
    at += 8 + 8 + 4;                       /* humanize, seed */
    for (unsigned k = 0; k < t; ++k) at += 2 + (data[at] | data[at + 1] << 8) * 8U;
    return at;
}
static void test_files(void)
{
    const uint8_t *data, *sound;
    uint32_t bytes, n, length;
    build_blocks();
    data = write_song(&song, &length);
    CHECK(data != NULL);
    memcpy(first, data, length);
    CHECK_EQ(first[0], SONG_FILE_VERSION);
    CHECK_EQ(song_file_read(&other, first, length, &sound, &n), 0);
    CHECK(other.events == song.events && !memcmp(other.event, song.event, song.events * sizeof song.event[0]));
    CHECK(!memcmp(other.count, song.count, sizeof song.count));
    unsigned blocks = 0;
    for (unsigned t = 0; t < SONG_TRACKS; ++t) blocks += blocks_on(&other, t);
    CHECK_EQ(blocks, 6);
    CHECK_EQ(song_file_crc(&other, sounds, sizeof sounds), song_file_crc(&song, sounds, sizeof sounds));
    data = write_song(&other, &bytes);
    CHECK(bytes == length && !memcmp(data, first, length));   /* back to the same bytes */
    /* As version 3, without the chord and chromatic bytes: it loads. Marked
     * version 2: refused, a block isn't a version 2 event. */
    first[0] = 3;
    CHECK_EQ(song_file_check(first, length - CHORD_BYTES, &sound, &n), 0);
    first[0] = 2;
    CHECK_EQ(song_file_check(first, length - CHORD_BYTES, &sound, &n), -1);
    /* Versions 2 and 1 of a song without blocks still load, and write back as the current version. */
    for (unsigned t = 0; t < SONG_TRACKS; ++t)
        for (unsigned i = song.count[t]; i-- > 0;) if (song_is_block(&song.event[song.first[t] + i])) song_remove(&song, t, i);
    data = write_song(&song, &length);
    memcpy(first, data, length);
    first[0] = 2;
    CHECK_EQ(song_file_read(&other, first, length - CHORD_BYTES, &sound, &n), 0);
    CHECK(other.events == song.events && !memcmp(other.event, song.event, song.events * sizeof song.event[0]));
    data = write_song(&other, &bytes);
    CHECK(data && data[0] == SONG_FILE_VERSION && bytes == length);
    memset(song.clip_length, 0, sizeof song.clip_length);
    for (unsigned p = SONG_TRACKS; p < SONG_PARTS; ++p) while (song.count[p]) song_remove(&song, p, 0);
    data = write_song(&song, &length);
    memcpy(first, data, length);
    first[0] = 1;
    CHECK_EQ(song_file_read(&other, first, length - 3U - CHORD_BYTES, &sound, &n), 0);
    CHECK(other.events == song.events && other.launch_q == 2);
    /* A block in a clip, one of no length, and one past the grid's rows: refused. */
    song_clear(&song, 4, 12000);
    song_block_put(&song, 3, 0, 1920, SONG_BLOCK_STOP);
    data = write_song(&song, &length);
    memcpy(first, data, length);
    CHECK_EQ(song_file_check(first, length, &sound, &n), 0);
    uint32_t e = part_at(first, 3) + 2;
    CHECK(first[e + 3] == SONG_FX && first[e + 4] == SONG_BLOCK_STOP);
    first[e + 4] = SONG_BLOCK | 15;
    CHECK_EQ(song_file_check(first, length, &sound, &n), 0);
    first[e + 4] = SONG_BLOCK | 16;
    CHECK_EQ(song_file_check(first, length, &sound, &n), -1);
    first[e + 4] = SONG_BLOCK_STOP;
    first[e + 6] = first[e + 7] = first[e + 5] = 0;
    CHECK_EQ(song_file_check(first, length, &sound, &n), -1);
    song_clear(&song, 4, 12000);
    song.clip_length[song_cell(0, 0)] = 480;
    const struct song_event stray = {SONG_FX << 24, SONG_BLOCK, 0, 480};
    song_add_event(&song, song_cell_part(song_cell(0, 0)), &stray);
    data = write_song(&song, &length);
    CHECK_EQ(song_file_check(data, length, &sound, &n), -1);
}

/* One copied track pasted onto another (0.5.7): its clip block lands as the
 * clip's notes, pass after pass, without the clip's controller. The track's
 * own note under the block stays behind, one sounding into it ends where it
 * starts, and a stop block leaves its span empty. Back on its own track the
 * block stays a block. */
static void test_cross_paste(void)
{
    static struct song_clip board;
    song_clear(&song, 16, 12000);
    unsigned cell = song_cell(1, 2), count;
    song.clip_length[cell] = 1920;
    clip_note(cell, 0, 50, 240);
    clip_note(cell, 960, 52, 240);
    const struct song_event cc = song_lane_event(480, 74, 90);
    song_add_event(&song, song_cell_part(cell), &cc);
    CHECK_EQ(song_block_put(&song, 1, 1920, 5760, 2), 0);   /* two passes */
    CHECK_EQ(song_block_put(&song, 1, 6400, 7000, SONG_BLOCK_STOP), 0);
    song_add_note(&song, 1, 960, 60, 100, 1920);           /* into the block */
    song_add_note(&song, 1, 2500, 70, 100, 100);           /* silent under it */
    song_add_note(&song, 1, 6000, 72, 100, 100);
    song_add_note(&song, 1, 6500, 74, 100, 100);           /* under the stop */
    CHECK_EQ(song_copy(&song, &board, 1U << 1, 0, 7680), 0);
    CHECK_EQ(song_paste(&song, &board, 0, 4, 0), 0);
    const struct song_event *e = song_track(&song, 4, &count);
    static const uint32_t want[6][3] = {{960, 60, 960}, {1920, 50, 240}, {2880, 52, 240}, {3840, 50, 240}, {4800, 52, 240}, {6000, 72, 100}};
    CHECK_EQ(count, 6);
    CHECK_EQ(blocks_on(&song, 4), 0);
    for (unsigned i = 0; i < 6 && i < count; ++i)
        CHECK(song_is_note(&e[i]) && song_event_start(&e[i]) == want[i][0] && e[i].a == want[i][1] && e[i].length == want[i][2]);
    CHECK_EQ(blocks_on(&song, 1), 2);                       /* the original is untouched */
    CHECK_EQ(song_paste(&song, &board, 7680, 1, 0), 0);     /* on its own track: blocks */
    CHECK_EQ(blocks_on(&song, 1), 4);
}

int main(void)
{
    test_edges();
    test_bar_edits();
    test_cross_paste();
    test_expand();
    test_files();
    return report("test_blocks");
}
