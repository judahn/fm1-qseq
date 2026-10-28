/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Corrupt data in storage: song payloads and the pool directory fed
 * truncated, bit-flipped and scrambled bytes (test_corrupt_kits.c does the
 * kit table and stock banks). Every load must either refuse with an error
 * and leave things as they were, or, where the damage still makes valid
 * data, load something whole. Never a crash or a sanitizer report, and the
 * good copies elsewhere in the pool still load. The random choices are
 * seeded, so a failure repeats, and the first of each kind is printed with
 * its cause. The power-loss test in test_store.c covers writes cut short.
 * This covers data gone bad. */
#include "corrupt_harness.h"

/* A damaged song that still loaded plays: from the top, row 1's clips launched. */
static void quiet(void *context, unsigned track, unsigned pitch, unsigned velocity) { (void)context; (void)track; (void)pitch; (void)velocity; }
static void play(struct song *s)
{
    static struct song_player p;
    static const struct song_output silent = {.note = quiet};
    song_player_init(&p, s, &silent);
    for (unsigned t = 0; t < SONG_TRACKS; ++t) song_player_launch(&p, t, 0);
    song_player_start(&p, 0);
    for (uint32_t b = 0; b < 400; ++b) song_player_advance(&p, (uint64_t)(b + 1U) * BLOCK);
    song_player_stop(&p);
}

/* One damaged payload, copied to a buffer of exactly its size so any read
 * past it is caught: refused with the song as it was, or loaded whole. */
static unsigned refused, loaded, played;
static void payload(const uint8_t *bytes, uint32_t n, const char *what, long where)
{
    uint8_t *copy = malloc(n ? n : 1);
    memcpy(copy, bytes, n);
    const uint8_t *block = NULL;
    uint32_t block_bytes = 0;
    int checked = song_file_check(copy, n, &block, &block_bytes);
    memcpy(&before, &target, sizeof target);
    int r = song_file_read(&target, copy, n, &block, &block_bytes);
    unsigned bad = r != checked;
    if (r) { ++refused; bad |= memcmp(&before, &target, sizeof target) != 0; }
    else {
        ++loaded;
        bad |= !whole(&target) || block < copy || block + block_bytes > copy + n;
        if (!bad && loaded % 8U == 0) { play(&target); ++played; }
    }
    if (bad) went_wrong(what, where);
    free(copy);
}
static void test_song_payloads(void)
{
    static uint8_t bytes[16384], damaged[16384];
    memset(flash, 0xff, sizeof flash);
    scan();
    for (unsigned i = 0; i < sizeof sounds; ++i) sounds[i] = (uint8_t)(i * 7);
    build(&song, 0);
    CHECK_EQ(save(&song, 1), 0);
    const struct pool_extent *e = pool_find(&pool, POOL_SONG, 1);
    const uint32_t n = e->length;
    memcpy(bytes, f_map(pool_address(e), n), n);
    song_clear(&target, 4, 12000);
    wrong = 0;
    payload(bytes, n, "the good payload", 0);
    CHECK(loaded == 1 && target.clip_length[song_cell(3, 5)] == 960);
    refused = loaded = 0;
    for (uint32_t cut = 0; cut < n; ++cut) payload(bytes, cut, "song payload truncated to", cut);
    CHECK_EQ(refused, n);                                  /* every truncation refused */
    const unsigned truncations = refused;
    refused = loaded = played = 0;
    for (uint32_t bit = 0; bit < n * 8U; ++bit) {          /* every single bit flipped */
        memcpy(damaged, bytes, n);
        flip(damaged, bit);
        payload(damaged, n, "song payload with bit flipped:", (long)bit);
    }
    CHECK(refused > 0 && loaded > 0);
    const unsigned flips = refused + loaded, flips_refused = refused;
    for (unsigned k = 0; k < 3000; ++k) {                  /* spans of random bytes, and random files */
        uint32_t length = n, at = rnd() % n, span = 1U + rnd() % 64U;
        memcpy(damaged, bytes, n);
        if (k % 3U == 2U) { length = rnd() % (uint32_t)sizeof damaged; at = 0; span = length; }
        for (uint32_t i = at; i < at + span && i < length; ++i) damaged[i] = (uint8_t)rnd();
        if (k % 3U == 1U) damaged[0] = (uint8_t)(1U + rnd() % 2U); /* a version it takes */
        payload(damaged, length, "song payload scrambled, try", k);
    }
    CHECK_EQ(wrong, 0);
    printf("song payloads: %u truncations all refused, %u bit flips (%u refused, the rest loaded whole), "
           "3000 scrambled, %u damaged songs played\n", truncations, flips, flips_refused, played);
}

/* The pool's directory with damaged headers, payloads and stray sectors: the
 * damaged object goes or still checks out whole, and every other one loads.
 * The kit is always there: its table is kept twice. */
