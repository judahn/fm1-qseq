/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#ifndef FM1_SONG_FLASH_H
#define FM1_SONG_FLASH_H
/* song-mode: SONG_DEVICE_FULL gives the stock project area to code, so the
 * user partition starts SONG_USER_BASE bytes later. Every flash call in the
 * app keeps the stock (logical) offsets, and these move them down. Nothing
 * below the base exists. instrument.h includes this. */
#include "song_config.h"
#include "fm1_device.h"
#if SONG_DEVICE_FULL && !defined(FM1_OPEN_WEB)
static inline int song_flash_read(uint32_t at, void *data, size_t bytes)
{ return at < SONG_USER_BASE ? FM1_FLASH_INVALID : fm1_flash_read(at - SONG_USER_BASE, data, bytes); }
static inline int song_flash_write(uint32_t at, const void *data, size_t bytes)
{ return at < SONG_USER_BASE ? FM1_FLASH_INVALID : fm1_flash_write(at - SONG_USER_BASE, data, bytes); }
static inline int song_flash_erase(uint32_t at)
{ return at < SONG_USER_BASE ? FM1_FLASH_INVALID : fm1_flash_erase(at - SONG_USER_BASE); }
static inline const uint8_t *song_flash_map(uint32_t at, size_t bytes)
{ return at < SONG_USER_BASE ? NULL : fm1_flash_map(at - SONG_USER_BASE, bytes); }
static inline int song_flash_invalidate(uint32_t at, size_t bytes)
{ return at < SONG_USER_BASE ? FM1_FLASH_INVALID : fm1_flash_invalidate(at - SONG_USER_BASE, bytes); }
static inline uint32_t song_flash_size(void) { return fm1_flash_size() + SONG_USER_BASE; }
#define fm1_flash_read song_flash_read
#define fm1_flash_write song_flash_write
#define fm1_flash_erase song_flash_erase
#define fm1_flash_map song_flash_map
#define fm1_flash_invalidate song_flash_invalidate
#define fm1_flash_size song_flash_size
#endif
#endif
