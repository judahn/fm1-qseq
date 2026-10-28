/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Storage glue (phase 5a): the pool on the device's flash, songs with their
 * sounds, the kit library behind the stock sample API, and the recovery
 * copy. Flash is written only while stopped, since writes mute audio. */
#include "song_port.h"
#include "instrument.h"
#include "fm1_engine_control.h"
#include "fm1_device.h"
#include "sampledrum.h"
#include "pool.h"
#include "song_file.h"
#include "kits.h"
#include "undo.h"
#include "record.h"
#include <string.h>
#if SONG_UI

static int f_read(uint32_t at, void *data, size_t bytes) { return fm1_flash_read(at, data, bytes); }
static int f_write(uint32_t at, const void *data, size_t bytes) { return fm1_flash_write(at, data, bytes); }
static int f_erase(uint32_t at) { return fm1_flash_erase(at); }
static const uint8_t *f_map(uint32_t at, size_t bytes) { return fm1_flash_map(at, bytes); }
static const struct song_flash device_flash = {f_read, f_write, f_erase, f_map};
static struct song_pool pool;
static struct kit_import import;
static struct song_recovery recovery;
static unsigned kit_id, importing, waiting, was_playing, bank_crc_for, baseline;
static uint32_t bank_crc, checked_at;

/* The sounds block: the 8 sound setups, mute and solo, the shared effects,
 * the arpeggiators and the tuning, as the stock project holds them. */
enum { SOUNDS = 1 + GROOVE_TRACKS * sizeof(struct groove_preset) + 2 + 4 + GROOVE_TRACKS * 2 +
                GROOVE_TRACKS * sizeof(struct groove_tuning) };
static uint8_t sounds[SOUNDS];
SONG_SMALL static void capture(void)
{
    struct groove_project *p = groove_project_scratch();
    struct groove_tuning tuning[GROOVE_TRACKS];
    uint8_t *at = sounds;
    (void)groove_project_capture(p);
    memset(tuning, 0, sizeof tuning);
    (void)groove_arrangement_tuning_capture(tuning);
    *at++ = 1;
    memcpy(at, p->preset, sizeof p->preset); at += sizeof p->preset;
    *at++ = p->mute; *at++ = p->solo;
    memcpy(at, p->effects, 4); at += 4;
    memcpy(at, p->arp, sizeof p->arp); at += sizeof p->arp;
    memcpy(at, tuning, sizeof tuning);
}
/* The sounds block over the scratch project, checked. The tuning, or NULL. */
SONG_SMALL static const uint8_t *fill(struct groove_project *p, const uint8_t *data, uint32_t bytes)
{
    if (bytes != SOUNDS || data[0] != 1) return NULL;
    (void)groove_project_capture(p);
    const uint8_t *at = data + 1;
    memcpy(p->preset, at, sizeof p->preset); at += sizeof p->preset;
    p->mute = *at++; p->solo = *at++;
    memcpy(p->effects, at, 4); at += 4;
    memcpy(p->arp, at, sizeof p->arp); at += sizeof p->arp;
    if (groove_project_valid(p)) return at;
    memset(p->step, 0, sizeof p->step); /* stock pattern steps that don't fit these sounds: song mode doesn't play them */
    memset(p->sequence.lock, 255, sizeof p->sequence.lock);
    return groove_project_valid(p) ? at : NULL;
}
SONG_SMALL static int apply(const uint8_t *data, uint32_t bytes)
{
    struct groove_project *p = groove_project_scratch();
    const uint8_t *at = fill(p, data, bytes);
    if (!at) return -1;
    if (groove_project_apply(p)) return GROOVE_PROJECT_MEMORY;
    struct groove_tuning tuning[GROOVE_TRACKS];
    memcpy(tuning, at, sizeof tuning);
    (void)groove_arrangement_tuning_apply(tuning);
    return 0;
}
SONG_SMALL static void programs(struct song *s)
{
    for (unsigned t = 0; t < SONG_TRACKS; ++t)
        s->base_program[t] = (uint8_t)(((const struct groove_preset *)(const void *)(sounds + 1))[t].program + 1U);
}

