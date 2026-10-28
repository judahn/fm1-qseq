/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* The storage pool. See pool.h. */
#include "pool.h"
#include "song_config.h"
#include <string.h>
#if SONG_UI
#include "song.h" /* SONG_SMALL */
#include "pool_private.h"
enum { LEGACY_SECTOR = 17, LEGACY_MAGIC = 0x53314d46U };

SONG_SMALL uint32_t pool_crc(uint32_t crc, const void *data, size_t bytes)
{
    static const uint32_t nibble[16] = {
        0x00000000U, 0x1db71064U, 0x3b6e20c8U, 0x26d930acU, 0x76dc4190U, 0x6b6b51f4U, 0x4db26158U, 0x5005713cU,
        0xedb88320U, 0xf00f9344U, 0xd6d6a3e8U, 0xcb61b38cU, 0x9b64c2b0U, 0x86d3d2d4U, 0xa00ae278U, 0xbdbdf21cU};
    const uint8_t *p = data;
    while (bytes--) {
        crc ^= *p++;
        crc = crc >> 4 ^ nibble[crc & 15U];
        crc = crc >> 4 ^ nibble[crc & 15U];
    }
    return crc;
}
SONG_SMALL unsigned pool_sectors(uint32_t length) { return (POOL_HEADER + length + POOL_SECTOR - 1U) / POOL_SECTOR; }
SONG_SMALL uint32_t pool_address(const struct pool_extent *e)
{
    return (uint32_t)e->sector * POOL_SECTOR + (e->type == POOL_LEGACY ? 0U : POOL_HEADER);
}
static int skipped(const struct song_pool *p, unsigned s) { return s >= p->skip_first && s < p->skip_end; }
/* Sectors an extent may cover: inside the pool, clear of the skipped range. */
static int fits(const struct song_pool *p, unsigned s, unsigned n)
{
    return s >= p->first && s + n <= p->end && n && (s + n <= p->skip_first || s >= p->skip_end);
}
SONG_SMALL void pool_used_map(const struct song_pool *p, uint8_t used[POOL_MAX_SECTORS])
{
    memset(used, 0, POOL_MAX_SECTORS);
    for (unsigned s = 0; s < POOL_MAX_SECTORS; ++s) used[s] = s < p->first || s >= p->end || skipped(p, s);
    for (unsigned i = 0; i < p->count; ++i)
        for (unsigned k = 0; k < p->extent[i].sectors; ++k) used[p->extent[i].sector + k] = 1;
}
uint32_t pool_header_crc(const struct pool_header *h) { return ~pool_crc(~0U, h, offsetof(struct pool_header, header_crc)); }
SONG_SMALL void pool_remove_at(struct song_pool *p, unsigned i)
{
    memmove(&p->extent[i], &p->extent[i + 1U], (p->count - i - 1U) * sizeof p->extent[0]);
    --p->count;
}
SONG_SMALL const struct pool_extent *pool_find(const struct song_pool *p, unsigned type, unsigned id)
{
    for (unsigned i = 0; i < p->count; ++i)
        if (p->extent[i].type == type && p->extent[i].id == id) return &p->extent[i];
    return NULL;
}
SONG_SMALL void pool_add(struct song_pool *p, const struct pool_extent *e)
{
    for (unsigned i = 0; i < p->count; ++i) /* one copy per object: the newest */
        if (p->extent[i].type == e->type && p->extent[i].id == e->id) {
            if (p->extent[i].generation - e->generation < 0x80000000U) return;
            pool_remove_at(p, i);
            break;
        }
    if (p->count < POOL_MAX_SECTORS) p->extent[p->count++] = *e;
    if (e->generation - p->generation < 0x80000000U) p->generation = e->generation;
}

