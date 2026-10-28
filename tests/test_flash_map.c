/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* The full device build's flash map (song_flash.h): logical sectors 15 and
 * 16, then 25 on, onto a user partition 23 sectors smaller. Each piece's
 * bounds, nothing below 15 or from 17 to 24, no access across the gap, round
 * trips through the RAM partition, the logical size, and logical 25 on
 * keeping the absolute flash address it has in the stock layout. Built with
 * -DSONG_DEVICE_FULL=1 -DTEST_FULL_LAYOUT (run_host.sh). */
#include "harness.h"
#include "ram_flash.h"

enum { S = POOL_SECTOR, LOW = 15, GAP = 17, HIGH = 25, LAST = 130, FLASH_END = 1044480, STOCK_USER = 536576 };

static void test_pieces(void)
{
    CHECK_EQ(song_flash_at(LOW * S, 1), 0);                 /* 15 and 16 start the partition */
    CHECK_EQ(song_flash_at(LOW * S, 2 * S), 0);
    CHECK_EQ(song_flash_at(16 * S + S - 1, 1), 2 * S - 1);
    CHECK_EQ(song_flash_at(LOW * S, 2 * S + 1), -1);        /* one byte over into the gap */
    CHECK_EQ(song_flash_at(16 * S + S - 1, 2), -1);
    CHECK_EQ(song_flash_at(16 * S + 100, S), -1);
    CHECK_EQ(song_flash_at(16 * S, 0xffffffffU), -1);       /* no wrap past the gap */
    for (uint32_t at = 0; at < LOW * S; at += 512) CHECK_EQ(song_flash_at(at, 1), -1);
    CHECK_EQ(song_flash_at(LOW * S - 1, 1), -1);
    for (uint32_t at = GAP * S; at < HIGH * S; at += 512) CHECK_EQ(song_flash_at(at, 1), -1);
    CHECK_EQ(song_flash_at(HIGH * S - 1, 1), -1);
    CHECK_EQ(song_flash_at(HIGH * S, 1), 2 * S);            /* 25 on, right after */
    CHECK_EQ(song_flash_at(HIGH * S + 5, 100), 2 * S + 5);
    CHECK_EQ(song_flash_at(LAST * S, S), (LAST - HIGH + 2) * S);
    CHECK_EQ((LAST - HIGH + 3) * S, BYTES);                 /* 130 is the partition's last sector */
    /* Every call refuses what isn't mapped, and touches nothing. */
    uint8_t byte = 0x5a, out[300];
    memset(flash, 0xff, sizeof flash);
    const uint32_t refused[] = {0, 14 * S, LOW * S - 1, GAP * S, 20 * S + 7, 24 * S, HIGH * S - 1};
    for (unsigned i = 0; i < sizeof refused / sizeof refused[0]; ++i) {
        uint32_t at = refused[i];
        CHECK_EQ(fm1_flash_read(at, out, 1), FM1_FLASH_INVALID);
        CHECK_EQ(fm1_flash_write(at, &byte, 1), FM1_FLASH_INVALID);
        CHECK_EQ(fm1_flash_erase(at - at % S), FM1_FLASH_INVALID);
        CHECK(fm1_flash_map(at, 1) == NULL);
        CHECK_EQ(fm1_flash_invalidate(at, 1), FM1_FLASH_INVALID);
    }
    CHECK_EQ(fm1_flash_read(16 * S + S - 100, out, 200), FM1_FLASH_INVALID); /* across the gap */
    CHECK_EQ(fm1_flash_write(16 * S + S - 100, out, 200), FM1_FLASH_INVALID);
    CHECK(fm1_flash_map(16 * S + S - 100, 200) == NULL);
    CHECK(fm1_flash_read((LAST + 1) * S, out, 1) != 0);     /* past the partition: the SDK refuses */
    unsigned touched = 0;
    for (unsigned i = 0; i < BYTES; ++i) touched += flash[i] != 0xff;
    CHECK_EQ(touched, 0);
}

/* Every mapped sector written and read back through its logical address,
 * landing where the map says, with its neighbours left alone. */
static void test_round_trips(void)
{
    static uint8_t pattern[S], back[S];
    memset(flash, 0, sizeof flash);
    unsigned sectors = 0, wrong = 0;
    for (unsigned sector = LOW; sector <= LAST; ++sector) {
        if (sector == GAP) sector = HIGH;
        uint32_t at = sector * S, physical = sector < GAP ? sector - LOW : sector - HIGH + GAP - LOW;
        for (unsigned i = 0; i < S; ++i) pattern[i] = (uint8_t)(sector * 7U + i * 13U + 1U);
        wrong += fm1_flash_erase(at) != 0 || fm1_flash_write(at, pattern, S) != 0 || fm1_flash_read(at, back, S) != 0;
        wrong += memcmp(back, pattern, S) != 0 || memcmp(flash + physical * S, pattern, S) != 0;
        wrong += fm1_flash_map(at, S) != flash + physical * S || fm1_flash_invalidate(at, S) != 0;
        ++sectors;
    }
    CHECK_EQ(sectors, 2 + LAST - HIGH + 1);
    CHECK_EQ(sectors, RAM_SECTORS);                         /* every sector of the partition, once */
    CHECK_EQ(wrong, 0);
    for (unsigned sector = LOW; sector <= LAST; ++sector) { /* each still holds its own pattern */
        if (sector == GAP) sector = HIGH;
        for (unsigned i = 0; i < S; ++i) pattern[i] = (uint8_t)(sector * 7U + i * 13U + 1U);
        wrong += fm1_flash_read(sector * S, back, S) != 0 || memcmp(back, pattern, S) != 0;
    }
    CHECK_EQ(wrong, 0);
}

/* The logical size is the stock partition's, and 25 on sits where it does
 * in the stock layout: the partition ends at the same place in both. */
static void test_size_and_addresses(void)
{
    CHECK_EQ(fm1_flash_size(), STOCK_USER);                 /* GROOVE_FLASH_BYTES, so the stock checks pass */
    const uint32_t full_start = FLASH_END - BYTES, stock_start = FLASH_END - STOCK_USER;
    unsigned same = 0;
    for (unsigned sector = HIGH; sector <= LAST; ++sector)
        same += full_start + (uint32_t)song_flash_at(sector * S, 1) == stock_start + sector * S;
    CHECK_EQ(same, LAST - HIGH + 1);
    CHECK_EQ(full_start + (uint32_t)song_flash_at(LOW * S, 1), stock_start + 23 * S); /* settings: where 23 was */
}

int main(void)
{
    test_pieces();
    test_round_trips();
    test_size_and_addresses();
    return report("test_flash_map");
}
