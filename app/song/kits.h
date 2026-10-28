/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#ifndef FM1_SONG_KITS_H
#define FM1_SONG_KITS_H
#include "pool.h"

/* The kit library (design section 8, Kits). A kit is 8 sample references,
 * with the pads' names, and optional pad settings. A sample is its own
 * extent in the pool, the SEA5 data right after the header, so a pad
 * binds straight to mapped flash. All kits share one table object. A
 * stock bank found at its old address stays there as one legacy extent,
 * its samples numbered KIT_LEGACY + pad. Kits that share a sample point at
 * the one copy. */
enum { KIT_PADS = 8, KIT_PARAMS = 6, KIT_LEGACY = 0xf000, KIT_BANK_HEADER = 512, KIT_CHUNK = 228, KIT_MAX = 32 };
struct kit {
    uint16_t sample[KIT_PADS];               /* 0 for an empty pad */
    uint8_t params[KIT_PADS][KIT_PARAMS];    /* level, tune, length, choke, variation, filter */
    uint8_t has_params, unused;
};
struct kit_sample { const uint8_t *data; uint32_t bytes, frames; char name[16]; };
int kit_sample(const struct song_pool *p, unsigned ref, struct kit_sample *out); /* 0, or -1 if missing */
/* Kits are kept in one table object, in id order, up to KIT_MAX. */
unsigned kit_count(const struct song_pool *p);
unsigned kit_id_at(const struct song_pool *p, unsigned index);
unsigned kit_next_id(const struct song_pool *p);
int kit_name(const struct song_pool *p, unsigned id, char out[17]);
int kit_read(const struct song_pool *p, unsigned id, struct kit *k);
int kit_write(struct song_pool *p, unsigned id, const char *name, const struct kit *k);
unsigned kit_refs(const struct song_pool *p, unsigned ref); /* how many kits use a sample */
/* Deletes a kit, and the samples no other kit uses. */
int kit_delete(struct song_pool *p, unsigned id);
/* After a scan: a stock bank no kit uses becomes a kit named `name`, and
 * samples no kit uses (an import cut short) go. Returns that kit or 0. When
 * the kit table is damaged (its CRC is checked), or samples have no table,
 * nothing is made or deleted. */
unsigned kit_tidy(struct song_pool *p, const char *name);

/* A stock-format bank streamed in by the transfer path, turned into
 * samples and a kit. A sample already in the pool is not stored again. */
struct kit_import {
    struct pool_writer w;
    uint8_t header[KIT_BANK_HEADER];
    uint32_t total, position, crc;
    int open, error;
    struct kit kit;
    uint16_t made[KIT_PADS];
    uint8_t made_count, sample;
};
int kit_import_begin(struct kit_import *k, uint32_t total);
int kit_import_write(struct kit_import *k, struct song_pool *p, uint32_t offset, const uint8_t *data, uint32_t bytes);
int kit_import_finish(struct kit_import *k, struct song_pool *p, const char *name, unsigned *id);
void kit_import_cancel(struct kit_import *k, struct song_pool *p);
/* A kit as a stock-format bank, for reading back out. */
uint32_t kit_bank_bytes(const struct song_pool *p, const struct kit *k);
uint32_t kit_bank_crc(const struct song_pool *p, const struct kit *k);
int kit_bank_read(const struct song_pool *p, const struct kit *k, uint32_t crc, uint32_t offset, uint8_t *out, uint32_t bytes);
#endif
