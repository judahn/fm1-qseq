/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Storage: songs round trip byte for byte, a save cut off at every byte
 * leaves a good copy, allocation and compaction, kits sharing samples,
 * a stock bank loading as a kit and reading back the same, and the
 * recovery copy's timing. Flash here is RAM that behaves like NOR: bits
 * only clear, erase sets a sector, and power can go at any byte. Built a
 * second time with TEST_FULL_LAYOUT (test_store_full), it all runs on the
 * full device build's pool through song_flash.h's map (ram_flash.h). */
#include "harness.h"
#include "pool.h"
#include "song_file.h"
#include "kits.h"
#include "glitch.h"
#include "lanes.h"

#include "ram_flash.h" /* the stock layout, or with TEST_FULL_LAYOUT the full device build's */

static uint8_t snapshot[BYTES];
static struct song a, b;
static uint8_t sounds[1998];
static void build(struct song *s, unsigned seed)
{
    song_clear(s, 12, 12000 + seed);
    for (unsigned t = 0; t < SONG_TRACKS; ++t)
        for (unsigned i = 0; i < 40 + t * 5; ++i) song_add_note(s, t, i * 120U + t, 40 + (i * 7 + seed) % 60, 90, 100);
    const struct song_event cc = song_lane_event(480, 74, 33), program = song_lane_event(960, SONG_LANE_PROGRAM, 5);
    song_add_event(s, 2, &cc); song_add_event(s, 2, &program);
    song_fx_add(s, 1920, 240, 1, 2);
    song_set_tempo(s, 3840, 9000);
    song_set_meter(s, 4, 3, 4);
    song_add_marker(s, 1920, "VERSE");
    s->glitch[1] = 0x123; s->glitch[5] = 0x45;
    s->humanize_time[3] = 7; s->seed = 99 + seed; s->knob_cc[1][2] = 74;
    memcpy(s->name, "ROUND TRIP  ", 12);
    s->kit = 3; s->base_program[0] = 12;
    s->loop_on = 1; s->swing = 20;
}
static int save(const struct song *s, unsigned id)
{
    struct pool_writer w;
    if (pool_begin(&w, &pool, POOL_SONG, id, "SONG", song_file_bytes(s, sizeof sounds))) return w.error;
    song_file_write(&w, s, sounds, sizeof sounds);
    return pool_end(&w);
}
/* Loads a song from the pool into `into`. Returns the stored CRC, or 0. */
static uint32_t load(unsigned id, struct song *into)
{
    const struct pool_extent *e = pool_find(&pool, POOL_SONG, id);
    const uint8_t *s = NULL;
    uint32_t n = 0;
    if (!e || pool_check(&pool, e) || song_file_read(into, f_map(pool_address(e), e->length), e->length, &s, &n)) return 0;
    return n == sizeof sounds && !memcmp(s, sounds, n) ? song_file_crc(into, sounds, n) : 0;
}

