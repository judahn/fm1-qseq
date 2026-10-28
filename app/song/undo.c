/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Undo journal. Layout: each transaction is [length][records][length], so
 * it can be walked both ways, and each record is [op][track][payload][size]
 * for the same reason. Lengths and sizes are 16-bit little-endian. */
#include "undo.h"
#include "song_config.h"
#include <string.h>
#if SONG_UI

static uint32_t get16(const uint8_t *p) { return p[0] | (uint32_t)p[1] << 8; }
static void put16(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }

SONG_SMALL void song_maps_read(const struct song *s, struct song_maps *m)
{
    memset(m, 0, sizeof *m);
    memcpy(m->tempo, s->tempo, sizeof m->tempo);
    memcpy(m->meter, s->meter, sizeof m->meter);
    memcpy(m->marker, s->marker, sizeof m->marker);
    m->loop_start = s->loop_start; m->loop_end = s->loop_end; m->bars = s->bars;
    m->tempos = s->tempos; m->meters = s->meters; m->markers = s->markers; m->loop_on = s->loop_on;
    m->swing = s->swing; m->swing_grid = s->swing_grid; m->swing_tracks = s->swing_tracks;
    m->key_root = s->key_root; m->key_scale = s->key_scale;
    memcpy(m->humanize_time, s->humanize_time, sizeof m->humanize_time);
    memcpy(m->humanize_velocity, s->humanize_velocity, sizeof m->humanize_velocity);
    memcpy(m->glitch, s->glitch, sizeof m->glitch);
}
/* Which groups of the maps differ, one bit each. */
enum { MAP_TEMPO = 1, MAP_METER = 2, MAP_MARKER = 4, MAP_LOOP = 8, MAP_BARS = 16, MAP_SWING = 32, MAP_KEY = 64,
       MAP_GLITCH = 128 };
#define DIFF(bit, field) (memcmp(&a->field, &b->field, sizeof a->field) ? (unsigned)(bit) : 0U)
SONG_SMALL static unsigned changed(const struct song_maps *a, const struct song_maps *b)
{
    return DIFF(MAP_TEMPO, tempo) | DIFF(MAP_TEMPO, tempos) | DIFF(MAP_METER, meter) | DIFF(MAP_METER, meters) |
           DIFF(MAP_MARKER, marker) | DIFF(MAP_MARKER, markers) | DIFF(MAP_LOOP, loop_start) |
           DIFF(MAP_LOOP, loop_end) | DIFF(MAP_LOOP, loop_on) | DIFF(MAP_BARS, bars) | DIFF(MAP_SWING, swing) |
           DIFF(MAP_SWING, swing_grid) | DIFF(MAP_SWING, swing_tracks) | DIFF(MAP_KEY, key_root) |
           DIFF(MAP_KEY, key_scale) | DIFF(MAP_SWING, humanize_time) | DIFF(MAP_SWING, humanize_velocity) |
           DIFF(MAP_GLITCH, glitch);
}
#undef DIFF
/* Writes back only the groups in `mask`, so a step never moves what it
 * didn't change. */
SONG_SMALL static void maps_write(struct song *s, const struct song_maps *m, unsigned mask)
{
    if (mask & MAP_TEMPO) { memcpy(s->tempo, m->tempo, sizeof m->tempo); s->tempos = m->tempos; }
    if (mask & MAP_METER) { memcpy(s->meter, m->meter, sizeof m->meter); s->meters = m->meters; }
    if (mask & MAP_MARKER) { memcpy(s->marker, m->marker, sizeof m->marker); s->markers = m->markers; }
    if (mask & MAP_LOOP) { s->loop_start = m->loop_start; s->loop_end = m->loop_end; s->loop_on = m->loop_on; }
    if (mask & MAP_BARS) s->bars = m->bars;
    if (mask & MAP_SWING) {
        s->swing = m->swing; s->swing_grid = m->swing_grid; s->swing_tracks = m->swing_tracks;
        memcpy(s->humanize_time, m->humanize_time, sizeof m->humanize_time);
        memcpy(s->humanize_velocity, m->humanize_velocity, sizeof m->humanize_velocity);
    }
    if (mask & MAP_GLITCH) memcpy(s->glitch, m->glitch, sizeof m->glitch);
    if (mask & MAP_KEY) { s->key_root = m->key_root; s->key_scale = m->key_scale; }
    song_tempo_refresh(s);
    ++s->revision;
}

