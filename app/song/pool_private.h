/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#ifndef FM1_SONG_POOL_PRIVATE_H
#define FM1_SONG_POOL_PRIVATE_H
/* What pool.c (scanning, finding room) and pool_write.c share. */
#include "pool.h"
enum { POOL_MAGIC = 0x50534d46U, POOL_VERSION = 1, POOL_FULL = -20 };
void pool_used_map(const struct song_pool *p, uint8_t used[POOL_MAX_SECTORS]);
uint32_t pool_header_crc(const struct pool_header *h);
void pool_remove_at(struct song_pool *p, unsigned i);
void pool_add(struct song_pool *p, const struct pool_extent *e); /* keeps the newest copy of each object */
int pool_find_run(const struct song_pool *p, unsigned n, unsigned from);
#endif
