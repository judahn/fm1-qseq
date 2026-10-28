/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#ifndef FM1_SONG_FLASH_H
#define FM1_SONG_FLASH_H
/* song-mode: SONG_DEVICE_FULL gives 23 sectors of the user partition to
 * code (song_config.h). Every flash call in the app keeps the stock
 * (logical) offsets, and these map them in two pieces: logical sectors 15
 * and 16 start the partition, and 25 on follow right after, keeping the
 * absolute flash address they have in the stock layout. Below 15 and from
 * 17 to 24 nothing exists, and an access that would cross from 16 into the
 * gap is refused. instrument.h includes this. */
#include "song_config.h"
#include "fm1_device.h"
#if SONG_DEVICE_FULL && !defined(FM1_OPEN_WEB)
/* The partition offset of [at, at + bytes), or -1 when that isn't all in
 * one piece. The partition's own end is the SDK's to check. */
static inline int32_t song_flash_at(uint32_t at, size_t bytes)
{
    const uint32_t low = SONG_FLASH_LOW * FM1_FLASH_SECTOR_BYTES, gap = SONG_FLASH_GAP * FM1_FLASH_SECTOR_BYTES,
                   high = SONG_FLASH_HIGH * FM1_FLASH_SECTOR_BYTES;
    if (at >= low && at < gap) return bytes <= gap - at ? (int32_t)(at - low) : -1;
    return at >= high ? (int32_t)(at - high + gap - low) : -1;
}
static inline int song_flash_read(uint32_t at, void *data, size_t bytes)
{ int32_t o = song_flash_at(at, bytes); return o < 0 ? FM1_FLASH_INVALID : fm1_flash_read((uint32_t)o, data, bytes); }
static inline int song_flash_write(uint32_t at, const void *data, size_t bytes)
{ int32_t o = song_flash_at(at, bytes); return o < 0 ? FM1_FLASH_INVALID : fm1_flash_write((uint32_t)o, data, bytes); }
static inline int song_flash_erase(uint32_t at)
{ int32_t o = song_flash_at(at, FM1_FLASH_SECTOR_BYTES); return o < 0 ? FM1_FLASH_INVALID : fm1_flash_erase((uint32_t)o); }
static inline const uint8_t *song_flash_map(uint32_t at, size_t bytes)
{ int32_t o = song_flash_at(at, bytes); return o < 0 ? NULL : fm1_flash_map((uint32_t)o, bytes); }
static inline int song_flash_invalidate(uint32_t at, size_t bytes)
{ int32_t o = song_flash_at(at, bytes); return o < 0 ? FM1_FLASH_INVALID : fm1_flash_invalidate((uint32_t)o, bytes); }
/* The logical size: the partition plus the 23 sectors code took, or 0 when
 * the SDK has no partition. */
static inline uint32_t song_flash_size(void)
{
    uint32_t n = fm1_flash_size();
    return n ? n + (SONG_FLASH_LOW + SONG_FLASH_HIGH - SONG_FLASH_GAP) * FM1_FLASH_SECTOR_BYTES : 0U;
}
#define fm1_flash_read song_flash_read
#define fm1_flash_write song_flash_write
#define fm1_flash_erase song_flash_erase
#define fm1_flash_map song_flash_map
#define fm1_flash_invalidate song_flash_invalidate
#define fm1_flash_size song_flash_size
#endif
#endif