SONG_SMALL void song_undo_init(struct song_undo *u) { memset(u, 0, sizeof *u); }

/* Make room by dropping the oldest finished transactions. */
SONG_SMALL static int room(struct song_undo *u, uint32_t bytes)
{
    while (u->used + bytes > SONG_UNDO_BYTES) {
        uint32_t oldest = get16(u->data) + 4U;
        if (!u->done || oldest > u->done || (u->depth && oldest > u->open_at)) { u->lost = 1; return -1; }
        memmove(u->data, u->data + oldest, u->used - oldest);
        u->used -= oldest; u->done -= oldest;
        if (u->depth) u->open_at -= oldest;
        if (u->levels) --u->levels;
    }
    return 0;
}
/* The first record of a transaction drops what could have been redone and
 * opens the transaction, so empty polls cost nothing and keep redo. */
SONG_SMALL static int open_transaction(struct song_undo *u)
{
    if (!u->pending) return 0;
    u->pending = 0;
    u->used = u->open_at = u->done;
    if (room(u, 2)) return -1;
    u->open_at = u->used;
    put16(u->data + u->used, 0);
    u->used += 2;
    return 0;
}
SONG_SMALL static void append(struct song_undo *u, unsigned op, unsigned track, const void *payload, uint32_t bytes)
{
    if (!u->depth || u->paused || u->lost || open_transaction(u) || room(u, bytes + 4U)) return;
    uint8_t *p = u->data + u->used;
    p[0] = (uint8_t)op; p[1] = (uint8_t)track;
    memcpy(p + 2, payload, bytes);
    put16(p + 2 + bytes, bytes + 4U);
    u->used += bytes + 4U;
}
SONG_SMALL void song_undo_record(struct song_undo *u, unsigned op, unsigned track, const struct song_event *e,
                                 uint32_t extra)
{
    if (op == SONG_UNDO_SHIFT) {
        uint32_t shift[2] = {e ? e->at : 0, extra}; /* from, then delta */
        append(u, op, 0, shift, sizeof shift);
    } else append(u, op, track, e, sizeof *e);
}
SONG_SMALL void song_undo_begin(struct song_undo *u, const struct song *s)
{
    if (u->depth++) return;
    u->lost = 0;
    u->pending = 1;
    song_maps_read(s, &u->before);
}
/* The last step holds just a maps record with this mask, and nothing waits
 * to be redone. */
SONG_SMALL static int same_kind(const struct song_undo *u, unsigned mask)
{
    uint32_t size = sizeof(struct song_maps) + 4U, at = u->done - 2U - size;
    return u->used == u->done && u->done >= size + 4U && get16(u->data + u->done - 2U) == size &&
           u->data[at] == SONG_UNDO_MAPS && u->data[at + 1U] == mask;
}
SONG_SMALL int song_undo_end(struct song_undo *u, const struct song *s)
{
    if (!u->depth) return 0;
    if (u->depth > 1) { --u->depth; return 0; }
    struct song_maps now;
    song_maps_read(s, &now);
    unsigned mask = changed(&now, &u->before);
    /* On its own, a loop change is the selection moving, not an edit. A
     * settings change right after one like it joins that step, so a knob
     * turned over many polls is one step. */
    if (u->pending && (mask == MAP_LOOP || (mask && same_kind(u, mask)))) mask = 0;
    if (mask) append(u, SONG_UNDO_MAPS, mask, &u->before, sizeof u->before);
    u->depth = 0;
    if (u->pending && !u->lost) { u->pending = 0; return 0; } /* nothing changed */
    if (u->lost || room(u, 2)) { u->used = u->done = 0; u->levels = 0; u->lost = 0; return -1; }
    uint32_t length = u->used - u->open_at - 2U;
    if (!length) { u->used = u->open_at; return 0; }
    put16(u->data + u->open_at, length);
    put16(u->data + u->used, length);
    u->used += 2;
    u->done = u->used;
    ++u->levels;
    while (u->levels > SONG_UNDO_LEVELS) {
        uint32_t oldest = get16(u->data) + 4U;
        memmove(u->data, u->data + oldest, u->used - oldest);
        u->used -= oldest; u->done -= oldest; --u->levels;
    }
    return 0;
}

