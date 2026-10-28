/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Songs and kits to and from a computer (phase 5b, design 8b): commands 32
 * to 63 of the transfer protocol, reached from a marked hook in transfer.c.
 * The stock framing, retries and reply cache stay as they are. Requests
 * carry up to 132 bytes, replies up to 64. Errors come back as 64 - error:
 * 71 invalid, 72 out of memory, 84 the pool is full, 85 playing, 86 busy.
 *
 *   32 info                 41 kit list (index)
 *   33 song list (index)    42 kit read begin (id)
 *   34 song read begin (id) 43 kit pad settings (id)
 *   35 read (offset, n)     44 kit set (id, name, settings)
 *   36 song write begin     45 kit delete (id)
 *   37 song write (offset)  46 kit use (id)
 *   38 song write finish    47 kit finish (after stock 12 and 13)
 *   39 song delete (id)     48 sample list (index)
 *   40 song load (id)       49 cancel
 *                           50 song save (as new)
 */
#include "song_port.h"
#include "instrument.h"
#include "pool.h"
#include "kits.h"
#include "player.h"
#include <stdlib.h>
#include <string.h>
#if SONG_UI

enum { SONG_TRANSFER_MAX = 40960, READ_SONG = 1, READ_KIT };
static struct {
    uint8_t *buffer;              /* a song on its way in */
    uint32_t length, received;
    uint16_t id, read_id;
    uint8_t reading;
    uint32_t generation, bytes, crc; /* of what is being read */
    struct kit kit;
} t;

static unsigned get16(const uint8_t *p) { return p[0] | (unsigned)p[1] << 8; }
static uint32_t get32(const uint8_t *p) { return get16(p) | (uint32_t)get16(p + 2) << 16; }
static uint8_t *put16(uint8_t *p, unsigned v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); return p + 2; }
static uint8_t *put32(uint8_t *p, uint32_t v) { return put16(put16(p, v & 0xffffU), v >> 16); }
static void drop(void) { free(t.buffer); t.buffer = NULL; t.length = t.received = 0; }

SONG_SMALL static unsigned info(uint8_t *out)
{
    const struct song_pool *p = song_store_pool();
    uint8_t *at = out;
    struct kit_sample s;
    unsigned samples = 0;
    for (unsigned i = 0; i < p->count; ++i) samples += p->extent[i].type == POOL_SAMPLE;
    for (unsigned pad = 0; pad < KIT_PADS; ++pad) samples += !kit_sample(p, KIT_LEGACY + pad, &s); /* a stock bank in place */
    *at++ = 1; /* this protocol's version */
    at = put16(at, (unsigned)(p->end - p->first - (p->skip_end - p->skip_first)));
    at = put16(at, pool_free(p));
    at = put16(at, pool_largest(p));
    *at++ = (uint8_t)song_store_count(); *at++ = (uint8_t)song_store_kits(); *at++ = (uint8_t)samples;
    at = put16(at, song_port_song()->id);
    at = put16(at, song_store_kit());
    *at++ = (uint8_t)(song_port_player()->playing | song_store_busy() << 1 | song_store_recovery() << 2);
    at = put16(at, pool_used(p, POOL_SONG) + pool_used(p, POOL_RECOVERY));
    at = put16(at, pool_used(p, POOL_SAMPLE) + pool_used(p, POOL_LEGACY) + pool_used(p, POOL_KIT));
    *at++ = KIT_MAX;
    at = put16(at, SONG_EVENTS);
    at = put32(at, SONG_TRANSFER_MAX);
    return (unsigned)(at - out);
}
/* A song's header, or a kit as a stock bank, to read in pieces. A piece is
 * refused if the song or kit changed since the read began. */