static int others_load(unsigned skip_song)
{
    static struct song into;
    int ok = 1;
    for (unsigned id = 1; id <= 3; ++id)
        if (id != skip_song) ok &= !load(id, &into) && whole(&into);
    struct kit k;
    return ok && kit_count(&pool) == 1 && !kit_read(&pool, 1, &k);
}
static void test_pool_directory(void)
{
    memset(flash, 0xff, sizeof flash);
    scan();
    for (unsigned id = 1; id <= 3; ++id) { build(&song, id); CHECK_EQ(save(&song, id), 0); }
    unsigned kit;
    const uint32_t bytes = make_bank(0);
    CHECK_EQ(import(bank, bytes, bytes, &kit), 0);
    memcpy(good, flash, sizeof flash);
    scan();
    CHECK(others_load(0));
    const struct song_pool clean = pool;
    wrong = 0;
    unsigned flips = 0, dropped = 0;
    for (unsigned i = 0; i < clean.count; ++i)            /* every bit of every header */
        for (unsigned bit = 0; bit < POOL_HEADER * 8U; ++bit, ++flips) {
            const struct pool_extent *e = &clean.extent[i];
            flip(raw(e->sector * POOL_SECTOR), bit);
            scan();
            const struct pool_extent *now = pool_find(&pool, e->type, e->id);
            dropped += !now;
            int ok = !now || (now->sector == e->sector && now->sectors == e->sectors && now->length == e->length);
            ok &= others_load(e->type == POOL_SONG ? e->id : 0);
            if (!ok) went_wrong("pool header bit flipped (extent, bit):", (long)(i * 1000U + bit));
            flip(raw(e->sector * POOL_SECTOR), bit);
        }
    /* Song 2's payload: a bit gone bad, or its end erased. The payload CRC
     * refuses it, and everything else loads. */
    const struct pool_extent *two = pool_find(&clean, POOL_SONG, 2);
    unsigned hits = 0, refused_two = 0;
    for (unsigned k = 0; k < 400; ++k) {
        uint32_t at = pool_address(two) + (k < 200 ? rnd() % two->length : two->length - 1U - (k - 200U));
        uint8_t keep = *raw(at);
        *raw(at) = k < 200 ? (uint8_t)(keep ^ 1U << rnd() % 8U) : 0xff;
        if (*raw(at) != keep) {
            ++hits;
            scan();
            refused_two += load(2, &target) != 0;
            if (!others_load(2)) went_wrong("song 2's payload damaged at", (long)at);
        }
        *raw(at) = keep;
    }
    CHECK_EQ(refused_two, hits);
    /* Stray sectors: random bytes in free ones, and headers with a good CRC
     * but a type, version or length out of range. None becomes an object. */
    uint8_t used[POOL_END] = {0};
    for (unsigned i = 0; i < clean.count; ++i)
        for (unsigned k = 0; k < clean.extent[i].sectors; ++k) used[clean.extent[i].sector + k] = 1;
    unsigned strays = 0;
    for (unsigned k = 0; k < 800; ++k) {
        unsigned s = POOL_FIRST + rnd() % (POOL_END - POOL_FIRST);
        if (used[s] || (s >= pool.skip_first && s < pool.skip_end) || s == LEGACY) continue;
        uint8_t *at = raw(s * POOL_SECTOR);
        for (unsigned i = 0; i < POOL_SECTOR; ++i) at[i] = (uint8_t)rnd();
        if (k % 2U) {
            struct pool_header h;
            CHECK_EQ(pool_header(&clean, &clean.extent[0], &h), 0);
            h.id = (uint16_t)(100U + k);
            switch (k / 2U % 4U) {
            case 0: h.type = (uint8_t)(k % 3U ? POOL_LEGACY + rnd() % 200U : 0U); break;
            case 1: h.version = (uint8_t)(h.version + 1U + rnd() % 200U); break;
            case 2: h.length = BYTES + rnd() % (1U << 30); break;
            default: h.length = 0xffffffffU - rnd() % 5000U; break; /* past 32 bits once a header is added */
            }
            h.header_crc = ~pool_crc(~0U, &h, offsetof(struct pool_header, header_crc));
            memcpy(at, &h, sizeof h);
        }
        scan();
        if (pool.count != clean.count || !others_load(0)) went_wrong("a stray sector, try", (long)k);
        memcpy(at, good_at(s * POOL_SECTOR), POOL_SECTOR);
        ++strays;
    }
    CHECK_EQ(wrong, 0);
    CHECK(dropped > flips / 2U);
    printf("pool directory: %u header bit flips (%u dropped the object, the rest still whole), "
           "%u hits on a song's payload (%u refused), %u stray sectors\n", flips, dropped, hits, refused_two, strays);
}

int main(void)
{
    test_song_payloads();
    test_pool_directory();
#ifdef TEST_FULL_LAYOUT
    return report("test_corrupt_full");
#else
    return report("test_corrupt");
#endif
}