/* Remove the first event on the track equal to e, ignoring the take flag. */
SONG_SMALL static void remove_equal(struct song *s, unsigned track, const struct song_event *e)
{
    unsigned count, i = song_find(s, track, song_event_start(e));
    const struct song_event *t = song_track(s, track, &count);
    for (; i < count && song_event_start(&t[i]) == song_event_start(e); ++i)
        if (((t[i].at ^ e->at) & ~SONG_FRESH) == 0 && t[i].a == e->a && t[i].b == e->b && t[i].length == e->length) {
            (void)song_remove(s, track, i);
            return;
        }
}
SONG_SMALL static void apply(struct song *s, uint8_t *record, unsigned forward)
{
    unsigned op = record[0], track = record[1];
    struct song_event e;
    if (op == SONG_UNDO_ADD || op == SONG_UNDO_DEL) {
        memcpy(&e, record + 2, sizeof e);
        if ((op == SONG_UNDO_ADD) == forward) (void)song_add_event(s, track, &e);
        else remove_equal(s, track, &e);
    } else if (op == SONG_UNDO_SHIFT) {
        uint32_t shift[2];
        memcpy(shift, record + 2, sizeof shift);
        int32_t delta = (int32_t)shift[1];
        if (forward) (void)song_shift(s, shift[0], delta);
        else (void)song_shift(s, shift[0] + (uint32_t)delta, -delta);
    } else if (op == SONG_UNDO_MAPS) { /* swap, so the record holds the other side */
        struct song_maps now, then;
        song_maps_read(s, &now);
        memcpy(&then, record + 2, sizeof then);
        maps_write(s, &then, track); /* the track byte holds the mask */
        memcpy(record + 2, &now, sizeof now);
    }
}
SONG_SMALL int song_undo(struct song_undo *u, struct song *s)
{
    if (u->depth || u->done < 4U) return -1;
    uint32_t length = get16(u->data + u->done - 2U), start = u->done - 2U - length, end = u->done - 2U;
    u->paused = 1;
    while (end > start) {
        uint32_t size = get16(u->data + end - 2U);
        end -= size;
        apply(s, u->data + end, 0);
    }
    u->paused = 0;
    u->done = start - 2U;
    if (u->levels) --u->levels;
    return 0;
}
SONG_SMALL int song_redo(struct song_undo *u, struct song *s)
{
    if (u->depth || u->done + 4U > u->used) return -1;
    uint32_t length = get16(u->data + u->done), at = u->done + 2U, end = at + length;
    u->paused = 1;
    while (at < end) {
        uint32_t size = get16(u->data + at + 2U + (u->data[at] == SONG_UNDO_MAPS ? sizeof(struct song_maps)
                                                    : u->data[at] == SONG_UNDO_SHIFT ? 8U : 8U));
        apply(s, u->data + at, 1);
        at += size;
    }
    u->paused = 0;
    u->done = end + 2U;
    ++u->levels;
    return 0;
}
SONG_SMALL unsigned song_undo_count(const struct song_undo *u, unsigned redo)
{
    if (!redo) return u->levels;
    unsigned n = 0;
    for (uint32_t at = u->done; at + 4U <= u->used; at += get16(u->data + at) + 4U) ++n;
    return n;
}
#endif