SONG_SMALL static int read_begin(unsigned kind, unsigned id, uint8_t *out, unsigned *count)
{
    struct song_pool *p = song_store_pool();
    struct pool_header h;
    char name[17] = {0};
    t.reading = 0;
    if (kind == READ_SONG) {
        const struct pool_extent *e = pool_find(p, POOL_SONG, id);
        if (!e) return GROOVE_PROJECT_EMPTY;
        if (pool_header(p, e, &h) || pool_check(p, e)) return GROOVE_PROJECT_INVALID;
        t.generation = e->generation; t.bytes = e->length; t.crc = h.crc;
        memcpy(name, h.name, 16);
        memset(&t.kit, 0, sizeof t.kit);
    } else {
        if (kit_read(p, id, &t.kit)) return GROOVE_PROJECT_EMPTY;
        t.bytes = kit_bank_bytes(p, &t.kit); t.crc = kit_bank_crc(p, &t.kit);
        (void)kit_name(p, id, name);
    }
    t.reading = (uint8_t)kind; t.read_id = (uint16_t)id;
    uint8_t *at = put32(put32(out, t.bytes), t.crc);
    memcpy(at, name, 16);
    at[16] = t.kit.has_params;
    for (unsigned pad = 0; pad < KIT_PADS; ++pad) put16(at + 17 + pad * 2, kind == READ_KIT ? t.kit.sample[pad] : 0);
    *count = 8 + 17 + 16;
    return 0;
}
SONG_SMALL static int read_piece(uint32_t offset, unsigned n, uint8_t *out, unsigned *count)
{
    struct song_pool *p = song_store_pool();
    struct kit now;
    if (!n || n > 64 || offset > t.bytes || n > t.bytes - offset) return GROOVE_PROJECT_INVALID;
    if (t.reading == READ_SONG) {
        const struct pool_extent *e = pool_find(p, POOL_SONG, t.read_id);
        const uint8_t *m = e && e->generation == t.generation ? p->flash->map(pool_address(e) + offset, n) : NULL;
        if (!m) return GROOVE_PROJECT_INVALID;
        memcpy(out, m, n);
    } else if (t.reading != READ_KIT || kit_read(p, t.read_id, &now) || memcmp(&now, &t.kit, sizeof now) ||
               kit_bank_read(p, &t.kit, t.crc, offset, out, n))
        return GROOVE_PROJECT_INVALID;
    *count = n;
    return 0;
}
SONG_SMALL static int write_begin(unsigned id, uint32_t length)
{
    drop();
    if (length < 16 || length > SONG_TRANSFER_MAX) return GROOVE_PROJECT_INVALID;
    if (song_port_player()->playing) return SONG_STORE_PLAYING;
    if (pool_free(song_store_pool()) < pool_sectors(length)) return SONG_STORE_FULL; /* before sending it all */
    t.buffer = malloc(length);
    if (!t.buffer) return GROOVE_PROJECT_MEMORY;
    t.length = length; t.id = (uint16_t)id;
    return 0;
}
SONG_SMALL static int write_finish(uint32_t crc, uint8_t *out, unsigned *count)
{
    unsigned made = 0;
    int r = !t.buffer || t.received != t.length || ~pool_crc(~0U, t.buffer, t.length) != crc
                ? GROOVE_PROJECT_INVALID : song_store_put(t.id, t.buffer, t.length, &made);
    drop();
    if (!r) { put16(out, made); *count = 2; }
    return r;
}
SONG_SMALL static int kit_entry(unsigned index, uint8_t *out, unsigned *count)
{
    struct song_pool *p = song_store_pool();
    struct kit k;
    char name[17];
    unsigned id = song_store_kit_id(index), used = 0;
    if (!id) return GROOVE_PROJECT_EMPTY;
    if (kit_read(p, id, &k) || kit_name(p, id, name)) return GROOVE_PROJECT_INVALID;
    for (unsigned pad = 0; pad < KIT_PADS; ++pad) used |= (k.sample[pad] != 0) << pad;
    uint8_t *at = put16(out, id);
    memcpy(at, name, 16);
    at[16] = (uint8_t)used; at[17] = k.has_params;
    put32(at + 18, kit_bank_bytes(p, &k));
    *count = 2 + 18 + 4;
    return 0;
}
/* The pool's samples, then the pads of a stock bank found in place. */
SONG_SMALL static int sample_entry(unsigned index, uint8_t *out, unsigned *count)
{
    struct song_pool *p = song_store_pool();
    struct kit_sample s;
    unsigned ref = 0, n = 0;
    for (unsigned i = 0; i < p->count; ++i)
        if (p->extent[i].type == POOL_SAMPLE && n++ == index) ref = p->extent[i].id;
    for (unsigned pad = 0; !ref && pad < KIT_PADS; ++pad)
        if (!kit_sample(p, KIT_LEGACY + pad, &s) && n++ == index) ref = KIT_LEGACY + pad;
    if (!ref || kit_sample(p, ref, &s)) return GROOVE_PROJECT_EMPTY;
    uint8_t *at = put32(put32(put32(put16(out, ref), s.bytes), s.frames), ~pool_crc(~0U, s.data, s.bytes));
    *at++ = (uint8_t)kit_refs(p, ref);
    memcpy(at, s.name, 16);
    *count = 2 + 12 + 1 + 16;
    return 0;
}

