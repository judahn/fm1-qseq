/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Shared by the corrupt-data tests (test_corrupt.c, test_corrupt_kits.c)
 * and the kit table's power-loss test (test_kit_copies.c): a
 * RAM flash and pool (ram_flash.h, so they run on the full device build's
 * layout too), a seeded random, a song with something of everything a file
 * holds (clip blocks too), the device's load path, what a loaded song promises, and a stock
 * bank with the transfer path's import. */
#ifndef FM1_TEST_CORRUPT_HARNESS_H
#define FM1_TEST_CORRUPT_HARNESS_H
#include "harness.h"
#include "pool.h"
#include "song_file.h"
#include "kits.h"
#include "clips.h"
#include "blocks.h"
#include "glitch.h"
#include "lanes.h"
#include <stddef.h>
#include <stdlib.h>

static uint32_t seed = 0x5eed7a11U;
static uint32_t rnd(void) { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return seed; }

#include "ram_flash.h" /* the stock layout, or with TEST_FULL_LAYOUT the full device build's */
enum { LEGACY = 17 };         /* where the stock firmware kept its sample bank */
static uint8_t good[BYTES];
static void flip(uint8_t *at, unsigned bit) { at[bit / 8U] ^= (uint8_t)(1U << bit % 8U); }
/* The same place in `good` as raw(at) is in the flash. */
static uint8_t *good_at(uint32_t at) { return good + (raw(at) - flash); }

static struct song song, target, before;
static uint8_t sounds[64];
static unsigned wrong; /* across a test: damage that went wrong, the first one printed */
static void went_wrong(const char *what, long where)
{
    if (!wrong++) printf("%s %ld goes wrong\n", what, where);
}

/* A song with something of everything a file holds, clips included. */
static void build(struct song *s, unsigned variant)
{
    song_clear(s, 12, 12000);
    for (unsigned t = 0; t < SONG_TRACKS; ++t)
        for (unsigned i = 0; i < 12; ++i) song_add_note(s, t, i * 240U + t, 40 + (i * 5 + t + variant) % 50, 90, 120);
    const struct song_event cc = song_lane_event(480, 74, 33), program = song_lane_event(960, SONG_LANE_PROGRAM, 5);
    song_add_event(s, 2, &cc); song_add_event(s, 2, &program);
    song_fx_add(s, 1920, 240, 1, 2);
    song_set_tempo(s, 3840, 9000);
    song_set_meter(s, 4, 3, 4);
    song_add_marker(s, 1920, "VERSE");
    s->glitch[1] = song_glitch_turn(0, GLITCH_REPEATS, 2);
    s->event[s->first[0] + 3].at |= 1U << SONG_GLITCH_SHIFT;
    s->humanize_time[3] = 7; s->seed = 99 + variant; s->knob_cc[1][2] = 74; s->swing = 20;
    memcpy(s->name, "CORRUPT     ", 12);
    const unsigned cells[3] = {song_cell(0, 0), song_cell(3, 5), song_cell(7, 15)};
    for (unsigned i = 0; i < 3; ++i) {
        s->clip_length[cells[i]] = (uint16_t)(480U << i);
        for (unsigned k = 0; k < 4; ++k) song_add_note(s, song_cell_part(cells[i]), k * 120U, 60 + k, 100, 60);
    }
    const struct song_event clip_cc = song_lane_event(0, 7, 90);
    song_add_event(s, song_cell_part(cells[1]), &clip_cc);
    memcpy(s->row_name[5], "CHORUS", 6);
    s->launch_q = 3;
    song_block_put(s, 3, 1920, 5760, 5);      /* clips in the song (phase 7b): a block, and a stop */
    song_block_put(s, 0, 3840, 4800, SONG_BLOCK_STOP);
}
static int save(const struct song *s, unsigned id)
{
    struct pool_writer w;
    if (pool_begin(&w, &pool, POOL_SONG, id, "SONG", song_file_bytes(s, sizeof sounds))) return w.error;
    song_file_write(&w, s, sounds, sizeof sounds);
    return pool_end(&w);
}
/* The device's load: the extent, its payload CRC, then the payload's check. */
static int load(unsigned id, struct song *into)
{
    const struct pool_extent *e = pool_find(&pool, POOL_SONG, id);
    const uint8_t *block = NULL;
    uint32_t n = 0;
    if (!e || pool_check(&pool, e)) return -1;
    return song_file_read(into, f_map(pool_address(e), e->length), e->length, &block, &n);
}