/* Pads bind straight to the samples in mapped flash. */
SONG_SMALL static void bind(unsigned id)
{
    struct kit k;
    if (kit_read(&pool, id, &k)) memset(&k, 0, sizeof k), id = 0;
    fm1_engine_pause();
    groove_samples_stop();
    for (unsigned pad = 0; pad < KIT_PADS; ++pad) {
        struct kit_sample s;
        if (kit_sample(&pool, k.sample[pad], &s)) (void)sampledrum_bind(pad, NULL, 0, 0, NULL);
        else (void)sampledrum_bind(pad, s.data, s.bytes, s.frames, s.name);
    }
    fm1_engine_resume();
    for (unsigned t = 0; id && k.has_params && t < GROOVE_TRACKS; ++t) /* the kit's pad settings */
        if (groove_source_type(t) == GROOVE_SAMPLE)
            for (unsigned pad = 0; pad < KIT_PADS; ++pad) {
                uint8_t now[GROOVE_TONE_PARAMS];
                if (groove_tone_copy(t, pad, now)) continue;
                for (unsigned i = 0; i < KIT_PARAMS; ++i)
                    if (now[i] != k.params[pad][i]) groove_tone_edit(t, pad, i, (int)k.params[pad][i] - (int)now[i]);
            }
    kit_id = id;
    bank_crc_for = 0;
}
SONG_SMALL void song_store_init(void)
{
    importing = 0;
    pool_scan(&pool, &device_flash, SONG_POOL_FIRST, SONG_POOL_END, SONG_POOL_SKIP_FIRST, SONG_POOL_SKIP_END);
    unsigned made = kit_tidy(&pool, "STOCK BANK");
    struct song *s = song_port_song();
    struct kit k;
    unsigned id = s->kit && !kit_read(&pool, s->kit, &k) ? s->kit : made ? made : kit_id_at(&pool, 0);
    bind(id);
    s->kit = (uint16_t)kit_id;
    waiting = pool_find(&pool, POOL_RECOVERY, 1) != NULL;
}

