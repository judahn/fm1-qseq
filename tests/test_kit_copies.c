/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* The kit table's two copies (kits.c) under power loss: a kit written, a kit
 * deleted, and boot's remaking of a copy cut short or damaged, each cut off
 * after every byte written in turn. After each cut the device boots (a scan
 * and kit_tidy): the kits are the old ones or the new ones, every sample a
 * kit uses is there and no other, both copies are whole and alike, and
 * compacting the pool afterwards changes nothing (a copy left behind would
 * come back as the newest), on a full pool too. Also a table from before
 * the second copy, and a pool with no room for both. Built a second time
 * with TEST_FULL_LAYOUT. */
#include "corrupt_harness.h"

static uint8_t start[BYTES];
static struct song_pool start_pool;
struct state { unsigned n; uint16_t id[KIT_MAX]; char name[KIT_MAX][17]; struct kit kit[KIT_MAX]; };
static void state_of(struct state *s)
{
    memset(s, 0, sizeof *s);
    s->n = kit_count(&pool);
    for (unsigned i = 0; i < s->n; ++i) {
        s->id[i] = (uint16_t)kit_id_at(&pool, i);
        (void)kit_name(&pool, s->id[i], s->name[i]);
        (void)kit_read(&pool, s->id[i], &s->kit[i]);
    }
}
static int same(const struct state *x, const struct state *y) { return !memcmp(x, y, sizeof *x); }
static void boot(void) { scan(); (void)kit_tidy(&pool, "STOCK BANK"); }
/* Every kit's samples there, no sample no kit uses, both copies alike. */
static int sound(void)
{
    struct kit k;
    struct kit_sample s;
    for (unsigned i = 0, n = kit_count(&pool); i < n; ++i) {
        if (kit_read(&pool, kit_id_at(&pool, i), &k)) return 0;
        for (unsigned pad = 0; pad < KIT_PADS; ++pad)
            if (k.sample[pad] && kit_sample(&pool, k.sample[pad], &s)) return 0;
    }
    for (unsigned i = 0; i < pool.count; ++i)
        if (pool.extent[i].type == POOL_SAMPLE && !kit_refs(&pool, pool.extent[i].id)) return 0;
    return copies_alike();
}

/* `act` from the flash as it is now, cut off after every byte in turn, then
 * a boot. Afterwards the flash is as `act` left it uncut. */
static void sweep(const char *what, void (*act)(void))
{
    static struct state old, fresh, now, after;
    memcpy(start, flash, sizeof flash);
    start_pool = pool;
    state_of(&old);
    budget = 1L << 30;
    act();
    const long total = (1L << 30) - budget;
    budget = -1;
    boot();
    state_of(&fresh);
    CHECK(sound());
    unsigned olds = 0, news = 0;
    wrong = 0;
    for (long cut = 0; cut <= total; ++cut) {
        memcpy(flash, start, sizeof flash);
        pool = start_pool;
        budget = cut;
        act();
        budget = -1;
        boot();
        state_of(&now);
        olds += same(&now, &old);
        news += same(&now, &fresh);
        int ok = sound() && (same(&now, &old) || same(&now, &fresh));
        (void)pool_compact(&pool, POOL_MAX_SECTORS);
        scan();
        state_of(&after);
        if (!ok || !same(&after, &now) || !sound()) went_wrong(what, cut);
    }
    printf("kit table power loss, %s: %ld cuts, old kits %u, new kits %u, wrong %u\n", what, total + 1, olds, news, wrong);
    CHECK_EQ(wrong, 0);
    CHECK(news >= 1);
    memcpy(flash, start, sizeof flash);
    pool = start_pool;
    act();
    boot();
}
static unsigned three;
static struct kit extra;
static void write_extra(void) { (void)kit_write(&pool, 9, "EXTRA", &extra); }
static void delete_three(void) { (void)kit_delete(&pool, three); }
static void tidy(void) { (void)kit_tidy(&pool, "STOCK BANK"); }

static void test_power_loss(void)
{
    memset(flash, 0xff, sizeof flash);
    scan();
    unsigned one, two;
    uint32_t bytes = make_bank(0);
    CHECK_EQ(import(bank, bytes, bytes, &one), 0);
    CHECK_EQ(import(bank, bytes, bytes, &two), 0);         /* the same samples */
    bytes = make_bank(1);
    CHECK_EQ(import(bank, bytes, bytes, &three), 0);       /* one sample its own */
    CHECK(samples() == 4 && kit_count(&pool) == 3 && copies_alike());
    CHECK_EQ(kit_read(&pool, one, &extra), 0);
    sweep("a kit written", write_extra);
    CHECK_EQ(kit_count(&pool), 4);
    sweep("a kit deleted", delete_three);
    CHECK(kit_count(&pool) == 3 && samples() == 3);
    /* A write cut off between its copies: the first is new, the second old.
     * Boot makes the second again from the first, cut off anywhere too. */
    memcpy(start, flash, sizeof flash);
    start_pool = pool;
    unsigned split = 0;
    extra.sample[7] = extra.sample[0];
    for (long cut = 0; !split && cut < 20000; ++cut) {
        memcpy(flash, start, sizeof flash);
        pool = start_pool;
        budget = cut;
        (void)kit_write(&pool, 10, "SPLIT", &extra);
        budget = -1;
        scan();
        const struct pool_extent *a = pool_find(&pool, POOL_KIT, 1), *b = pool_find(&pool, POOL_KIT, 2);
        split = a && b && !pool_check(&pool, a) && !pool_check(&pool, b) && !copies_alike();
    }
    CHECK(split && kit_count(&pool) == 4);
    sweep("the second copy made again", tidy);
    CHECK(kit_count(&pool) == 4 && copies_alike());
    /* The newest copy damaged: the other is read, and made the newest again. */
    const struct pool_extent *b = pool_find(&pool, POOL_KIT, 2);
    flip(raw(pool_address(b)), 40);
    scan();
    CHECK(kit_count(&pool) == 4 && !copies_alike());
    sweep("a damaged copy made again", tidy);
    CHECK(kit_count(&pool) == 4 && copies_alike());
}

