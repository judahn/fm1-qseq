/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Corrupt data in the kit library (test_corrupt.c does songs and the pool
 * directory): the kit table's two copies, banks coming in through the
 * transfer path, and a stock bank in place where the stock firmware kept
 * it, all damaged with flipped bits, cut short and scrambled. Seeded, so a
 * failure repeats. */
#include "corrupt_harness.h"

/* Every kit, its name and its bank read back, as the song list and the
 * transfers do. A damaged record may point anywhere, so this only must not
 * read wrong memory. */
static void read_all(void)
{
    static uint8_t out[KIT_BANK_HEADER + 8 * 30 * KIT_CHUNK];
    for (unsigned i = 0, n = kit_count(&pool); i < n; ++i) {
        unsigned id = kit_id_at(&pool, i);
        char name[17];
        struct kit k;
        (void)kit_name(&pool, id, name);
        if (kit_read(&pool, id, &k)) continue;
        uint32_t bytes = kit_bank_bytes(&pool, &k);
        (void)kit_bank_read(&pool, &k, kit_bank_crc(&pool, &k), 0, out, bytes < sizeof out ? bytes : (uint32_t)sizeof out);
    }
}

/* The kit table holds every kit, kept twice. Either copy damaged, nothing
 * is lost: the other is read, and boot's tidy-up (kit_tidy) makes the
 * damaged one again. Both damaged, the kits are lost, but nothing reads
 * wrong memory, and the tidy-up deletes and writes nothing, since it can't
 * be sure no kit uses a sample. */
static void test_kit_table(void)
{
    memset(flash, 0xff, sizeof flash);
    scan();
    unsigned one, two;
    uint32_t bytes = make_bank(0);
    CHECK_EQ(import(bank, bytes, bytes, &one), 0);
    bytes = make_bank(9);                                 /* shares two samples, one new */
    CHECK_EQ(import(bank, bytes, bytes, &two), 0);
    build(&song, 0);
    CHECK_EQ(save(&song, 1), 0);
    memcpy(good, flash, sizeof flash);
    scan();
    const struct song_pool clean = pool;
    const unsigned stored = samples();
    CHECK(stored == 4 && kit_count(&pool) == 2 && copies_alike());
    wrong = 0;
    unsigned flips = 0, kits_lost = 0, remade = 0;
    for (unsigned copy = 1; copy <= 2; ++copy) {
        const struct pool_extent *table = pool_find(&clean, POOL_KIT, copy);
        const uint32_t payload_bits = table->length * 8U, header = table->sector * POOL_SECTOR;
        for (unsigned bit = 0; bit < payload_bits + 256U + POOL_HEADER * 8U; ++bit, ++flips) {
            memcpy(flash, good, sizeof flash);
            if (bit < payload_bits) flip(raw(pool_address(table)), bit); /* every bit of the copy */
            else if (bit < payload_bits + 256U) *raw(pool_address(table)) = (uint8_t)(bit - payload_bits); /* every kit count */
            else flip(raw(header), bit - payload_bits - 256U); /* every bit of its header */
            scan();
            remade += !copies_alike();
            read_all();
            kits_lost += kit_count(&pool) != 2;
            (void)kit_tidy(&pool, "STOCK BANK");          /* as at boot */
            if (samples() != stored || kit_count(&pool) != 2 || !copies_alike()) went_wrong("kit table copy damaged (copy, try):", (long)(copy * 100000U + bit));
            scan();                                       /* and as at the next boot */
            if (kit_count(&pool) != 2 || !copies_alike()) went_wrong("kit table copy made again (copy, try):", (long)(copy * 100000U + bit));
            read_all();
        }
    }
    CHECK_EQ(kits_lost, 0);
    CHECK(remade > 1000);
    /* Both copies damaged: a bit in each payload, or each header. */
    static uint8_t damaged[BYTES];
    unsigned both = 0, both_lost = 0;
    const struct pool_extent *a = pool_find(&clean, POOL_KIT, 1), *b = pool_find(&clean, POOL_KIT, 2);
    for (unsigned k = 0; k < 400; ++k, ++both) {
        memcpy(flash, good, sizeof flash);
        if (k < 300) {
            flip(raw(pool_address(a)), rnd() % (a->length * 8U));
            flip(raw(pool_address(b)), rnd() % (b->length * 8U));
        } else {
            flip(raw(a->sector * POOL_SECTOR), rnd() % (offsetof(struct pool_header, header_crc) * 8U));
            flip(raw(b->sector * POOL_SECTOR), rnd() % (offsetof(struct pool_header, header_crc) * 8U));
        }
        scan();
        read_all();
        both_lost += kit_count(&pool) == 0;
        memcpy(damaged, flash, sizeof flash);
        (void)kit_tidy(&pool, "STOCK BANK");
        if (samples() != stored || memcmp(damaged, flash, sizeof flash)) went_wrong("both kit table copies damaged, try", (long)k);
    }
    memcpy(flash, good, sizeof flash);
    scan();
    printf("kit table: %u damaged copies (%u lost a kit, %u made again at boot), %u with both damaged (%u lost their kits), "
           "samples lost or flash written wrongly: %u\n", flips, kits_lost, remade, both, both_lost, wrong);
    CHECK_EQ(both_lost, both);
    CHECK_EQ(wrong, 0);
}

