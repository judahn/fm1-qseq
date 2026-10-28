/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#ifndef FM1_SONG_KIT_BANK_H
#define FM1_SONG_KIT_BANK_H
/* The stock sample bank format, shared by kits.c (a bank found in place)
 * and kit_bank.c (banks imported and read back). */
#include "kits.h"
#include <string.h>

/* The stock bank layout, 512 bytes ahead of its sample data. */
enum { BANK_MAGIC = 0x53314d46U, BANK_RATE = 32000, FRAMES_PER_CHUNK = 320 };
struct bank_entry { uint32_t offset, frames, bytes; char name[16]; };
struct bank_header {
    uint32_t magic, version, bytes, crc, commit, rate, reserved[2];
    struct bank_entry entry[16];
    uint8_t padding[32];
};
_Static_assert(sizeof(struct bank_header) == KIT_BANK_HEADER, "stock bank header");

/* The stock rules: entries back to back from the header on, 8 pads used,
 * whole SEA5 chunks, printable names. */
static inline int bank_valid(const struct bank_header *h, uint32_t limit)
{
    if (h->magic != BANK_MAGIC || h->version != 1 || h->rate != BANK_RATE || h->bytes < KIT_BANK_HEADER ||
        h->bytes > limit || h->reserved[0] || h->reserved[1]) return 0;
    uint32_t next = KIT_BANK_HEADER;
    for (unsigned i = 0; i < 16; ++i) {
        const struct bank_entry *e = &h->entry[i];
        if (e->offset != next || (i >= KIT_PADS && (e->frames || e->name[0])) || e->name[15] ||
            e->bytes != (e->frames + FRAMES_PER_CHUNK - 1U) / FRAMES_PER_CHUNK * KIT_CHUNK || e->bytes > h->bytes - next)
            return 0;
        for (unsigned n = 0; n < 15; ++n)
            if (e->name[n] && (e->name[n] < 32 || e->name[n] > 126)) return 0;
        next += e->bytes;
    }
    return next == h->bytes;
}
static inline int chunks_ok(const uint8_t *data, uint32_t bytes)
{
    for (uint32_t at = 0; data && at < bytes; at += KIT_CHUNK)
        if (data[at] != 1 || data[at + 1] != 0x45 || data[at + 2] != 20 || data[at + 3] != 0x5a) return 0;
    return data != NULL;
}
#endif