/* Saving and loading. */
SONG_SMALL static int room(unsigned sectors)
{
    if (importing) return SONG_STORE_BUSY; /* a kit is coming in */
    if (pool_largest(&pool) >= sectors) return 0;
    if (pool_free(&pool) < sectors) return SONG_STORE_FULL;
    int r = pool_compact(&pool, sectors); /* samples may move: the pads bind again */
    bind(kit_id);
    return r ? SONG_STORE_FULL : 0;
}
SONG_SMALL static int write(unsigned type, unsigned id)
{
    struct song *s = song_port_song();
    uint32_t bytes = song_file_bytes(s, SOUNDS);
    int r = room(pool_sectors(bytes));
    if (r) return r;
    struct pool_writer w;
    char name[13] = {0};
    memcpy(name, s->name, 12);
    if (pool_begin(&w, &pool, type, id, name, bytes)) return w.error;
    w.header.extra[0] = s->id;
    song_file_write(&w, s, sounds, SOUNDS);
    return pool_end(&w);
}
SONG_SMALL int song_store_save(unsigned as_new)
{
    struct song *s = song_port_song();
    if (song_port_player()->playing) return SONG_STORE_PLAYING;
    unsigned id = as_new || !s->id ? pool_next_id(&pool, POOL_SONG) : s->id;
    if (!s->name[0] || (as_new && !memcmp(s->name, "SONG ", 5))) { /* a default name follows the slot */
        memcpy(s->name, "SONG        ", 12);
        s->name[5] = (char)('0' + id / 10U % 10U);
        s->name[6] = (char)('0' + id % 10U);
    }
    capture();
    programs(s);
    uint16_t before = s->id;
    s->id = (uint16_t)id;
    int r = write(POOL_SONG, id);
    if (r) { s->id = before; return r; }
    recovery.saved_crc = song_file_crc(s, sounds, SOUNDS);
    if (pool_find(&pool, POOL_RECOVERY, 1)) (void)pool_delete(&pool, POOL_RECOVERY, 1); /* saved: nothing to recover */
    waiting = 0;
    return 0;
}
SONG_SMALL static int load_from(unsigned type, unsigned id)
{
    struct song *s = song_port_song();
    const struct pool_extent *e = pool_find(&pool, type, id);
    const uint8_t *data = e ? device_flash.map(pool_address(e), e->length) : NULL, *block = NULL;
    uint32_t bytes = 0;
    if (song_port_player()->playing) return SONG_STORE_PLAYING;
    if (!e || pool_check(&pool, e) || song_file_read(s, data, e->length, &block, &bytes)) return -1;
    int r = apply(block, bytes);
    memcpy(sounds, block, SOUNDS);
    song_undo_init(song_port_undo());
    if (type == POOL_SONG) s->id = (uint16_t)id;
    struct kit k;
    if (!s->kit || kit_read(&pool, s->kit, &k)) s->kit = (uint16_t)kit_id; /* a kit not here: keep the one in use */
    bind(s->kit);
    recovery.saved_crc = type == POOL_SONG ? song_file_crc(s, sounds, SOUNDS) : 0;
    song_port_jump(0);
    song_chord_forget();
    return r;
}
SONG_SMALL int song_store_load(unsigned id) { return load_from(POOL_SONG, id); }
SONG_SMALL int song_store_new(int factory)
{
    struct song *s = song_port_song();
    if (song_port_player()->playing) return SONG_STORE_PLAYING;
    if (factory >= 0) {
        struct groove_project *p = groove_project_scratch();
        if (groove_factory_build((unsigned)factory, p) || groove_project_apply(p)) return -1;
    }
    struct song_undo *undo = s->undo;
    song_clear(s, 8, 12000);
    s->undo = undo;
    song_undo_init(undo);
    memcpy(s->name, "NEW SONG    ", 12);
    const char *name = factory >= 0 ? groove_factory_name((unsigned)factory) : NULL;
    if (name) memset(s->name, ' ', 12);
    for (unsigned i = 0; name && name[i] && i < 12; ++i) s->name[i] = name[i];
    s->kit = (uint16_t)kit_id;
    capture();
    programs(s);
    song_port_jump(0);
    song_chord_forget();
    return 0;
}
/* A stored song copied to `to`, with a new name in its header and payload. */
SONG_SMALL static int copy_song(unsigned id, unsigned to, const char name[12])
{
    const struct pool_extent *e = pool_find(&pool, POOL_SONG, id);
    struct pool_header h;
    if (!e || pool_header(&pool, e, &h) || pool_check(&pool, e)) return -1;
    uint32_t length = e->length, from = pool_address(e), at_name = 3U + SOUNDS;
    int r = room(e->sectors);
    if (r) return r;
    e = pool_find(&pool, POOL_SONG, id); /* compaction may have moved it */
    from = pool_address(e);
    char header_name[13] = {0};
    memcpy(header_name, name, 12);
    struct pool_writer w;
    if (pool_begin(&w, &pool, POOL_SONG, to, header_name, length)) return w.error;
    w.header.extra[0] = to;
    for (uint32_t at = 0; at < length && !w.error; at += 64) {
        uint8_t chunk[64];
        uint32_t n = length - at < 64 ? length - at : 64;
        w.error = pool.flash->read(from + at, chunk, n);
        for (uint32_t i = 0; i < n; ++i)
            if (at + i >= at_name && at + i < at_name + 12U) chunk[i] = (uint8_t)name[at + i - at_name];
        pool_put(&w, chunk, n);
    }
    return pool_end(&w);
}
SONG_SMALL int song_store_rename(unsigned id, const char name[12])
{
    struct song *s = song_port_song();
    if (song_port_player()->playing) return SONG_STORE_PLAYING;
    if (id == s->id) memcpy(s->name, name, 12);
    return id ? copy_song(id, id, name) : 0;
}
SONG_SMALL int song_store_duplicate(unsigned id)
{
    char name[13] = {0};
    song_store_name(id, name);
    unsigned length = (unsigned)strlen(name);
    while (length < 10) name[length++] = ' ';
    name[10] = ' '; name[11] = '2';
    return song_port_player()->playing ? SONG_STORE_PLAYING : copy_song(id, pool_next_id(&pool, POOL_SONG), name);
}
SONG_SMALL int song_store_delete(unsigned id)
{
    if (song_port_player()->playing) return SONG_STORE_PLAYING;
    if (id == song_port_song()->id) song_port_song()->id = 0;
    return pool_delete(&pool, POOL_SONG, id);
}
#include "song_store_list.inc"
#include "song_store_io.inc"
#endif
