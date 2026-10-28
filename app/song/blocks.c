/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Clip blocks in the song: placing them, the grid's clip pasted as one,
 * joining, and the Expand job. See blocks.h. The player plays them
 * (player_clips.inc). */
#include "blocks.h"
#include "lanes.h"
#include "player.h"
#include <string.h>
#if SONG_UI

/* The cell a block on track t points at (a stop's is never read). */
static unsigned block_cell(unsigned t, unsigned a) { return t * SONG_GRID_ROWS + (a & (SONG_GRID_ROWS - 1U)); }
SONG_SMALL int song_block_put(struct song *s, unsigned t, uint32_t from, uint32_t to, unsigned row)
{
    if (t >= SONG_TRACKS || to <= from || to > SONG_MAX_CLOCK || s->events >= SONG_EVENTS) return -1;
    const struct song_event *e = s->event + s->first[t];
    for (unsigned i = song_find(s, t, to); i-- > 0;) {
        if (!song_is_block(&e[i])) continue;
        uint32_t at = song_event_start(&e[i]);
        if (at >= from) (void)song_remove(s, t, i);
        else if (at + song_block_length(&e[i]) > from) {
            struct song_event c = e[i];
            song_block_resize(&c, from - at);
            (void)song_set_event(s, t, i, &c);
        }
    }
    struct song_event b = {from | (uint32_t)SONG_FX << 24, (uint8_t)(row < SONG_GRID_ROWS ? SONG_BLOCK | row : SONG_BLOCK_STOP), 0, 0};
    song_block_resize(&b, to - from);
    return song_add_event(s, t, &b);
}
SONG_SMALL int song_block_paste(struct song *s, const struct song_clip *board, uint32_t at)
{
    unsigned cell = board->cell - 1U, count, part;
    if (!board->cell || cell >= SONG_CELLS) return -1;
    part = song_owner(s, SONG_TRACKS + cell);
    const struct song_event *e = song_track(s, part, &count);
    unsigned n = song_find(s, part, board->length), same = s->clip_length[cell] == board->length && n == board->count;
    for (unsigned i = 0; same && i < n; ++i)
        same = !((e[i].at ^ board->event[i].at) & ~SONG_FRESH) && !memcmp(&e[i].a, &board->event[i].a, 4);
    if (!same) return -2;
    return song_block_put(s, cell / SONG_GRID_ROWS, at, at + board->length, cell % SONG_GRID_ROWS);
}
SONG_SMALL void song_blocks_join(struct song *s, unsigned tracks)
{
    for (unsigned t = 0; t < SONG_TRACKS; ++t) {
        if (!(tracks >> t & 1U)) continue;
        const struct song_event *e = s->event + s->first[t];
        for (unsigned i = s->count[t]; i-- > 0;) {
            if (!song_is_block(&e[i])) continue;
            uint32_t end = song_event_start(&e[i]) + song_block_length(&e[i]);
            unsigned j = i + 1U, pass = e[i].a == SONG_BLOCK_STOP ? 1U : s->clip_length[block_cell(t, e[i].a)];
            while (j < s->count[t] && !song_is_block(&e[j])) ++j; /* the next block, past the notes under this one */
            if (j >= s->count[t] || song_event_start(&e[j]) != end || e[j].a != e[i].a || !pass ||
                song_block_length(&e[i]) % pass) continue;
            struct song_event c = e[i];
            song_block_resize(&c, song_block_length(&e[i]) + song_block_length(&e[j]));
            (void)song_remove(s, t, j);
            (void)song_set_event(s, t, i, &c);
        }
    }
}

/* Expand. Controller values chased at a block's edge, put there after.
 * `own`: leave out lanes the track changes right at the edge itself. */
struct edge { struct song *s; uint32_t at; unsigned track, own, count; uint8_t lane[32], value[32]; };
SONG_SMALL static void keep(void *context, unsigned track, unsigned lane, unsigned value)
{
    struct edge *g = context;
    (void)track;
    if (lane >= SONG_LANE_PROGRAM || g->count >= 32 || (g->own && song_lane_find(g->s, g->track, lane, g->at) >= 0)) return;
    g->lane[g->count] = (uint8_t)lane;
    g->value[g->count++] = (uint8_t)value;
}
SONG_SMALL static void put_edge(struct song *s, const struct edge *g)
{
    for (unsigned k = 0; k < g->count; ++k) {
        const struct song_event e = song_lane_event(g->at, g->lane[k], g->value[k]);
        (void)song_add_event(s, g->track, &e);
    }
}
/* Block i on track t, counted (apply 0) or done: the events it adds less
 * those it takes out. */
