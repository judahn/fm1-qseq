/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* The kit library. See kits.h. */
#include "kits.h"
#include "song_config.h"
#include <string.h>
#if SONG_UI
#include "song.h" /* SONG_SMALL */

#include "kit_bank.h"

SONG_SMALL static const struct bank_header *legacy(const struct song_pool *p, const uint8_t **base)
{
    const struct pool_extent *e = pool_find(p, POOL_LEGACY, 0);
    const uint8_t *m = e ? p->flash->map(pool_address(e), e->length) : NULL;
    if (!m || !bank_valid((const struct bank_header *)(const void *)m, e->length)) return NULL;
    *base = m;
    return (const struct bank_header *)(const void *)m;
}
SONG_SMALL int kit_sample(const struct song_pool *p, unsigned ref, struct kit_sample *out)
{
    memset(out, 0, sizeof *out);
    if (ref >= KIT_LEGACY) {
        const uint8_t *base;
        const struct bank_header *h = legacy(p, &base);
        if (!h || ref - KIT_LEGACY >= KIT_PADS || !h->entry[ref - KIT_LEGACY].frames) return -1;
        const struct bank_entry *e = &h->entry[ref - KIT_LEGACY];
        *out = (struct kit_sample){base + e->offset, e->bytes, e->frames, {0}};
        memcpy(out->name, e->name, 15);
        return 0;
    }
    const struct pool_extent *e = ref ? pool_find(p, POOL_SAMPLE, ref) : NULL;
    struct pool_header h;
    if (!e || pool_header(p, e, &h)) return -1;
    *out = (struct kit_sample){p->flash->map(pool_address(e), e->length), e->length, h.extra[0], {0}};
    memcpy(out->name, h.name, 15);
    return out->data ? 0 : -1;
}
/* Every kit lives in one table object (POOL_KIT, id 1): a count, then per
 * kit its id, name and record, in id order. A kit costs 84 bytes there. */
struct kit_entry { uint16_t id; char name[16]; struct kit kit; };
_Static_assert(sizeof(struct kit_entry) == 84, "kit table entry");
static struct kit_entry table[KIT_MAX];
/* The table's kit count, 0 when there is none, or -1 when it is there but
 * damaged: its CRC, or a count that doesn't match its length. */
