/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Hardware test measurements, in the hardware test build only. See diag.h. */
#include "diag.h"
#include "song.h"
#include "song_config.h"
#if SONG_HW_TEST
#include "instrument.h"
#include <stdlib.h>
#include <string.h>

struct song_diag song_diag;

SONG_SMALL void song_diag_reset(void)
{
    struct song_diag kept = song_diag;
    memset(&song_diag, 0, sizeof song_diag);
    song_diag.heaviest = kept.heaviest; /* memory results survive a stats reset */
    song_diag.largest_free = kept.largest_free;
    song_diag.heap = kept.heap;
    song_diag.outside = kept.outside;
    memcpy(song_diag.engine_free, kept.engine_free, sizeof kept.engine_free);
}
SONG_SMALL void song_diag_poll(uint32_t ticks, uint32_t now_ms, uint32_t blocks)
{
    struct song_diag *d = &song_diag;
    if (d->primed) {
        uint32_t span = ticks - d->last_ticks, us = span / 24U, b = blocks - d->last_blocks;
        d->interval_us[d->at] = us;
        d->interval_blocks[d->at] = (uint16_t)(b < 0xffff ? b : 0xffff);
        d->at = (uint8_t)((d->at + 1U) % SONG_DIAG_WINDOW);
        if (d->count < SONG_DIAG_WINDOW) ++d->count;
        if (us > d->run_max_us) d->run_max_us = us;
        if (b > d->run_max_blocks) d->run_max_blocks = b;
        ++d->polls;
        d->check_ticks += span;
        d->check_ms += now_ms - d->last_ms;
        if (d->check_ms >= 1000U) {
            d->tick_khz = d->check_ticks / d->check_ms;
            d->check_ticks = d->check_ms = 0;
        }
    }
    d->primed = 1;
    d->last_ticks = ticks;
    d->last_ms = now_ms;
    d->last_blocks = blocks;
}
SONG_SMALL void song_diag_window(uint32_t us[3], uint32_t blocks_x10[3])
{
    const struct song_diag *d = &song_diag;
    uint64_t sum_us = 0, sum_b = 0;
    us[0] = blocks_x10[0] = ~0U;
    us[2] = blocks_x10[2] = 0;
    for (unsigned i = 0; i < d->count; ++i) {
        uint32_t u = d->interval_us[i], b = d->interval_blocks[i] * 10U;
        if (u < us[0]) us[0] = u;
        if (u > us[2]) us[2] = u;
        if (b < blocks_x10[0]) blocks_x10[0] = b;
        if (b > blocks_x10[2]) blocks_x10[2] = b;
        sum_us += u;
        sum_b += b;
    }
    if (!d->count) us[0] = blocks_x10[0] = 0;
    us[1] = d->count ? (uint32_t)(sum_us / d->count) : 0;
    blocks_x10[1] = d->count ? (uint32_t)(sum_b / d->count) : 0;
}
SONG_SMALL void song_diag_late(uint32_t blocks_now, uint32_t due_block)
{
    uint32_t late = blocks_now > due_block ? blocks_now - due_block : 0;
    ++song_diag.late[late < SONG_DIAG_LATE ? late : SONG_DIAG_LATE - 1];
    if (late > song_diag.late_max) song_diag.late_max = late;
    ++song_diag.notes;
}

/* On the FM-1 the first probe read 524,272 bytes free with every engine, the
 * top of the search, while the link map gives the heap about 213 KB. So
 * malloc there doesn't return NULL when it runs out, and only a block that
 * lies inside the heap counts. The heap is _HEAP_BEGIN to _HEAP_END in the
 * link map (build/dx7/sdk.map). The browser has no such bound. */
#ifndef FM1_OPEN_WEB
extern char _HEAP_BEGIN[], _HEAP_END[];
static uint32_t heap_bytes(void) { return (uint32_t)((uintptr_t)_HEAP_END - (uintptr_t)_HEAP_BEGIN); }
#endif
SONG_SMALL uint32_t song_diag_top(void)
{
#ifdef FM1_OPEN_WEB
    return SONG_DIAG_CEILING;
#else
    return heap_bytes() < SONG_DIAG_CEILING ? heap_bytes() : SONG_DIAG_CEILING;
#endif
}
SONG_SMALL static int inside(const char *p, uint32_t bytes)
{
#ifdef FM1_OPEN_WEB
    (void)p;
    (void)bytes;
    return 1;
#else
    uintptr_t at = (uintptr_t)p, begin = (uintptr_t)_HEAP_BEGIN, end = (uintptr_t)_HEAP_END;
    return at >= begin && at <= end && bytes <= end - at;
#endif
}
/* Each block is freed at once and never written. The first block outside
 * the heap ends the search, and the probe stays off until a restart. */
SONG_SMALL uint32_t song_diag_largest_free(void)
{
    uint32_t low = 0, high = song_diag_top();
#ifndef FM1_OPEN_WEB
    song_diag.heap = heap_bytes();
#endif
    while (!song_diag.outside && low + 16U < high) {
        uint32_t mid = (low + high) / 2U;
        char *p = malloc(mid);
        if (p && !inside(p, mid)) song_diag.outside = 1;
        free(p);
        if (p && !song_diag.outside) low = mid;
        else high = mid;
    }
    song_diag.largest_free = low;
    return low;
}
/* Free memory with each engine type on track 6 in turn, one engine at a
 * time so the heap is never filled, then track 6 gets its engine back. The
 * default project already runs all eight engines. */
int song_diag_engines(void)
{
    unsigned keep = groove_source_type(5), least = ~0U;
    uint32_t now = song_diag_largest_free();
    song_diag.heaviest = -1;
    memset(song_diag.engine_free, 0, sizeof song_diag.engine_free);
    for (unsigned type = 0; !song_diag.outside && type < GROOVE_SOURCES && type < 6; ++type) {
        song_diag.engine_free[type] = groove_source_set(5, type) ? 0 : song_diag_largest_free();
        if (song_diag.engine_free[type] && song_diag.engine_free[type] < least) {
            least = song_diag.engine_free[type];
            song_diag.heaviest = (int8_t)type;
        }
    }
    int result = groove_source_set(5, keep);
    song_diag.largest_free = now;
    return song_diag.outside ? -1 : result;
}
#endif