SONG_SMALL static int expand(struct song *s, unsigned t, unsigned i, unsigned apply)
{
    const struct song_event *e = s->event + s->first[t];
    uint32_t from = song_event_start(&e[i]), to = from + song_block_length(&e[i]);
    unsigned next = i + 1U, cell = block_cell(t, e[i].a), part = song_owner(s, SONG_TRACKS + cell);
    uint32_t length = e[i].a == SONG_BLOCK_STOP ? 0 : s->clip_length[cell];
    while (next < s->count[t] && !song_is_block(&e[next])) ++next;
    if (next < s->count[t] && song_event_start(&e[next]) < to) to = song_event_start(&e[next]);
    unsigned count = length ? song_find(s, part, length) : 0, first = song_find(s, t, from);
    static struct edge in, out;
    in = (struct edge){s, from, t, 0, 0, {0}, {0}};
    out = (struct edge){s, to, t, 1, 0, {0}, {0}};
    if (length) song_chase(s, part, 0, keep, &in);
    song_chase(s, t, to, keep, &out);
    int n = (int)(in.count + out.count) - 1; /* the block itself goes */
    for (unsigned j = song_find(s, t, to); j-- > first;) {
        if (song_event_kind(&e[j]) == SONG_FX) continue; /* the FX lane plays on, and the block goes last */
        --n;
        if (apply) (void)song_remove(s, t, j);
    }
    for (unsigned j = 0; apply && j < first; ++j) /* notes sounding into it end where it starts */
        if (song_is_note(&e[j]) && song_event_start(&e[j]) + e[j].length > from) {
            struct song_event cut = e[j];
            cut.length = (uint16_t)(from - song_event_start(&e[j]));
            (void)song_set_event(s, t, j, &cut);
        }
    for (uint32_t pass = from; length && pass < to; pass += length)
        for (unsigned j = 0; j < count; ++j) {
            struct song_event x = s->event[s->first[part] + j]; /* the part moves as the track grows */
            uint32_t at = pass + song_event_start(&x);
            if (at >= to) break;
            ++n;
            if (!apply) continue;
            x.at = (x.at & ~(SONG_FRESH | 0xffffffU)) | at;
            if (song_is_note(&x) && at + x.length > to) x.length = (uint16_t)(to - at);
            (void)song_add_event(s, t, &x);
        }
    if (!apply) return n;
    put_edge(s, &in);
    put_edge(s, &out);
    for (unsigned j = song_find(s, t, from); j < s->count[t]; ++j)
        if (song_is_block(&e[j])) { (void)song_remove(s, t, j); break; }
    return n;
}
/* Takes. */
SONG_SMALL static void close_block(struct song_take *k, struct song *s, unsigned t, uint32_t clock)
{
    if (k->row[t] != SONG_FROM_NONE && clock > k->from[t])
        (void)song_block_put(s, t, k->from[t], clock, k->row[t] == SONG_FROM_STOP ? SONG_BLOCK_STOP : k->row[t]);
    k->row[t] = SONG_FROM_NONE;
}
SONG_SMALL void song_take_begin(struct song_take *k, struct song_player *p)
{
    memset(k->row, SONG_FROM_NONE, sizeof k->row);
    p->log_in = p->log_out = 0;
    p->perf = 1;
}
SONG_SMALL void song_take_drain(struct song_take *k, struct song_player *p)
{
    for (; p->log_out != p->log_in; ++p->log_out) {
        const struct song_launch *l = &p->log[p->log_out % 16U];
        close_block(k, p->song, l->track, l->clock);
        if (l->to != SONG_FROM_SONG) { k->from[l->track] = l->clock; k->row[l->track] = l->to; }
    }
}
SONG_SMALL void song_take_end(struct song_take *k, struct song_player *p)
{
    struct song *s = p->song;
    unsigned ran = p->perf == 2; /* armed and never started: nothing to close or grow */
    song_take_drain(k, p);
    p->perf = 0;
    for (unsigned t = 0; t < SONG_TRACKS; ++t) close_block(k, s, t, p->next);
    while (ran && s->bars < SONG_MAX_BARS && song_bar_clock(s, s->bars) < p->next) ++s->bars;
}
SONG_SMALL int song_job_expand(struct song *s, const struct song_range *r)
{
    int need = 0, added = 0;
    if (s->open) return -1;
    for (unsigned apply = 0; apply < 2; ++apply) {
        for (unsigned t = 0; t < SONG_TRACKS; ++t) {
            if (!(r->tracks >> t & 1U)) continue;
            for (uint32_t at = r->from; at < r->to;) {
                const struct song_event *e = s->event + s->first[t];
                unsigned i = song_find(s, t, at), last = song_find(s, t, r->to);
                while (i < last && !song_is_block(&e[i])) ++i;
                if (i >= last) break;
                at = song_event_start(&e[i]) + 1U;
                int n = expand(s, t, i, apply);
                if (apply) added += n; else need += n;
            }
        }
        if (!apply && (int)s->events + need > SONG_EVENTS) return -1;
    }
    return added;
}
#endif