/* Banks coming in damaged: refused, nothing left behind, and the kit already
 * there reads back as it went in. A bank that still checks out (a bit in its
 * unused commit word) makes a kit, which goes again. */
static unsigned refused_banks, accepted_banks;
static void bank_in(const uint8_t *data, uint32_t sent, uint32_t total, unsigned stored, const char *what, long where)
{
    unsigned id;
    if (import(data, sent, total, &id)) ++refused_banks;
    else { ++accepted_banks; (void)kit_delete(&pool, id); }
    if (samples() != stored || kit_count(&pool) != 1) went_wrong(what, where);
}
static void test_banks(void)
{
    static uint8_t original[sizeof bank], fresh[sizeof bank], damaged[sizeof bank], out[sizeof bank];
    memset(flash, 0xff, sizeof flash);
    scan();
    const uint32_t bytes = make_bank(0);
    memcpy(original, bank, bytes);
    unsigned one;
    CHECK_EQ(import(original, bytes, bytes, &one), 0);
    struct kit k1;
    CHECK_EQ(kit_read(&pool, one, &k1), 0);
    const unsigned stored = samples();
    CHECK_EQ(make_bank(5), bytes);                        /* the bank to damage: its third sample new */
    memcpy(fresh, bank, bytes);
    wrong = 0;
    for (unsigned bit = 0; bit < KIT_BANK_HEADER * 8U; ++bit) { /* every bit of the header */
        memcpy(damaged, fresh, bytes);
        flip(damaged, bit);
        bank_in(damaged, bytes, bytes, stored, "bank header bit flipped:", (long)bit);
    }
    for (unsigned k = 0; k < 300; ++k) {                  /* bits in the sample data */
        memcpy(damaged, fresh, bytes);
        flip(damaged, KIT_BANK_HEADER * 8U + rnd() % ((bytes - KIT_BANK_HEADER) * 8U));
        bank_in(damaged, bytes, bytes, stored, "bank data bit flipped, try", (long)k);
    }
    for (uint32_t cut = 0; cut < bytes; cut += 61) {      /* declared short, and sent short */
        bank_in(fresh, cut, cut, stored, "bank declared as", (long)cut);
        bank_in(fresh, cut, bytes, stored, "bank cut off at", (long)cut);
    }
    for (unsigned k = 0; k < 300; ++k) {                  /* scrambled spans, and random banks */
        uint32_t at = k % 2U ? rnd() % KIT_BANK_HEADER : rnd() % bytes, span = 1U + rnd() % 96U;
        memcpy(damaged, fresh, bytes);
        if (k % 5U == 4U) { at = 0; span = bytes; }
        for (uint32_t i = at; i < at + span && i < bytes; ++i) damaged[i] = (uint8_t)rnd();
        bank_in(damaged, bytes, bytes, stored, "bank scrambled, try", (long)k);
    }
    CHECK_EQ(kit_bank_read(&pool, &k1, kit_bank_crc(&pool, &k1), 0, out, bytes), 0);
    CHECK(!memcmp(out, original, bytes));
    CHECK_EQ(wrong, 0);
    const unsigned in_refused = refused_banks, in_accepted = accepted_banks;
    /* A stock bank in place, its header damaged bit by bit: a kit only from a
     * bank that checks out, its samples inside the bank, read back whole. The
     * full device build has no place for one: its first sectors are code. */
    unsigned made = 0, in_place = 0;
    for (unsigned bit = 0; raw(LEGACY * POOL_SECTOR) && bit < KIT_BANK_HEADER * 8U; ++bit, ++in_place) {
        memset(flash, 0xff, sizeof flash);
        memcpy(raw(LEGACY * POOL_SECTOR), original, bytes);
        flip(raw(LEGACY * POOL_SECTOR), bit);
        scan();
        unsigned id = kit_tidy(&pool, "STOCK BANK");
        struct kit k;
        if (!id) continue;
        ++made;
        int ok = !kit_read(&pool, id, &k);
        for (unsigned pad = 0; ok && pad < KIT_PADS; ++pad) {
            struct kit_sample s;
            if (!k.sample[pad] || kit_sample(&pool, k.sample[pad], &s)) continue;
            ok = s.data >= raw(LEGACY * POOL_SECTOR) && s.data + s.bytes <= raw(LEGACY * POOL_SECTOR) + bytes;
        }
        ok = ok && !kit_bank_read(&pool, &k, kit_bank_crc(&pool, &k), 0, out, kit_bank_bytes(&pool, &k));
        if (!ok) went_wrong("stock bank in place, header bit flipped:", (long)bit);
    }
    CHECK_EQ(wrong, 0);
    printf("banks: %u damaged banks in (%u refused, %u still checked out), %u stock banks in place damaged (%u still made a kit)\n",
           in_refused + in_accepted, in_refused, in_accepted, in_place, made);
}

int main(void)
{
    test_kit_table();
    test_banks();
#ifdef TEST_FULL_LAYOUT
    return report("test_corrupt_kits_full");
#else
    return report("test_corrupt_kits");
#endif
}