SONG_SMALL int song_transfer_command(unsigned cmd, const uint8_t *data, unsigned n, uint8_t *out, unsigned *count)
{
    int r = GROOVE_PROJECT_INVALID;
    unsigned id = n >= 2 ? get16(data) : 0, made = 0;
    *count = 0;
    if (cmd == 32 && !n) { *count = info(out); return 0; }
    if (cmd == 33 && n == 1) {
        struct song_pool *p = song_store_pool();
        const struct pool_extent *e = pool_find(p, POOL_SONG, song_store_id(data[0]));
        struct pool_header h;
        if (!e) return GROOVE_PROJECT_EMPTY;
        if (pool_header(p, e, &h)) return GROOVE_PROJECT_INVALID;
        memcpy(put32(put32(put16(out, e->id), e->length), h.crc), h.name, 16);
        *count = 2 + 8 + 16;
        return 0;
    }
    if ((cmd == 34 || cmd == 42) && n == 2) return read_begin(cmd == 34 ? READ_SONG : READ_KIT, id, out, count);
    if (cmd == 35 && n == 5) return read_piece(get32(data), data[4], out, count);
    if (cmd == 36 && n == 6) return write_begin(id, get32(data + 2));
    if (cmd == 37 && n > 4 && n <= 132 && t.buffer && get32(data) == t.received && n - 4 <= t.length - t.received) {
        memcpy(t.buffer + t.received, data + 4, n - 4);
        t.received += n - 4;
        return 0;
    }
    if (cmd == 38 && n == 4) return write_finish(get32(data), out, count);
    if (cmd == 39 && n == 2) return song_store_delete(id);
    if (cmd == 40 && n == 2) return song_store_load(id);
    if (cmd == 41 && n == 1) return kit_entry(data[0], out, count);
    if (cmd == 43 && n == 2) {
        struct kit k;
        if (kit_read(song_store_pool(), id, &k)) return GROOVE_PROJECT_EMPTY;
        memcpy(out, k.params, sizeof k.params);
        *count = sizeof k.params;
        return 0;
    }
    if (cmd == 44 && n == 2 + 16 + 1 + 48) return song_store_kit_set(id, (const char *)data + 2, data[18] ? data + 19 : NULL);
    if (cmd == 45 && n == 2) return song_store_delete_kit(id);
    if (cmd == 46 && n == 2) return song_store_use_kit(id);
    if (cmd == 47 && n == 16 + 1 + 48) r = song_store_kit_finish((const char *)data, data[16] ? data + 17 : NULL, &made);
    if (cmd == 48 && n == 1) return sample_entry(data[0], out, count);
    if (cmd == 49 && !n) { drop(); t.reading = 0; return 0; }
    if (cmd == 50 && n == 1) {
        r = song_store_save(data[0]);
        made = song_port_song()->id;
    }
    if (!r && (cmd == 47 || cmd == 50)) { put16(out, made); *count = 2; }
    return r;
}
#endif