/* What a loaded song promises: parts back to back and in clock order, notes
 * in range, the maps in order, and clip events only where a cell has a length. */
static int whole(const struct song *s)
{
    unsigned at = 0;
    for (unsigned p = 0; p < SONG_PARTS; ++p) {
        if (s->first[p] != at) return 0;
        for (unsigned i = 0; i < s->count[p]; ++i) {
            const struct song_event *e = &s->event[at + i];
            if ((i && song_event_start(e) < song_event_start(e - 1)) || (song_is_note(e) && (e->a > 127 || !e->length))) return 0;
        }
        if (p >= SONG_TRACKS && s->count[p] && !s->clip_length[p - SONG_TRACKS]) return 0;
        at += s->count[p];
    }
    if (at != s->events || at > SONG_EVENTS || !s->bars || s->bars > SONG_MAX_BARS || s->open || s->launch_q > 4) return 0;
    if (!s->tempos || s->tempos > SONG_TEMPOS || s->tempo[0].clock || !s->meters || s->meters > SONG_METERS || s->meter[0].bar ||
        s->markers > SONG_MARKERS || s->loop_start > s->loop_end) return 0;
    for (unsigned k = 0; k < s->tempos; ++k)
        if ((k && s->tempo[k].clock <= s->tempo[k - 1].clock) || s->tempo[k].bpm100 < SONG_TEMPO_MIN || s->tempo[k].bpm100 > SONG_TEMPO_MAX) return 0;
    for (unsigned m = 0; m < s->meters; ++m)
        if ((m && s->meter[m].bar <= s->meter[m - 1].bar) || !s->meter[m].beats || s->meter[m].beats > 16 ||
            (s->meter[m].unit != 4 && s->meter[m].unit != 8 && s->meter[m].unit != 16)) return 0;
    return 1;
}
/* A stock-format bank in RAM: 3 samples of whole chunks. */
static uint8_t bank[KIT_BANK_HEADER + 30 * KIT_CHUNK];
static uint32_t make_bank(unsigned variant)
{
    static const uint16_t chunks[3] = {10, 6, 14};
    memset(bank, 0, sizeof bank);
    uint32_t *h = (uint32_t *)(void *)bank, at = KIT_BANK_HEADER;
    h[0] = 0x53314d46U; h[1] = 1; h[5] = 32000;
    for (unsigned i = 0; i < 3; ++i) {
        uint32_t *e = h + 8 + i * 7;
        e[0] = at; e[1] = chunks[i] * 320U - 7U; e[2] = chunks[i] * KIT_CHUNK;
        memcpy(e + 3, i ? "SNARE" : "KICK", 5);
        for (unsigned c = 0; c < chunks[i]; ++c) {
            uint8_t *p = bank + at + c * KIT_CHUNK;
            p[0] = 1; p[1] = 0x45; p[2] = 20; p[3] = 0x5a;
            for (unsigned k = 4; k < KIT_CHUNK; ++k) p[k] = (uint8_t)(k * (i + 3) + c + (i == 2 ? variant : 0));
        }
        at += chunks[i] * KIT_CHUNK;
    }
    for (unsigned i = 3; i < 16; ++i) h[8 + i * 7] = at;
    h[2] = at;
    h[3] = ~pool_crc(~0U, bank, at); /* crc and commit read as zero while they are zero */
    return at;
}
/* As the transfer path does it: the bank in 128-byte writes declared as
 * `total`, a refused write cancelling, and the finish checking the CRC. */
static int import(const uint8_t *data, uint32_t bytes, uint32_t total, unsigned *id)
{
    static struct kit_import k;
    if (kit_import_begin(&k, total)) return -1;
    for (uint32_t at = 0; at < bytes; at += 128)
        if (kit_import_write(&k, &pool, at, data + at, bytes - at < 128 ? bytes - at : 128)) { kit_import_cancel(&k, &pool); return -1; }
    return kit_import_finish(&k, &pool, "KIT", id);
}
static unsigned samples(void)
{
    unsigned n = 0;
    for (unsigned i = 0; i < pool.count; ++i) n += pool.extent[i].type == POOL_SAMPLE;
    return n;
}
/* Both copies of the kit table there, whole and alike. */
static int copies_alike(void)
{
    const struct pool_extent *a = pool_find(&pool, POOL_KIT, 1), *b = pool_find(&pool, POOL_KIT, 2);
    return a && b && !pool_check(&pool, a) && !pool_check(&pool, b) && a->length == b->length &&
           !memcmp(f_map(pool_address(a), a->length), f_map(pool_address(b), b->length), a->length);
}

#endif
