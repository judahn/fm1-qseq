/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* song-mode: the rest of the storage glue, included by song_store.c. The
 * song and kit lists, the recovery copy, the memory gauge, and the stock
 * sample API served by the kit library. */

/* Songs and kits by position, in id order. */
SONG_SMALL static unsigned nth(unsigned type, unsigned index)
{
    unsigned last = 0;
    for (unsigned k = 0; k <= index; ++k) {
        unsigned best = 0;
        for (unsigned i = 0; i < pool.count; ++i)
            if (pool.extent[i].type == type && pool.extent[i].id > last && (!best || pool.extent[i].id < best))
                best = pool.extent[i].id;
        if (!best) return 0;
        last = best;
    }
    return last;
}
SONG_SMALL static unsigned count_of(unsigned type)
{
    unsigned n = 0;
    for (unsigned i = 0; i < pool.count; ++i) n += pool.extent[i].type == type;
    return n;
}
unsigned song_store_count(void) { return count_of(POOL_SONG); }
unsigned song_store_id(unsigned index) { return nth(POOL_SONG, index); }
unsigned song_store_kits(void) { return kit_count(&pool); }
unsigned song_store_kit_id(unsigned index) { return kit_id_at(&pool, index); }
unsigned song_store_kit(void) { return kit_id; }
SONG_SMALL static void name_of(unsigned type, unsigned id, char out[17])
{
    const struct pool_extent *e = pool_find(&pool, type, id);
    struct pool_header h;
    memset(out, 0, 17);
    if (e && !pool_header(&pool, e, &h)) memcpy(out, h.name, 16);
}
void song_store_name(unsigned id, char out[17]) { name_of(POOL_SONG, id, out); }
void song_store_kit_name(unsigned id, char out[17]) { (void)kit_name(&pool, id, out); }
uint32_t song_store_bytes(unsigned id)
{
    const struct pool_extent *e = pool_find(&pool, POOL_SONG, id);
    return e ? e->length : 0;
}
/* A kit change only binds the pads again. */
SONG_SMALL int song_store_use_kit(unsigned id)
{
    if (song_port_player()->playing) return SONG_STORE_PLAYING;
    struct kit k;
    if (kit_read(&pool, id, &k)) return -1;
    bind(id);
    song_port_song()->kit = (uint16_t)id;
    return 0;
}
SONG_SMALL int song_store_delete_kit(unsigned id)
{
    if (song_port_player()->playing) return SONG_STORE_PLAYING;
    int r = kit_delete(&pool, id);
    if (!r && id == kit_id) bind(kit_id_at(&pool, 0));
    return r;
}

/* The recovery copy: when playback stops and the song isn't what was last
 * written or saved, at most every 15 s. A stop too soon after leaves it
 * waiting, and the poll writes it once the time has passed. */
enum { RECOVERY_GAP = 15000 };
SONG_SMALL static void recover_maybe(uint32_t now)
{
    struct song *s = song_port_song();
    if (song_port_player()->playing || song_port_recorder()->active || importing) return;
    capture();
    uint32_t crc = song_file_crc(s, sounds, SOUNDS);
    if (!song_recovery_due(&recovery, crc, now, RECOVERY_GAP)) return;
    if (!write(POOL_RECOVERY, 1)) {
        recovery.written_crc = crc;
        recovery.written_at = now;
        recovery.any = 1;
    }
}
SONG_SMALL void song_store_poll(uint32_t now)
{
    unsigned playing = song_port_player()->playing;
    if (!baseline && !playing) { /* the song as it booted needs no copy */
        baseline = 1;
        capture();
        recovery.saved_crc = song_file_crc(song_port_song(), sounds, SOUNDS);
    }
    if (was_playing && !playing) recover_maybe(now);
    else if (recovery.pending && !playing && now - checked_at >= 1000U) { checked_at = now; recover_maybe(now); }
    was_playing = playing;
}
unsigned song_store_recovery(void) { return waiting; }
SONG_SMALL int song_store_restore(void)
{
    const struct pool_extent *e = pool_find(&pool, POOL_RECOVERY, 1);
    struct pool_header h;
    if (!e || pool_header(&pool, e, &h)) return -1;
    int r = load_from(POOL_RECOVERY, 1);
    if (!r) song_port_song()->id = (uint16_t)(pool_find(&pool, POOL_SONG, h.extra[0]) ? h.extra[0] : 0);
    waiting = 0;
    return r;
}
void song_store_skip_recovery(void) { waiting = 0; }

void song_store_gauge(struct song_gauge *g)
{
    g->pool = (uint32_t)(pool.end - pool.first - (pool.skip_end - pool.skip_first)) * POOL_SECTOR;
    g->songs = (uint32_t)(pool_used(&pool, POOL_SONG) + pool_used(&pool, POOL_RECOVERY)) * POOL_SECTOR;
    g->samples = (uint32_t)(pool_used(&pool, POOL_SAMPLE) + pool_used(&pool, POOL_LEGACY) + pool_used(&pool, POOL_KIT)) *
                 POOL_SECTOR;
    g->free = (uint32_t)pool_free(&pool) * POOL_SECTOR;
    g->events = song_port_song()->events;
}

/* The stock sample API (instrument.h), now over the kit library: the
 * transfer path sends a stock bank, which becomes samples and a kit, and
 * reading back gives the current kit as a stock bank. */