SONG_SMALL unsigned pool_scan(struct song_pool *p, const struct song_flash *flash, unsigned first, unsigned end,
                              unsigned skip_first, unsigned skip_end)
{
    memset(p, 0, sizeof *p);
    p->flash = flash;
    p->first = (uint16_t)first;
    p->end = (uint16_t)(end < POOL_MAX_SECTORS ? end : POOL_MAX_SECTORS);
    p->skip_first = (uint16_t)skip_first;
    p->skip_end = (uint16_t)skip_end;
    p->cursor = p->first;
    for (unsigned s = first; s < p->end;) {
        struct pool_header h;
        if (skipped(p, s) || flash->read(s * POOL_SECTOR, &h, sizeof h)) { ++s; continue; }
        unsigned n = pool_sectors(h.length);
        if (h.magic == POOL_MAGIC && h.version == POOL_VERSION && !h.commit && h.header_crc == pool_header_crc(&h) &&
            h.type >= POOL_SONG && h.type < POOL_LEGACY && fits(p, s, n)) {
            const struct pool_extent e = {(uint16_t)s, (uint16_t)n, h.id, h.type, h.generation, h.length};
            pool_add(p, &e);
        } else if (s == LEGACY_SECTOR) {
            /* A stock bank: magic, version, bytes, crc, commit and rate words. */
            uint32_t word[6];
            memcpy(word, &h, sizeof word);
            n = (word[2] + POOL_SECTOR - 1U) / POOL_SECTOR;
            if (word[0] == LEGACY_MAGIC && word[1] == 1 && !word[4] && word[5] == 32000U && word[2] >= 512U &&
                fits(p, s, n)) {
                const struct pool_extent e = {(uint16_t)s, (uint16_t)n, 0, POOL_LEGACY, 0, word[2]};
                pool_add(p, &e);
                s += n;
                continue;
            }
        }
        ++s; /* free, a copy replaced by a newer one, or the rest of an extent */
    }
    return p->count;
}
SONG_SMALL int pool_header(const struct song_pool *p, const struct pool_extent *e, struct pool_header *h)
{
    return p->flash->read((uint32_t)e->sector * POOL_SECTOR, h, sizeof *h);
}
SONG_SMALL unsigned pool_free(const struct song_pool *p)
{
    uint8_t used[POOL_MAX_SECTORS];
    unsigned n = 0;
    pool_used_map(p, used);
    for (unsigned s = 0; s < POOL_MAX_SECTORS; ++s) n += !used[s];
    return n;
}
SONG_SMALL unsigned pool_used(const struct song_pool *p, unsigned type)
{
    unsigned n = 0;
    for (unsigned i = 0; i < p->count; ++i)
        if (p->extent[i].type == type) n += p->extent[i].sectors;
    return n;
}
/* The free run of `n` sectors next after the cursor, or -1. */
SONG_SMALL int pool_find_run(const struct song_pool *p, unsigned n, unsigned from)
{
    uint8_t used[POOL_MAX_SECTORS];
    pool_used_map(p, used);
    for (unsigned pass = 0; pass < 2; ++pass)
        for (unsigned s = pass ? p->first : from; s + n <= p->end && (!pass || s < from); ++s) {
            unsigned k = 0;
            while (k < n && !used[s + k]) ++k;
            if (k == n) return (int)s;
            s += k;
        }
    return -1;
}
SONG_SMALL unsigned pool_largest(const struct song_pool *p)
{
    uint8_t used[POOL_MAX_SECTORS];
    unsigned best = 0, run = 0;
    pool_used_map(p, used);
    for (unsigned s = 0; s < POOL_MAX_SECTORS; ++s) {
        run = used[s] ? 0 : run + 1U;
        if (run > best) best = run;
    }
    return best;
}
SONG_SMALL unsigned pool_next_id(const struct song_pool *p, unsigned type)
{
    unsigned id = 1;
    for (unsigned i = 0; i < p->count; ++i)
        if (p->extent[i].type == type && p->extent[i].id >= id) id = p->extent[i].id + 1U;
    return id;
}

#endif
