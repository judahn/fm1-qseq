/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#ifndef FM1_SONG_POOL_H
#define FM1_SONG_POOL_H
#include <stddef.h>
#include <stdint.h>

/* The storage pool (design 8c, phase 5a): sectors [first, end) of the user
 * partition, less one skipped range, hold every song, kit and sample as an
 * extent of whole sectors. An extent starts with a 64-byte header, written
 * after its payload and committed last, so a write cut short is never
 * mistaken for data. A replacement is written in free space first and only
 * then is the old copy erased: the two-copy scheme, so power loss keeps
 * the previous good copy. Allocation is next fit from a moving cursor,
 * which spreads the wear. A stock sample bank at its old address stays
 * where it is, as a legacy extent. */
enum { POOL_SECTOR = 4096, POOL_HEADER = 64, POOL_MAX_SECTORS = 160 };
enum { POOL_SONG = 1, POOL_RECOVERY, POOL_SAMPLE, POOL_KIT, POOL_LEGACY };
struct song_flash { /* the flash under the pool: the SDK's, or a test double */
    int (*read)(uint32_t at, void *data, size_t bytes);
    int (*write)(uint32_t at, const void *data, size_t bytes);
    int (*erase)(uint32_t at);
    const uint8_t *(*map)(uint32_t at, size_t bytes);
};
struct pool_header {
    uint32_t magic;
    uint8_t version, type;
    uint16_t id;
    uint32_t generation, length, crc; /* payload bytes and their CRC */
    char name[16];
    uint32_t extra[3];                /* by type: a sample's frames and hash, a recovery's song */
    uint32_t header_crc, unused, commit;
};
struct pool_extent { uint16_t sector, sectors, id; uint8_t type; uint32_t generation, length; };
struct song_pool {
    const struct song_flash *flash;
    uint16_t first, end, skip_first, skip_end, cursor, count;
    uint32_t generation;
    struct pool_extent extent[POOL_MAX_SECTORS];
};
struct pool_writer {
    struct song_pool *pool;
    struct pool_header header;
    uint32_t start, written, crc;
    uint16_t sectors, fill;
    int error;
    uint8_t buffer[256];
};

uint32_t pool_crc(uint32_t crc, const void *data, size_t bytes); /* CRC-32, start with ~0U */
/* Reads every header. Bounds in sectors. Returns the number of extents. */
unsigned pool_scan(struct song_pool *p, const struct song_flash *flash, unsigned first, unsigned end,
                   unsigned skip_first, unsigned skip_end);
const struct pool_extent *pool_find(const struct song_pool *p, unsigned type, unsigned id);
uint32_t pool_address(const struct pool_extent *e); /* of the payload */
int pool_header(const struct song_pool *p, const struct pool_extent *e, struct pool_header *h);
unsigned pool_sectors(uint32_t length);
unsigned pool_free(const struct song_pool *p); /* free sectors */
unsigned pool_used(const struct song_pool *p, unsigned type);
/* Writing: begin finds and erases room, put streams the payload, end writes
 * the header, commits, and erases the copy it replaces. 0 or an error. */
int pool_begin(struct pool_writer *w, struct song_pool *p, unsigned type, unsigned id, const char *name,
               uint32_t length);
void pool_put(struct pool_writer *w, const void *data, size_t bytes); /* with no pool: counts and CRCs only */
void pool_flush(struct pool_writer *w); /* everything put so far, into flash */
int pool_end(struct pool_writer *w);
int pool_delete(struct song_pool *p, unsigned type, unsigned id);
int pool_check(const struct song_pool *p, const struct pool_extent *e); /* payload CRC */
/* Moves extents down into earlier gaps, newest copy first, until a run of
 * `sectors` is free. Returns 0 when there is one. Legacy banks stay put. */
int pool_compact(struct song_pool *p, unsigned sectors);
unsigned pool_largest(const struct song_pool *p); /* longest free run */
unsigned pool_next_id(const struct song_pool *p, unsigned type);
#endif