void groove_samples_init(void) { song_store_init(); }
void groove_samples_poll(void) {}
unsigned groove_samples_status(void) { return importing ? 3U : kit_id ? 2U : 0U; }
SONG_SMALL static uint32_t bank_crc_now(const struct kit *k)
{
    if (bank_crc_for != kit_id) { bank_crc = kit_bank_crc(&pool, k); bank_crc_for = kit_id; }
    return bank_crc;
}
unsigned groove_samples_size(void)
{
    struct kit k;
    return kit_id && !kit_read(&pool, kit_id, &k) ? kit_bank_bytes(&pool, &k) : 0;
}
SONG_SMALL int groove_samples_begin(unsigned bytes)
{
    song_port_stop(); /* flash is only written while stopped */
    importing = !kit_import_begin(&import, bytes);
    return importing ? 0 : GROOVE_PROJECT_INVALID;
}
SONG_SMALL int groove_samples_write(unsigned offset, const uint8_t *data, unsigned bytes)
{
    if (!importing || !data || !bytes || bytes > 128) return GROOVE_PROJECT_INVALID;
    if (!kit_import_write(&import, &pool, offset, data, bytes)) return 0;
    kit_import_cancel(&import, &pool);
    importing = 0;
    return GROOVE_PROJECT_INVALID;
}
SONG_SMALL int groove_samples_finish(void)
{
    unsigned id = 0;
    char name[17] = "KIT ";
    name[4] = (char)('0' + kit_next_id(&pool) / 10U % 10U);
    name[5] = (char)('0' + kit_next_id(&pool) % 10U);
    if (!importing) return GROOVE_PROJECT_INVALID;
    for (unsigned t = 0; t < GROOVE_TRACKS && !import.kit.has_params; ++t) /* the pads as a sampler track has them */
        if (groove_source_type(t) == GROOVE_SAMPLE) {
            for (unsigned pad = 0; pad < KIT_PADS; ++pad) {
                uint8_t values[GROOVE_TONE_PARAMS];
                if (!groove_tone_copy(t, pad, values)) memcpy(import.kit.params[pad], values, KIT_PARAMS);
            }
            import.kit.has_params = 1;
        }
    int r = kit_import_finish(&import, &pool, name, &id);
    importing = 0;
    if (r) return GROOVE_PROJECT_INVALID;
    bind(id);
    song_port_song()->kit = (uint16_t)id;
    return 0;
}
void groove_samples_cancel(void)
{
    if (importing) kit_import_cancel(&import, &pool);
    importing = 0;
}
SONG_SMALL int groove_samples_read(unsigned offset, uint8_t *data, unsigned bytes)
{
    struct kit k;
    if (!kit_id || !data || !bytes || bytes > 64 || kit_read(&pool, kit_id, &k)) return GROOVE_PROJECT_INVALID;
    return kit_bank_read(&pool, &k, bank_crc_now(&k), offset, data, bytes) ? GROOVE_PROJECT_INVALID : 0;
}

#ifdef FM1_OPEN_WEB
/* For the headless check: the store's state, by number. */
SONG_SMALL uint32_t song_store_probe(unsigned what)
{
    const struct pool_extent *e = pool_find(&pool, POOL_RECOVERY, 1);
    struct kit k;
    struct kit_sample s;
    const uint8_t *base = device_flash.map(0, 1);
    switch (what) {
    case 0: return song_store_count();
    case 1: return song_port_song()->id;
    case 2: return song_store_kits();
    case 3: return kit_id;
    case 4: return waiting;
    case 5: return (uint32_t)pool_free(&pool) * POOL_SECTOR;
    case 6: return e ? e->generation : 0;
    case 7: return count_of(POOL_SAMPLE);
    case 16: capture(); return song_file_crc(song_port_song(), sounds, SOUNDS);
    default:
        if (what < 16 && !kit_read(&pool, kit_id, &k) && !kit_sample(&pool, k.sample[what - 8U], &s)) return (uint32_t)(s.data - base);
        return 0;
    }
}
/* A copy of the current kit with its last pad's sound changed, sent
 * through the transfer path's calls 128 bytes at a time. */
#include <stdlib.h>
SONG_SMALL int sim_song_kit_copy_run(void)
{
    uint32_t bytes = groove_samples_size();
    uint8_t *bank = bytes ? malloc(bytes) : NULL;
    int r = bank ? 0 : -1;
    for (uint32_t at = 0; !r && at < bytes; at += 64) r = groove_samples_read(at, bank + at, bytes - at < 64 ? bytes - at : 64);
    if (!r) { /* the last used pad, a byte past its first chunk's header */
        uint32_t at = 0;
        for (unsigned pad = 0; pad < KIT_PADS; ++pad) {
            uint32_t offset, length;
            memcpy(&offset, bank + 32 + pad * 28, 4);
            memcpy(&length, bank + 32 + pad * 28 + 8, 4);
            if (length) at = offset + 9U;
        }
        bank[at] ^= 0x55;
        uint32_t zero = 0, crc;
        memcpy(bank + 12, &zero, 4);
        crc = ~pool_crc(~0U, bank, bytes);
        memcpy(bank + 12, &crc, 4);
        r = groove_samples_begin(bytes);
        for (uint32_t i = 0; !r && i < bytes; i += 128) r = groove_samples_write(i, bank + i, bytes - i < 128 ? bytes - i : 128);
        if (!r) r = groove_samples_finish();
    }
    free(bank);
    return r;
}
#endif
