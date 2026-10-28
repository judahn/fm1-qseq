/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Stock banks in and out of the kit library: importing one streamed in by
 * the transfer path, and reading a kit back as one. See kits.h. */
#include "kit_bank.h"
#include "song_config.h"
#if SONG_UI
#include "song.h" /* SONG_SMALL */

/* Importing. The bank's CRC skips its own crc and commit words. */
SONG_SMALL int kit_import_begin(struct kit_import *k, uint32_t total)
{
    memset(k, 0, sizeof *k);
    k->total = total;
    k->crc = ~0U;
    return total < KIT_BANK_HEADER ? -1 : 0;
}
/* The same sample already stored, or 0. */
SONG_SMALL static unsigned existing(const struct song_pool *p, const uint8_t *data, uint32_t bytes, uint32_t frames,
                                    uint32_t crc, uint32_t self)
{
    for (unsigned i = 0; i < p->count; ++i) {
        const struct pool_extent *e = &p->extent[i];
        struct pool_header h;
        if (e->type != POOL_SAMPLE || e->length != bytes || e->sector * POOL_SECTOR == self || pool_header(p, e, &h) ||
            h.crc != crc || h.extra[0] != frames) continue;
        const uint8_t *m = p->flash->map(pool_address(e), bytes);
        if (m && !memcmp(m, data, bytes)) return e->id;
    }
    struct kit_sample s;
    for (unsigned pad = 0; pad < KIT_PADS; ++pad)
        if (!kit_sample(p, KIT_LEGACY + pad, &s) && s.bytes == bytes && s.frames == frames && !memcmp(s.data, data, bytes))
            return KIT_LEGACY + pad;
    return 0;
}
SONG_SMALL static void sample_done(struct kit_import *k, struct song_pool *p)
{
    const struct bank_entry *e = &((const struct bank_header *)(const void *)k->header)->entry[k->sample];
    pool_flush(&k->w);
    const uint8_t *data = p->flash->map(k->w.start + POOL_HEADER, e->bytes);
    unsigned same = data ? existing(p, data, e->bytes, e->frames, ~k->w.crc, k->w.start) : 0;
    k->w.header.extra[0] = e->frames;
    k->open = 0;
    if (!chunks_ok(data, e->bytes)) { k->error = -1; return; }
    if (same) { k->kit.sample[k->sample] = (uint16_t)same; return; } /* stored once: this copy is never committed */
    if (pool_end(&k->w)) { k->error = k->w.error; return; }
    k->kit.sample[k->sample] = k->w.header.id;
    k->made[k->made_count++] = k->w.header.id;
}
SONG_SMALL int kit_import_write(struct kit_import *k, struct song_pool *p, uint32_t offset, const uint8_t *data, uint32_t bytes)
{
    if (k->error || offset != k->position || bytes > k->total - offset) return k->error = -1;
    const struct bank_header *h = (const struct bank_header *)(const void *)k->header;
    static const uint8_t zero = 0;
    for (uint32_t i = 0; i < bytes; ++i) /* the bank CRC, as the stock format takes it */
        k->crc = pool_crc(k->crc, offset + i >= 12 && offset + i < 20 ? &zero : data + i, 1);
    while (bytes && !k->error) {
        if (k->position < KIT_BANK_HEADER) {
            uint32_t n = KIT_BANK_HEADER - k->position < bytes ? KIT_BANK_HEADER - k->position : bytes;
            memcpy(k->header + k->position, data, n);
            k->position += n; data += n; bytes -= n;
            if (k->position == KIT_BANK_HEADER && (!bank_valid(h, k->total) || h->bytes != k->total)) k->error = -1;
            continue;
        }
        while (k->sample < KIT_PADS && k->position >= h->entry[k->sample].offset + h->entry[k->sample].bytes) ++k->sample;
        if (k->sample >= KIT_PADS) { k->error = -1; break; }
        const struct bank_entry *e = &h->entry[k->sample];
        if (!k->open) {
            unsigned sectors = pool_sectors(e->bytes);
            if (pool_largest(p) < sectors) (void)pool_compact(p, sectors);
            if (pool_begin(&k->w, p, POOL_SAMPLE, pool_next_id(p, POOL_SAMPLE), e->name, e->bytes)) { k->error = k->w.error; break; }
            k->open = 1;
        }
        uint32_t n = e->offset + e->bytes - k->position < bytes ? e->offset + e->bytes - k->position : bytes;
        pool_put(&k->w, data, n);
        if (k->w.error) { k->error = k->w.error; break; }
        k->position += n; data += n; bytes -= n;
        if (k->position == e->offset + e->bytes) { sample_done(k, p); ++k->sample; }
    }
    return k->error;
}
SONG_SMALL void kit_import_cancel(struct kit_import *k, struct song_pool *p)
{
    for (unsigned i = 0; i < k->made_count; ++i) (void)pool_delete(p, POOL_SAMPLE, k->made[i]);
    k->made_count = 0;
    k->open = 0;
    k->error = -1;
}
SONG_SMALL int kit_import_finish(struct kit_import *k, struct song_pool *p, const char *name, unsigned *id)
{
    const struct bank_header *h = (const struct bank_header *)(const void *)k->header;
    if (k->error || k->position != k->total || ~k->crc != h->crc) { kit_import_cancel(k, p); return -1; }
    *id = kit_next_id(p);
    if (kit_write(p, *id, name, &k->kit)) { kit_import_cancel(k, p); return -1; }
    return 0;
}

