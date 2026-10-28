/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* song-mode: the storage side of transfers to and from a computer (phase
 * 5b), included by song_store.c. A song sent over is checked whole first,
 * then written like a save: a new copy, committed, then the old one erased. */

struct song_pool *song_store_pool(void) { return &pool; }
unsigned song_store_busy(void) { return importing; }

SONG_SMALL int song_store_put(unsigned id, const uint8_t *data, uint32_t bytes, unsigned *made)
{
    const uint8_t *block = NULL;
    uint32_t block_bytes = 0;
    if (song_port_player()->playing) return SONG_STORE_PLAYING;
    if (importing) return SONG_STORE_BUSY;
    if (song_file_check(data, bytes, &block, &block_bytes) || !fill(groove_project_scratch(), block, block_bytes))
        return GROOVE_PROJECT_INVALID;
    if (!id) id = pool_next_id(&pool, POOL_SONG);
    int r = room(pool_sectors(bytes));
    if (r) return r;
    char name[13] = {0};
    memcpy(name, data + 3 + SOUNDS, 12); /* after the version and the sounds block */
    struct pool_writer w;
    if (pool_begin(&w, &pool, POOL_SONG, id, name, bytes)) return w.error;
    w.header.extra[0] = id;
    pool_put(&w, data, bytes);
    r = pool_end(&w);
    if (r) return r;
    *made = id;
    if (id == song_port_song()->id) song_port_song()->id = 0; /* SAVE now makes a new song, not this one over again */
    return 0;
}

/* Kits: the samples arrive through the stock calls (12 to 14 in
 * transfer.c), and this ends the import as a library kit. */
SONG_SMALL int song_store_kit_finish(const char name[16], const uint8_t *params, unsigned *made)
{
    char text[17] = {0};
    if (!importing) return GROOVE_PROJECT_INVALID;
    memcpy(text, name, 16);
    if (params) {
        memcpy(import.kit.params, params, sizeof import.kit.params);
        import.kit.has_params = 1;
    }
    unsigned full = kit_count(&pool) >= KIT_MAX;
    int r = kit_import_finish(&import, &pool, text, made);
    importing = 0;
    bind(kit_id); /* samples may have moved */
    return !r ? 0 : full ? SONG_STORE_FULL : GROOVE_PROJECT_INVALID;
}
SONG_SMALL int song_store_kit_set(unsigned id, const char name[16], const uint8_t *params)
{
    struct kit k;
    char text[17] = {0};
    if (song_port_player()->playing) return SONG_STORE_PLAYING;
    if (importing) return SONG_STORE_BUSY;
    if (kit_read(&pool, id, &k)) return GROOVE_PROJECT_INVALID;
    memcpy(text, name, 16);
    k.has_params = params != NULL;
    if (params) memcpy(k.params, params, sizeof k.params);
    int r = kit_write(&pool, id, text, &k);
    if (!r && id == kit_id) bind(id); /* the pad settings, now */
    return r;
}
SONG_SMALL void song_store_clear(void)
{
    for (unsigned id; (id = nth(POOL_SONG, 0)) != 0;)
        if (pool_delete(&pool, POOL_SONG, id)) break;
    (void)pool_delete(&pool, POOL_RECOVERY, 1);
    song_port_song()->id = 0;
    waiting = 0;
}
