/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* The flash under the storage tests: RAM that behaves like NOR (bits only
 * clear, an erase sets a whole sector, and power can go at any byte once
 * `budget` bytes have been written). By default the pool sees it as the
 * stock layout's first 48 logical sectors, with 15 and 16 skipped. Built
 * with TEST_FULL_LAYOUT it is SONG_DEVICE_FULL's partition, 23 sectors
 * short, and the pool reaches logical sectors 25 to 125 through
 * song_flash.h's two-piece map, the code the device runs. raw() is the RAM
 * behind a logical address, for tests that damage bytes in place. The
 * full layout's tests are built with -DSONG_DEVICE_FULL=1 as well, so
 * every file in them sees the device's configuration. */
#ifndef FM1_TEST_RAM_FLASH_H
#define FM1_TEST_RAM_FLASH_H
#include "pool.h"
#include "song_config.h"
#include <string.h>
#if defined(TEST_FULL_LAYOUT) && !SONG_DEVICE_FULL
#error "TEST_FULL_LAYOUT is built with -DSONG_DEVICE_FULL=1 (run_host.sh)"
#endif

#ifdef TEST_FULL_LAYOUT
enum { RAM_SECTORS = 108, POOL_FIRST = 25, POOL_END = 126, SKIP = 25, SKIP_END = 25 };
#else
enum { RAM_SECTORS = 48, POOL_FIRST = 0, POOL_END = 48, SKIP = 15, SKIP_END = 17 };
#endif
enum { BYTES = RAM_SECTORS * POOL_SECTOR, POOL_SECTORS = POOL_END - POOL_FIRST - (SKIP_END - SKIP) };
static uint8_t flash[BYTES];
static long budget = -1; /* bytes still written before the power goes, -1 for no limit */
static int ram_read(uint32_t at, void *data, size_t n) { if (at > BYTES || n > BYTES - at) return -1; memcpy(data, flash + at, n); return 0; }
static int ram_write(uint32_t at, const void *data, size_t n)
{
    const uint8_t *s = data;
    if (at > BYTES || n > BYTES - at) return -1;
    for (size_t i = 0; i < n; ++i) if ((flash[at + i] & s[i]) != s[i]) return -4; /* needs erase */
    for (size_t i = 0; i < n; ++i) {
        if (!budget) return -2;
        if (budget > 0) --budget;
        flash[at + i] = s[i];
    }
    return 0;
}
static int ram_erase(uint32_t at)
{
    if (!budget || at % POOL_SECTOR || at >= BYTES) return -2;
    memset(flash + at, 0xff, POOL_SECTOR);
    return 0;
}
static const uint8_t *ram_map(uint32_t at, size_t n) { return at <= BYTES && n <= BYTES - at ? flash + at : NULL; }

#ifdef TEST_FULL_LAYOUT
/* The SDK's calls, as the partition: song_flash.h maps onto these. */
uint32_t fm1_flash_size(void) { return BYTES; }
int fm1_flash_read(uint32_t at, void *data, size_t n) { return ram_read(at, data, n); }
int fm1_flash_write(uint32_t at, const void *data, size_t n) { return ram_write(at, data, n); }
int fm1_flash_erase(uint32_t at) { return ram_erase(at); }
const uint8_t *fm1_flash_map(uint32_t at, size_t n) { return ram_map(at, n); }
int fm1_flash_invalidate(uint32_t at, size_t n) { return ram_map(at, n) ? 0 : -1; }
#include "song_flash.h"
static int f_read(uint32_t at, void *data, size_t n) { return song_flash_read(at, data, n); }
static int f_write(uint32_t at, const void *data, size_t n) { return song_flash_write(at, data, n); }
static int f_erase(uint32_t at) { return song_flash_erase(at); }
static const uint8_t *f_map(uint32_t at, size_t n) { return song_flash_map(at, n); }
static uint8_t *raw(uint32_t at) { int32_t o = song_flash_at(at, 1); return o < 0 ? NULL : flash + o; }
#else
static int f_read(uint32_t at, void *data, size_t n) { return ram_read(at, data, n); }
static int f_write(uint32_t at, const void *data, size_t n) { return ram_write(at, data, n); }
static int f_erase(uint32_t at) { return ram_erase(at); }
static const uint8_t *f_map(uint32_t at, size_t n) { return ram_map(at, n); }
static uint8_t *raw(uint32_t at) { return at < BYTES ? flash + at : NULL; }
#endif
static const struct song_flash ram = {f_read, f_write, f_erase, f_map};
static struct song_pool pool;
static void scan(void) { pool_scan(&pool, &ram, POOL_FIRST, POOL_END, SKIP, SKIP_END); }
#endif