SONG_SMALL static int load_table(const struct song_pool *p)
{
    const struct pool_extent *e = pool_find(p, POOL_KIT, 1);
    uint8_t n = 0;
    if (!e) return 0;
    if (pool_check(p, e) || p->flash->read(pool_address(e), &n, 1) || n > KIT_MAX || e->length != 1U + n * sizeof table[0] ||
        p->flash->read(pool_address(e) + 1U, table, n * sizeof table[0])) return -1;
    return n;
}
SONG_SMALL static unsigned load(const struct song_pool *p) { int n = load_table(p); return n > 0 ? (unsigned)n : 0U; }
SONG_SMALL static int store(struct song_pool *p, unsigned n)
{
    struct pool_writer w;
    if (!n) return pool_find(p, POOL_KIT, 1) ? pool_delete(p, POOL_KIT, 1) : 0;
    if (pool_begin(&w, p, POOL_KIT, 1, "KITS", 1U + n * sizeof table[0])) return w.error;
    const uint8_t count = (uint8_t)n;
    pool_put(&w, &count, 1);
    pool_put(&w, table, n * sizeof table[0]);
    return pool_end(&w);
}
SONG_SMALL static int find(unsigned n, unsigned id)
{
    for (unsigned i = 0; i < n; ++i) if (table[i].id == id) return (int)i;
    return -1;
}
SONG_SMALL unsigned kit_count(const struct song_pool *p) { return load(p); }
SONG_SMALL unsigned kit_id_at(const struct song_pool *p, unsigned index) { return index < load(p) ? table[index].id : 0U; }
SONG_SMALL unsigned kit_next_id(const struct song_pool *p)
{
    unsigned n = load(p), id = 1;
    for (unsigned i = 0; i < n; ++i) if (table[i].id >= id) id = table[i].id + 1U;
    return id;
}
SONG_SMALL int kit_name(const struct song_pool *p, unsigned id, char out[17])
{
    int at = find(load(p), id);
    memset(out, 0, 17);
    if (at >= 0) memcpy(out, table[at].name, 16);
    return at < 0 ? -1 : 0;
}
SONG_SMALL int kit_read(const struct song_pool *p, unsigned id, struct kit *k)
{
    int at = find(load(p), id);
    if (at >= 0) *k = table[at].kit;
    return at < 0 ? -1 : 0;
}
SONG_SMALL int kit_write(struct song_pool *p, unsigned id, const char *name, const struct kit *k)
{
    unsigned n = load(p);
    int at = find(n, id);
    if (at < 0) {
        if (n >= KIT_MAX || !id) return -1;
        for (at = (int)n; at > 0 && table[at - 1].id > id; --at) table[at] = table[at - 1];
        ++n;
    }
    memset(&table[at], 0, sizeof table[at]);
    table[at].id = (uint16_t)id;
    for (unsigned i = 0; name && name[i] && i < sizeof table[at].name; ++i) table[at].name[i] = name[i];
    table[at].kit = *k;
    return store(p, n);
}
SONG_SMALL unsigned kit_refs(const struct song_pool *p, unsigned ref)
{
    unsigned refs = 0, n = load(p);
    for (unsigned i = 0; i < n; ++i)
        for (unsigned pad = 0; pad < KIT_PADS; ++pad)
            if (ref >= KIT_LEGACY ? table[i].kit.sample[pad] >= KIT_LEGACY : table[i].kit.sample[pad] == ref) { ++refs; break; }
    return refs;
}
SONG_SMALL static void drop_unused(struct song_pool *p, unsigned ref)
{
    if (!ref || kit_refs(p, ref)) return;
    if (ref >= KIT_LEGACY) (void)pool_delete(p, POOL_LEGACY, 0);
    else (void)pool_delete(p, POOL_SAMPLE, ref);
}
SONG_SMALL int kit_delete(struct song_pool *p, unsigned id)
{
    unsigned n = load(p);
    int at = find(n, id);
    if (at < 0) return -1;
    const struct kit k = table[at].kit;
    memmove(&table[at], &table[at + 1], (n - (unsigned)at - 1U) * sizeof table[0]);
    if (store(p, n - 1U)) return -1;
    for (unsigned pad = 0; pad < KIT_PADS; ++pad) drop_unused(p, k.sample[pad]);
    return 0;
}
SONG_SMALL unsigned kit_tidy(struct song_pool *p, const char *name)
{
    /* A damaged table, or samples with no table at all: the library doesn't
     * read whole, so nothing here can be sure a sample is unused. Leave it. */
    int kits = load_table(p);
    for (unsigned i = 0; !kits && i < p->count; ++i)
        if (p->extent[i].type == POOL_SAMPLE) kits = -1;
    if (kits < 0) return 0;
    unsigned made = 0;
    const uint8_t *base;
    const struct bank_header *h = legacy(p, &base);
    if (h && !kit_refs(p, KIT_LEGACY)) {
        struct kit k;
        memset(&k, 0, sizeof k);
        for (unsigned pad = 0; pad < KIT_PADS; ++pad)
            if (h->entry[pad].frames) k.sample[pad] = (uint16_t)(KIT_LEGACY + pad);
        made = kit_next_id(p);
        if (kit_write(p, made, name, &k)) made = 0;
    }
    for (unsigned i = 0; i < p->count;) /* samples left by an import cut short */
        if (p->extent[i].type == POOL_SAMPLE && !kit_refs(p, p->extent[i].id) &&
            !pool_delete(p, POOL_SAMPLE, p->extent[i].id)) continue;
        else ++i;
    return made;
}

#endif