static void test_round_trip(void)
{
    memset(flash, 0xff, sizeof flash);
    for (unsigned i = 0; i < sizeof sounds; ++i) sounds[i] = (uint8_t)(i * 31);
    scan();
    build(&a, 0);
    CHECK_EQ(save(&a, 1), 0);
    uint32_t crc = song_file_crc(&a, sounds, sizeof sounds);
    scan();
    CHECK_EQ(pool.count, 1);
    CHECK_EQ(load(1, &b), crc);
    /* Byte-exact: the reloaded song, saved again, writes the very same bytes. */
    CHECK_EQ(save(&b, 2), 0);
    const struct pool_extent *e = pool_find(&pool, POOL_SONG, 1), *e2 = pool_find(&pool, POOL_SONG, 2);
    CHECK(e && e2 && e->length == e2->length && e->length == song_file_bytes(&a, sizeof sounds));
    CHECK(!memcmp(f_map(pool_address(e), e->length), f_map(pool_address(e2), e->length), e->length));
    CHECK(b.events == a.events && !memcmp(b.event, a.event, a.events * sizeof a.event[0]));
    CHECK(!memcmp(b.first, a.first, sizeof a.first) && !memcmp(b.count, a.count, sizeof a.count));
    CHECK(b.tempos == 2 && b.tempo[1].bpm100 == 9000 && b.meters == 2 && b.markers == 1 && b.glitch[5] == 0x45);
    CHECK(!memcmp(b.name, a.name, 12) && b.kit == 3 && b.base_program[0] == 12 && b.seed == a.seed && b.loop_on);
    CHECK(b.tempo_units[1] == a.tempo_units[1]);
    /* A bad payload changes nothing. */
    struct song keep = b;
    const uint8_t *s = NULL;
    uint32_t n = 0;
    static uint8_t broken[65536];
    memcpy(broken, f_map(pool_address(e), e->length), e->length);
    broken[1 + 2 + sizeof sounds + 12 + 2 + 8] = 0; /* bars 0 */
    broken[1 + 2 + sizeof sounds + 12 + 2 + 9] = 0;
    CHECK_EQ(song_file_read(&b, broken, e->length, &s, &n), -1);
    CHECK(!memcmp(&keep, &b, sizeof b));
    CHECK_EQ(song_file_read(&b, f_map(pool_address(e), e->length), e->length - 1, &s, &n), -1);
    /* Phase 5b: a payload from a computer, checked alone. Meters must rise
     * and settings stay in range (a has 2 tempos, 2 meters, 1 marker). */
    const uint32_t meters = 1 + 2 + sizeof sounds + 12 + 2 + 8 + 2 + 1 + 2 * 6, settings = meters + 1 + 2 * 4 + 1 + 12 + 9;
    memcpy(broken, f_map(pool_address(e), e->length), e->length);
    CHECK_EQ(song_file_check(broken, e->length, &s, &n), 0);
    CHECK(s == broken + 3 && n == sizeof sounds && broken[meters] == 2 && broken[meters + 5] == a.meter[1].bar && broken[settings] == a.swing);
    broken[meters + 5] = 0, broken[meters + 6] = 0; /* the second meter on bar 0 too */
    CHECK_EQ(song_file_check(broken, e->length, &s, &n), -1);
    memcpy(broken, f_map(pool_address(e), e->length), e->length);
    broken[settings] = 51; /* swing over 50 */
    CHECK_EQ(song_file_check(broken, e->length, &s, &n), -1);
}

/* Song A is saved. Saving A' is cut off after every byte in turn. After
 * each cut the pool is scanned again: the song must load as A or as A'. */
static void test_power_loss(void)
{
    memset(flash, 0xff, sizeof flash);
    scan();
    build(&a, 0);
    build(&b, 1);
    CHECK_EQ(save(&a, 1), 0);
    uint32_t old = song_file_crc(&a, sounds, sizeof sounds), new = song_file_crc(&b, sounds, sizeof sounds);
    memcpy(snapshot, flash, sizeof flash);
    struct song_pool before = pool;
    budget = 1L << 30;
    CHECK_EQ(save(&b, 1), 0);
    long total = (1L << 30) - budget;
    unsigned olds = 0, news = 0, bad = 0;
    static struct song loaded;
    for (long cut = 0; cut <= total + 1; ++cut) {
        memcpy(flash, snapshot, sizeof flash);
        pool = before;
        budget = cut;
        (void)save(&b, 1);
        budget = -1;
        scan();
        uint32_t got = load(1, &loaded);
        olds += got == old;
        news += got == new;
        bad += got != old && got != new;
    }
    CHECK_EQ(bad, 0);
    CHECK(olds > 1000 && news >= 1);
    printf("power loss: %ld cuts, old copy %u, new copy %u, corrupt %u\n", total + 2, olds, news, bad);
}

static void test_pool(void)
{
    memset(flash, 0xff, sizeof flash);
    scan();
    CHECK_EQ(pool_free(&pool), POOL_SECTORS);
    /* Small songs, then every other one deleted: plenty free, no long run. */
    static struct song small;
    song_clear(&small, 2, 12000);
    for (unsigned id = 1; id <= POOL_SECTORS; ++id) {
        small.seed = id;
        CHECK_EQ(save(&small, id), 0);
    }
    CHECK_EQ(pool_free(&pool), 0);
    unsigned per = pool_find(&pool, POOL_SONG, 1)->sectors;
    for (unsigned id = 1; id <= POOL_SECTORS; id += 2) CHECK_EQ(pool_delete(&pool, POOL_SONG, id), 0);
    build(&a, 3);
    unsigned need = pool_sectors(song_file_bytes(&a, sizeof sounds));
    CHECK(pool_free(&pool) >= need);
    CHECK(pool_largest(&pool) < need);
    CHECK(save(&a, 99) != 0);
    CHECK_EQ(pool_compact(&pool, need), 0);
    CHECK_EQ(save(&a, 99), 0);
    scan();
    unsigned intact = 0;
    for (unsigned id = 2; id <= POOL_SECTORS; id += 2) {
        const struct pool_extent *e = pool_find(&pool, POOL_SONG, id);
        intact += e && !pool_check(&pool, e) && e->sectors == per;
    }
    CHECK_EQ(intact, POOL_SECTORS / 2);
    CHECK(load(99, &b) == song_file_crc(&a, sounds, sizeof sounds));
}

