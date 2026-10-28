/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Writing to the pool: a new copy streamed into free space and committed
 * before the old one goes, deleting, checking and compacting. See pool.h. */
#include "pool.h"
#include "song_config.h"
#include <string.h>
#if SONG_UI
#include "song.h" /* SONG_SMALL */
#include "pool_private.h"

SONG_SMALL static int blank(const struct song_pool *p, uint32_t at)
{
    const uint8_t *m = p->flash->map(at, POOL_SECTOR);
    if (!m) return 0;
    for (unsigned i = 0; i < POOL_SECTOR; ++i)
        if (m[i] != 0xff) return 0;
    return 1;
}
SONG_SMALL static int begin_at(struct pool_writer *w, struct song_pool *p, int sector, unsigned type, unsigned id,
                               const char *name, uint32_t length)
{
    memset(w, 0, sizeof *w - sizeof w->buffer);
    w->pool = p;
    w->sectors = (uint16_t)pool_sectors(length);
    if (sector < 0) return w->error = POOL_FULL;
    w->start = (uint32_t)sector * POOL_SECTOR;
    for (unsigned k = 0; k < w->sectors && !w->error; ++k)
        if (!blank(p, w->start + k * POOL_SECTOR)) w->error = p->flash->erase(w->start + k * POOL_SECTOR);
    w->header = (struct pool_header){POOL_MAGIC, POOL_VERSION, (uint8_t)type, (uint16_t)id, p->generation + 1U, length,
                                     0, {0}, {0}, 0, 0xffffffffU, 0xffffffffU};
    for (unsigned i = 0; name && name[i] && i < sizeof w->header.name; ++i) w->header.name[i] = name[i];
    w->crc = ~0U;
    p->cursor = (uint16_t)(sector + w->sectors);
    return w->error;
}
SONG_SMALL int pool_begin(struct pool_writer *w, struct song_pool *p, unsigned type, unsigned id, const char *name,
                          uint32_t length)
{
    return begin_at(w, p, pool_find_run(p, pool_sectors(length), p->cursor), type, id, name, length);
}
SONG_SMALL void pool_flush(struct pool_writer *w)
{
    if (w->fill && !w->error)
        w->error = w->pool->flash->write(w->start + POOL_HEADER + w->written - w->fill, w->buffer, w->fill);
    w->fill = 0;
}
SONG_SMALL void pool_put(struct pool_writer *w, const void *data, size_t bytes)
{
    const uint8_t *b = data;
    w->crc = pool_crc(w->crc, data, bytes);
    if (!w->pool) { w->written += (uint32_t)bytes; return; } /* measuring only */
    while (bytes && !w->error) {
        size_t n = sizeof w->buffer - w->fill < bytes ? sizeof w->buffer - w->fill : bytes;
        if (w->written + n > w->header.length) { w->error = -1; return; }
        memcpy(w->buffer + w->fill, b, n);
        w->fill = (uint16_t)(w->fill + n);
        w->written += (uint32_t)n;
        b += n;
        bytes -= n;
        if (w->fill == sizeof w->buffer) pool_flush(w);
    }
}
SONG_SMALL int pool_end(struct pool_writer *w)
{
    struct song_pool *p = w->pool;
    pool_flush(w);
    if (!w->error && w->written != w->header.length) w->error = -1;
    w->header.crc = ~w->crc;
    w->header.header_crc = pool_header_crc(&w->header);
    if (!w->error) w->error = p->flash->write(w->start, &w->header, sizeof w->header);
    uint32_t commit = 0; /* the moment the new copy counts */
    if (!w->error) w->error = p->flash->write(w->start + offsetof(struct pool_header, commit), &commit, 4);
    if (w->error) return w->error;
    const struct pool_extent *old = pool_find(p, w->header.type, w->header.id);
    if (old) (void)p->flash->erase((uint32_t)old->sector * POOL_SECTOR); /* only now the old copy goes */
    const struct pool_extent e = {(uint16_t)(w->start / POOL_SECTOR), w->sectors, w->header.id, w->header.type,
                                  w->header.generation, w->header.length};
    pool_add(p, &e);
    return 0;
}
SONG_SMALL int pool_delete(struct song_pool *p, unsigned type, unsigned id)
{
    for (unsigned i = 0; i < p->count; ++i)
        if (p->extent[i].type == type && p->extent[i].id == id) {
            int r = p->flash->erase((uint32_t)p->extent[i].sector * POOL_SECTOR);
            if (!r) pool_remove_at(p, i);
            return r;
        }
    return -1;
}
SONG_SMALL int pool_check(const struct song_pool *p, const struct pool_extent *e)
{
    struct pool_header h;
    const uint8_t *m = p->flash->map(pool_address(e), e->length);
    if (e->type == POOL_LEGACY) return 0;
    if (pool_header(p, e, &h) || !m) return -1;
    return ~pool_crc(~0U, m, e->length) == h.crc ? 0 : -1;
}

/* One move: the first extent past a gap that fits in it, copied there. */
SONG_SMALL static int move_one(struct song_pool *p)
{
    uint8_t used[POOL_MAX_SECTORS];
    pool_used_map(p, used);
    for (unsigned s = p->first; s < p->end; ++s) {
        if (used[s]) continue;
        unsigned run = 0;
        while (s + run < p->end && !used[s + run]) ++run;
        for (unsigned i = 0; i < p->count; ++i) {
            struct pool_extent e = p->extent[i];
            struct pool_header h;
            if (e.sector < s || e.type == POOL_LEGACY || e.sectors > run || pool_header(p, &e, &h)) continue;
            struct pool_writer w;
            if (begin_at(&w, p, (int)s, e.type, e.id, h.name, e.length)) return w.error;
            memcpy(w.header.extra, h.extra, sizeof h.extra);
            for (uint32_t at = 0; at < e.length && !w.error; at += sizeof w.buffer) {
                uint8_t chunk[256];
                uint32_t n = e.length - at < sizeof chunk ? e.length - at : (uint32_t)sizeof chunk;
                w.error = p->flash->read(pool_address(&e) + at, chunk, n);
                if (!w.error) pool_put(&w, chunk, n);
            }
            return pool_end(&w);
        }
        s += run;
    }
    return POOL_FULL;
}
SONG_SMALL int pool_compact(struct song_pool *p, unsigned sectors)
{
    for (unsigned guard = 0; pool_largest(p) < sectors && guard < POOL_MAX_SECTORS; ++guard) {
        int r = move_one(p);
        if (r) return r;
    }
    return pool_largest(p) >= sectors ? 0 : POOL_FULL;
}
#endif
