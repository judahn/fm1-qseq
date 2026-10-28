/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* song-mode: clip blocks in the player (design 8f, blocks.h), included by
 * player_clips.inc. A track on the song inside a block plays the clip in
 * its column from the block's start, pass after pass, or nothing for a
 * stop, its own events silent, until the block ends: at its length or
 * where the track's next block starts. Entering a block ends what the
 * track was sounding and chases the clip's lanes, and leaving one does the
 * same with the track's own. */

/* Blocks. The next one at or after index i, and where one ends: at its
 * length, or where the next one starts. */
SONG_SMALL static unsigned next_block(const struct song_event *e, unsigned i, unsigned count)
{
    while (i < count && !song_is_block(&e[i])) ++i;
    return i;
}
SONG_SMALL static uint32_t block_end(const struct song_event *e, unsigned i, unsigned count)
{
    unsigned n = next_block(e, i + 1U, count);
    uint32_t end = song_event_start(&e[i]) + song_block_length(&e[i]);
    return n < count && song_event_start(&e[n]) < end ? song_event_start(&e[n]) : end;
}
/* Track t inside block i from song clock `at`: its clip from the pass
 * holding `at`, counted from the block's start, its lanes chased. */
SONG_SMALL static void enter(struct song_player *p, unsigned t, const struct song_event *e, unsigned i, unsigned count,
                             uint32_t at)
{
    p->blk_end[t] = block_end(e, i, count);
    p->blk[t] = e[i].a == SONG_BLOCK_STOP ? SONG_FROM_STOP : e[i].a & (SONG_GRID_ROWS - 1U);
    p->late_count[t] = 0;
    if (p->blk[t] >= SONG_GRID_ROWS) return;
    unsigned cell = t * SONG_GRID_ROWS + p->blk[t];
    p->base[t] = song_event_start(&e[i]);
    pass_at(p, t, cell, &p->base[t], at);
    song_chase(p->song, clip_part(p->song, cell), at - p->base[t], chased, p);
}
/* Track t on the song as of song clock `at`: inside a block or not. */
SONG_SMALL static void song_place(struct song_player *p, unsigned t, uint32_t at)
{
    unsigned count, cursor = song_find(p->song, t, at), inside = NEVER;
    const struct song_event *e = song_track(p->song, t, &count);
    for (unsigned i = next_block(e, 0, count); i < cursor; i = next_block(e, i + 1U, count))
        if (block_end(e, i, count) > at) inside = i;
    p->blk[t] = SONG_FROM_NONE;
    p->late_count[t] = 0;
    p->cursor[t] = (uint16_t)cursor;
    p->blk_next[t] = (uint16_t)next_block(e, cursor, count);
    if (inside != NEVER) enter(p, t, e, inside, count, at);
}
/* The song on track t up to song clock `end`, blocks taking over. */
SONG_SMALL static void emit_song(struct song_player *p, unsigned t, uint32_t end)
{
    const struct song *s = p->song;
    unsigned count;
    const struct song_event *e = song_track(s, t, &count);
    for (;;) {
        if (p->blk[t] != SONG_FROM_NONE) { /* its end is due once clocks from there are */
            uint32_t stop = p->blk_end[t] < end ? p->blk_end[t] : end;
            if (p->blk[t] < SONG_GRID_ROWS) emit_loop(p, t, t * SONG_GRID_ROWS + p->blk[t], &p->base[t], stop);
            if (end <= p->blk_end[t]) return;
            song_player_cut(p, t); /* the block ends: the track's own events again */
            song_place(p, t, stop);
            song_chase(s, t, stop, chased, p);
            continue;
        }
        unsigned b = p->blk_next[t];
        uint32_t at = b < count ? song_event_start(&e[b]) : NEVER;
        emit_run(p, t, e, count, at < end ? at : end, 0);
        if (at >= end) return;
        song_player_cut(p, t);
        enter(p, t, e, b, count, at);
        if (p->blk_end[t] <= at) { /* nothing left of it: the next one starts there too */
            p->blk[t] = SONG_FROM_NONE;
            p->blk_next[t] = (uint16_t)next_block(e, b + 1U, count);
        }
    }
}