/* A stock-format bank in RAM: 3 samples of whole chunks. */
static uint8_t bank[KIT_BANK_HEADER + 40 * KIT_CHUNK];
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
    h[3] = ~pool_crc(~0U, bank, at); /* crc and commit read as zero while it is zero */
    return at;
}
static int import(uint32_t bytes, unsigned *id)
{
    static struct kit_import k;
    CHECK_EQ(kit_import_begin(&k, bytes), 0);
    for (uint32_t at = 0; at < bytes; at += 128)
        if (kit_import_write(&k, &pool, at, bank + at, bytes - at < 128 ? bytes - at : 128)) return -1;
    return kit_import_finish(&k, &pool, "KIT", id);
}
static void test_kits(void)
{
    memset(flash, 0xff, sizeof flash);
    scan();
    uint32_t bytes = make_bank(0);
    unsigned one, two, three;
    CHECK_EQ(import(bytes, &one), 0);
    CHECK_EQ(pool_used(&pool, POOL_SAMPLE) > 0, 1);
    unsigned samples = 0;
    for (unsigned i = 0; i < pool.count; ++i) samples += pool.extent[i].type == POOL_SAMPLE;
    CHECK_EQ(samples, 3);
    /* The same bank again: a second kit, no new samples. One changed sample: one more. */
    CHECK_EQ(import(bytes, &two), 0);
    make_bank(1);
    CHECK_EQ(import(bytes, &three), 0);
    unsigned now = 0;
    for (unsigned i = 0; i < pool.count; ++i) now += pool.extent[i].type == POOL_SAMPLE;
    CHECK_EQ(now, 4);
    struct kit k1, k2;
    CHECK(!kit_read(&pool, one, &k1) && !kit_read(&pool, two, &k2) && !memcmp(k1.sample, k2.sample, sizeof k1.sample));
    struct kit_sample s;
    CHECK(!kit_sample(&pool, k1.sample[1], &s) && s.frames == 6 * 320U - 7U && !strcmp(s.name, "SNARE"));
    /* The kit table is one object: a write cut off anywhere keeps every old kit. */
    memcpy(snapshot, flash, sizeof flash);
    struct song_pool keep = pool;
    struct kit extra = k1;
    budget = 1L << 30;
    CHECK_EQ(kit_write(&pool, 9, "EXTRA", &extra), 0);
    long total = (1L << 30) - budget;
    unsigned lost = 0, olds = 0;
    for (long cut = 0; cut <= total; ++cut) {
        memcpy(flash, snapshot, sizeof flash);
        pool = keep;
        budget = cut;
        (void)kit_write(&pool, 9, "EXTRA", &extra);
        budget = -1;
        scan();
        struct kit check;
        lost += kit_read(&pool, one, &check) || kit_read(&pool, two, &check) || kit_read(&pool, three, &check);
        olds += kit_count(&pool) == 3;
    }
    CHECK_EQ(lost, 0);
    CHECK(olds > 100 && kit_count(&pool) == 4);
    CHECK_EQ(kit_delete(&pool, 9), 0);
    /* Read back as a bank: byte for byte what came in. */
    make_bank(0);
    uint32_t crc = kit_bank_crc(&pool, &k1);
    CHECK_EQ(kit_bank_bytes(&pool, &k1), bytes);
    static uint8_t out[sizeof bank];
    CHECK_EQ(kit_bank_read(&pool, &k1, crc, 0, out, bytes), 0);
    CHECK(!memcmp(out, bank, bytes));
    /* Deleting: shared samples stay until the last kit using them goes. */
    CHECK_EQ(kit_delete(&pool, one), 0);
    CHECK_EQ(kit_sample(&pool, k1.sample[0], &s), 0);
    CHECK_EQ(kit_delete(&pool, two), 0);
    CHECK_EQ(kit_delete(&pool, three), 0);
    CHECK_EQ(pool_used(&pool, POOL_SAMPLE), 0);
    /* A bad CRC: nothing stays. */
    make_bank(0);
    bank[KIT_BANK_HEADER + 9] ^= 1;
    CHECK(import(bytes, &one) != 0);
    CHECK_EQ(pool_used(&pool, POOL_SAMPLE) + pool_used(&pool, POOL_KIT), 0);
}