/* A write cut off between its copies, on a pool with no sector free: boot
 * can't make the second copy again in free space, so it drops the old one
 * first. Left there, compacting would later move it, make it the newest,
 * and bring the old kits back. */
static void test_full_split(void)
{
    struct state newest, now;
    struct kit k;
    memset(&k, 0, sizeof k);
    memset(flash, 0xff, sizeof flash);
    scan();
    CHECK_EQ(kit_write(&pool, 1, "ONE", &k), 0);
    memcpy(start, flash, sizeof flash);
    start_pool = pool;
    unsigned split = 0;
    for (long cut = 0; !split && cut < 20000; ++cut) {
        memcpy(flash, start, sizeof flash);
        pool = start_pool;
        budget = cut;
        (void)kit_write(&pool, 2, "TWO", &k);
        budget = -1;
        scan();
        split = pool_find(&pool, POOL_KIT, 1) && pool_find(&pool, POOL_KIT, 2) && kit_count(&pool) == 2 && !copies_alike();
    }
    CHECK(split);
    static struct song small;
    song_clear(&small, 2, 12000);
    for (unsigned id = 1; id < 200 && !save(&small, id); ++id) {}
    CHECK_EQ(pool_free(&pool), 0);
    state_of(&newest);
    boot();
    CHECK(kit_count(&pool) == 2 && copies_alike());
    for (unsigned i = 0; i < pool.count;)                  /* the songs go, and the pool is compacted */
        if (pool.extent[i].type == POOL_SONG && !pool_delete(&pool, POOL_SONG, pool.extent[i].id)) continue;
        else ++i;
    (void)pool_compact(&pool, POOL_MAX_SECTORS);
    scan();
    state_of(&now);
    CHECK(same(&now, &newest) && copies_alike());
}

/* A table written before there were two copies, as either id: read, and the
 * other copy made at boot. */
static void test_one_copy(void)
{
    struct state before, now;
    for (unsigned missing = 1; missing <= 2; ++missing) {
        memset(flash, 0xff, sizeof flash);
        scan();
        unsigned id;
        const uint32_t bytes = make_bank(0);
        CHECK_EQ(import(bank, bytes, bytes, &id), 0);
        state_of(&before);
        CHECK_EQ(pool_delete(&pool, POOL_KIT, missing), 0);
        scan();
        state_of(&now);
        CHECK(same(&now, &before) && !copies_alike());
        boot();
        state_of(&now);
        CHECK(same(&now, &before) && sound());
    }
}

/* Room for both copies, a sector each, before either is written. */
static void test_room(void)
{
    struct kit k;
    memset(&k, 0, sizeof k);
    memset(flash, 0xff, sizeof flash);
    pool_scan(&pool, &ram, POOL_FIRST, POOL_FIRST + 1U, SKIP, SKIP_END);
    CHECK(kit_write(&pool, 1, "ONE", &k) != 0);           /* one sector: nothing written */
    unsigned blank = 1;
    for (unsigned i = 0; i < POOL_SECTOR; ++i) blank &= *raw(POOL_FIRST * POOL_SECTOR + i) == 0xff;
    CHECK(blank && !pool_find(&pool, POOL_KIT, 1) && !pool_find(&pool, POOL_KIT, 2));
    pool_scan(&pool, &ram, POOL_FIRST, POOL_FIRST + 2U, SKIP, SKIP_END);
    CHECK_EQ(kit_write(&pool, 1, "ONE", &k), 0);          /* two: one write, and no room to replace it */
    CHECK(kit_write(&pool, 2, "TWO", &k) != 0);
    CHECK(kit_count(&pool) == 1 && copies_alike());
    pool_scan(&pool, &ram, POOL_FIRST, POOL_FIRST + 3U, SKIP, SKIP_END);
    unsigned failed = 0;
    for (unsigned id = 2; id <= 20; ++id) failed += kit_write(&pool, id, "MORE", &k) != 0;
    CHECK(!failed && kit_count(&pool) == 20 && copies_alike());
}

int main(void)
{
    test_power_loss();
    test_full_split();
    test_one_copy();
    test_room();
#ifdef TEST_FULL_LAYOUT
    return report("test_kit_copies_full");
#else
    return report("test_kit_copies");
#endif
}