/* Reading a kit back out as a stock bank: the header made here, then each
 * sample's data, pad by pad. */
SONG_SMALL static void bank_of(const struct song_pool *p, const struct kit *k, struct bank_header *h, uint32_t crc)
{
    memset(h, 0, sizeof *h);
    *h = (struct bank_header){BANK_MAGIC, 1, KIT_BANK_HEADER, crc, 0, BANK_RATE, {0, 0}, {{0}}, {0}};
    for (unsigned pad = 0; pad < 16; ++pad) {
        struct kit_sample s;
        h->entry[pad].offset = h->bytes;
        if (pad >= KIT_PADS || kit_sample(p, k->sample[pad], &s)) continue;
        h->entry[pad] = (struct bank_entry){h->bytes, s.frames, s.bytes, {0}};
        memcpy(h->entry[pad].name, s.name, 15);
        h->bytes += s.bytes;
    }
}
SONG_SMALL uint32_t kit_bank_bytes(const struct song_pool *p, const struct kit *k)
{
    struct bank_header h;
    bank_of(p, k, &h, 0);
    return h.bytes;
}
SONG_SMALL int kit_bank_read(const struct song_pool *p, const struct kit *k, uint32_t crc, uint32_t offset, uint8_t *out, uint32_t bytes)
{
    struct bank_header h;
    bank_of(p, k, &h, crc);
    if (offset > h.bytes || bytes > h.bytes - offset) return -1;
    for (; bytes; ++offset, --bytes) {
        if (offset < KIT_BANK_HEADER) { *out++ = ((const uint8_t *)&h)[offset]; continue; }
        unsigned pad = 0;
        while (pad < KIT_PADS - 1U && offset >= h.entry[pad].offset + h.entry[pad].bytes) ++pad;
        struct kit_sample s;
        if (kit_sample(p, k->sample[pad], &s)) return -1;
        *out++ = s.data[offset - h.entry[pad].offset];
    }
    return 0;
}
SONG_SMALL uint32_t kit_bank_crc(const struct song_pool *p, const struct kit *k)
{
    struct bank_header h;
    bank_of(p, k, &h, 0);
    uint32_t crc = pool_crc(~0U, &h, KIT_BANK_HEADER); /* crc and commit are zero here */
    for (unsigned pad = 0; pad < KIT_PADS; ++pad) {
        struct kit_sample s;
        if (!kit_sample(p, k->sample[pad], &s)) crc = pool_crc(crc, s.data, s.bytes);
    }
    return ~crc;
}
#endif