/* A stock bank where the stock firmware kept it becomes a kit in place. The
 * full device build has none: its first sectors are code there. */
static void test_stock_bank(void)
{
    memset(flash, 0xff, sizeof flash);
    uint32_t bytes = make_bank(0);
#ifdef TEST_FULL_LAYOUT
    CHECK(raw(17 * POOL_SECTOR) == NULL && f_write(17 * POOL_SECTOR, bank, bytes) != 0);
    return;
#endif
    memcpy(raw(17 * POOL_SECTOR), bank, bytes);
    scan();
    CHECK(pool_find(&pool, POOL_LEGACY, 0) != NULL);
    unsigned id = kit_tidy(&pool, "STOCK BANK");
    CHECK(id > 0);
    struct kit k;
    CHECK_EQ(kit_read(&pool, id, &k), 0);
    CHECK(k.sample[0] == KIT_LEGACY && k.sample[2] == KIT_LEGACY + 2 && !k.sample[3]);
    struct kit_sample s;
    CHECK(!kit_sample(&pool, KIT_LEGACY + 2, &s) && s.data == raw(17 * POOL_SECTOR) + KIT_BANK_HEADER + 16 * KIT_CHUNK);
    static uint8_t out[sizeof bank];
    CHECK_EQ(kit_bank_read(&pool, &k, kit_bank_crc(&pool, &k), 0, out, bytes), 0);
    CHECK(!memcmp(out, bank, bytes));
    scan(); /* tidying again makes no second kit */
    CHECK_EQ(kit_tidy(&pool, "STOCK BANK"), 0);
    /* Importing a bank with the same samples points at the stock ones. */
    unsigned again;
    CHECK_EQ(import(bytes, &again), 0);
    CHECK_EQ(pool_used(&pool, POOL_SAMPLE), 0);
}

static void test_recovery(void)
{
    struct song_recovery r = {0};
    CHECK_EQ(song_recovery_due(&r, 5, 1000, 10000), 1);
    r.written_crc = 5; r.written_at = 1000; r.any = 1;
    CHECK_EQ(song_recovery_due(&r, 5, 2000, 10000), 0); /* nothing new */
    CHECK_EQ(song_recovery_due(&r, 6, 3000, 10000), 0); /* too soon */
    CHECK_EQ(r.pending, 1);
    CHECK_EQ(song_recovery_due(&r, 6, 11500, 10000), 1);
    r.saved_crc = 7;
    CHECK_EQ(song_recovery_due(&r, 7, 30000, 10000), 0); /* saved as it is */
    /* Each write lands somewhere new: the wear moves around. */
    memset(flash, 0xff, sizeof flash);
    scan();
    static struct song small;
    song_clear(&small, 2, 12000);
    unsigned seen[POOL_END] = {0}, places = 0;
    for (unsigned i = 0; i < 30; ++i) {
        small.seed = i;
        struct pool_writer w;
        CHECK_EQ(pool_begin(&w, &pool, POOL_RECOVERY, 1, "REC", song_file_bytes(&small, 0)), 0);
        song_file_write(&w, &small, NULL, 0);
        CHECK_EQ(pool_end(&w), 0);
        places += !seen[pool_find(&pool, POOL_RECOVERY, 1)->sector]++;
    }
    CHECK(places >= 20);
    CHECK_EQ(pool.count, 1);
}

int main(void)
{
    test_round_trip();
    test_power_loss();
    test_pool();
    test_kits();
    test_stock_bank();
    test_recovery();
#ifdef TEST_FULL_LAYOUT
    return report("test_store_full");
#else
    return report("test_store");
#endif
}
