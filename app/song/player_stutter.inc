/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* song-mode: emitting with glitch, included by player.c. The per-track
 * emitter of the editing build, and the stutter (design 8e): from a slice
 * grid line on, every track repeats the slice that just played instead of
 * the song, each repeat drifting the pitch. Song time runs on underneath,
 * so when the stutter ends playing rejoins exactly where the song is. A
 * stutter on the FX lane plays the same way for its length. */

SONG_SMALL static void emit_tracks(struct song_player *p, uint32_t end)
{
    for (unsigned t = 0; t < SONG_TRACKS; ++t) emit_track(p, t, end); /* player_clips.inc */
}

/* A track the stutter repeats: on the song, outside a block. Clips, and
 * blocks, play on through a stutter. */
static int stutters(const struct song_player *p, unsigned t) { return on_song(p, t) && p->blk[t] == SONG_FROM_NONE; }
/* Everything sounding on the song stops, and swung or repeating hits still
 * due go. */
SONG_SMALL static void cut(struct song_player *p)
{
    for (unsigned t = 0; t < SONG_TRACKS; ++t)
        if (stutters(p, t)) song_player_cut(p, t);
}
/* The slice's notes on one track, repeat after repeat, up to `stop`. */
SONG_SMALL static void stutter_track(struct song_player *p, unsigned t, uint32_t stop)
{
    const struct song *s = p->song;
    uint32_t len = p->stut_len, from = p->stut_anchor - len;
    unsigned count, first = song_find(s, t, from), last = song_find(s, t, p->stut_anchor), notes = 0;
    const struct song_event *e = song_track(s, t, &count);
    for (unsigned i = first; i < last; ++i) notes += song_is_note(&e[i]);
    for (;;) {
        uint32_t on = NEVER, off = NEVER;
        while (notes && (first + p->stut_i[t] >= last || !song_is_note(&e[first + p->stut_i[t]]))) {
            if (first + p->stut_i[t] < last) ++p->stut_i[t];
            else { p->stut_i[t] = 0; ++p->stut_k[t]; }
        }
        const struct song_event *n = notes ? &e[first + p->stut_i[t]] : NULL;
        if (n) on = p->stut_at + p->stut_k[t] * len + (song_event_start(n) - from);
        unsigned held = 0;
        for (unsigned i = 0; i < p->held_count[t]; ++i)
            if (p->held[t][i].end < off) { off = p->held[t][i].end; held = i; }
        if (off <= on && off < stop) release(p, t, held);
        else if (on < stop) {
            int pitch = n->a + (p->pad_count[t] ? 0 : p->stut_drift * (int)p->stut_k[t]);
            uint32_t length = from + len - song_event_start(n); /* each repeat ends at its slice's end */
            const struct song_event r = {on, (uint8_t)(pitch < 0 ? 0 : pitch > 127 ? 127 : pitch), n->b,
                                         (uint16_t)(n->length < length ? n->length : length)};
            note_on(p, t, &r, on);
            ++p->stut_i[t];
        } else break;
    }
}
/* A stutter on the FX lane starts when playing reaches it. */
SONG_SMALL static void fx_lane(struct song_player *p, uint32_t end)
{
    unsigned count, part = song_owner(p->song, 0); /* track 1's own part, a clip open there or not */
    const struct song_event *e = song_track(p->song, part, &count);
    for (unsigned i = song_find(p->song, part, p->next); p->stut_at == NEVER && i < count && song_event_start(&e[i]) < end; ++i) {
        uint32_t at = song_event_start(&e[i]), len = song_slice_clocks(e[i].a);
        if (!song_is_stutter(&e[i]) || at < len) continue;
        p->stut_at = p->stut_anchor = at;
        p->stut_to = at + e[i].length;
        p->stut_len = len;
        p->stut_drift = (int8_t)(e[i].b - 64);
        p->stut_running = 0;
    }
}
SONG_SMALL static void emit(struct song_player *p, uint32_t end)
{
    clicks(p, end);
    fx_lane(p, end);
    if (p->stut_at != NEVER && (p->stut_running || p->stut_at < end)) {
        if (!p->stut_running) {
            emit_tracks(p, p->stut_at);
            cut(p);
            memset(p->stut_k, 0, sizeof p->stut_k);
            memset(p->stut_i, 0, sizeof p->stut_i);
            p->stut_running = 1;
        }
        uint32_t stop = end < p->stut_to ? end : p->stut_to;
        for (unsigned t = 0; t < SONG_TRACKS; ++t)
            if (stutters(p, t)) stutter_track(p, t, stop);
            else emit_track(p, t, stop);
        if (stop == p->stut_to) { /* rejoin the song where it is now */
            cut(p);
            for (unsigned t = 0; t < SONG_TRACKS; ++t)
                if (stutters(p, t)) { song_place(p, t, stop); chase_track(p, t, stop); }
            p->stut_at = p->stut_to = NEVER;
            p->stut_running = 0;
            p->next = stop;
            emit_tracks(p, end);
        }
    } else emit_tracks(p, end);
    if (end > p->next) p->next = end;
}

SONG_SMALL void song_player_stutter(struct song_player *p, uint32_t slice, int drift)
{
    if (!p->playing || !slice) return;
    p->stut_drift = (int8_t)drift;
    if (p->stut_at != NEVER && p->stut_running) { /* a new slice of the same moment, from its next grid line */
        uint32_t from = p->next > p->stut_anchor ? p->next - p->stut_anchor : 0;
        p->stut_len = slice;
        p->stut_at = p->stut_anchor + (from + slice - 1U) / slice * slice;
        p->stut_to = NEVER;
        memset(p->stut_k, 0, sizeof p->stut_k);
        memset(p->stut_i, 0, sizeof p->stut_i);
        return;
    }
    uint32_t start;
    song_bar_at(p->song, p->next, &start, NULL);
    uint32_t at = start + (p->next - start + slice - 1U) / slice * slice;
    p->stut_at = p->stut_anchor = at < slice ? slice : at;
    p->stut_to = NEVER;
    p->stut_len = slice;
    p->stut_running = 0;
}
SONG_SMALL void song_player_stutter_drift(struct song_player *p, int drift) { p->stut_drift = (int8_t)drift; }
SONG_SMALL uint32_t song_player_stutter_stop(struct song_player *p)
{
    if (p->stut_at == NEVER) return NEVER;
    if (!p->stut_running) { p->stut_at = NEVER; return NEVER; } /* let go before it began */
    /* Up to the clock not yet sent: the next block cuts it, as the lane does. */
    if (p->stut_to == NEVER || p->stut_to > p->next + 1U) p->stut_to = p->next + 1U;
    return p->stut_anchor;
}
