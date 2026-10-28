/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#ifndef FM1_SONG_FILE_H
#define FM1_SONG_FILE_H
#include "song.h"
#include "pool.h"

/* A song as stored (phase 5a), little endian and only what is used:
 *   version, then the sounds block (the 8 sound setups and the mix, opaque
 *   here, captured by the port), name, kit, base programs, bars, the tempo,
 *   time signature and marker lists, the loop, the settings, the knobs,
 *   the glitch entries in use, humanize and seed, then per track its
 *   event count and events at 8 bytes each (the FX lane among track 1's). */
enum { SONG_FILE_VERSION = 1 };
/* Streams the song into a writer, or with a writer that has no pool,
 * just counts it and takes its CRC. */
void song_file_write(struct pool_writer *w, const struct song *s, const uint8_t *sounds, uint32_t sounds_bytes);
uint32_t song_file_bytes(const struct song *s, uint32_t sounds_bytes);
uint32_t song_file_crc(const struct song *s, const uint8_t *sounds, uint32_t sounds_bytes);
/* Checks the whole payload first and changes nothing if it isn't right.
 * Then fills the song, keeping its undo journal pointer, and points
 * `sounds` at the sounds block inside `data`. 0 or -1. */
int song_file_read(struct song *s, const uint8_t *data, uint32_t bytes, const uint8_t **sounds, uint32_t *sounds_bytes);

/* The recovery copy: written when playback stops, if the song is not what
 * was last written or saved and the last write is at least `gap` ms old. */
struct song_recovery { uint32_t written_crc, saved_crc, written_at; uint8_t pending, any; };
int song_recovery_due(struct song_recovery *r, uint32_t crc, uint32_t now, uint32_t gap);
#endif
