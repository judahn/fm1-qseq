/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* song-mode: clips in the player (design 8f), included by player.c. Each
 * track plays the song, a clip of its column, or nothing, and on the song
 * the clip blocks on its row too. A clip loops at its own length, pass
 * after pass from a base clock: a transport clock for a launched clip and
 * the clip open in the editor (the transport runs on through the song's
 * loops and jumps, so they keep their phase), a song clock for a block,
 * whose clip starts at the block's start. Its events go through the song's
 * emitter, so swing, humanize and glitch apply as on the song. A launch
 * waits for its transport clock and a block for its start, and each takes
 * the track over exactly there, ending what the track was sounding. */

/* One track's events from its cursor up to `end`, every time `shift` on from
 * the events' own clocks (0 on the song, the pass's start for a clip). */
SONG_SMALL static void emit_run(struct song_player *p, unsigned t, const struct song_event *e, unsigned count, uint32_t end,
                                uint32_t shift)
{
    for (;;) {
        uint32_t late = p->late_count[t] ? onset(p, t, &e[p->late_first[t]]) + shift : NEVER;
        while (late >= end && p->cursor[t] < count && song_event_start(&e[p->cursor[t]]) + shift < end &&
               onset(p, t, &e[p->cursor[t]]) + shift >= end &&
               (!p->late_count[t] || p->late_first[t] + p->late_count[t] == p->cursor[t])) {
            if (!p->late_count[t]) p->late_first[t] = p->cursor[t];
            ++p->late_count[t];
            ++p->cursor[t];
            late = onset(p, t, &e[p->late_first[t]]) + shift;
        }
        uint32_t on = p->cursor[t] < count ? onset(p, t, &e[p->cursor[t]]) + shift : NEVER, off = NEVER;
        unsigned first = 0, which = 0;
        uint32_t hit = next_hit(p, t, &which);
        for (unsigned i = 0; i < p->held_count[t]; ++i)
            if (p->held[t][i].end < off) { off = p->held[t][i].end; first = i; }
        if (off <= on && off <= late && off <= hit && off < end) release(p, t, first);
        else if (hit <= on && hit <= late && hit < end) hit_out(p, t, which);
        else if (late <= on && late < end) {
            PLAY(p, t, &e[p->late_first[t]], late);
            ++p->late_first[t];
            --p->late_count[t];
        } else if (on < end) {
            PLAY(p, t, &e[p->cursor[t]], on);
            ++p->cursor[t];
        } else break;
    }
}

SONG_SMALL void song_player_cut(struct song_player *p, unsigned t)
{
    while (p->held_count[t]) release(p, t, p->held_count[t] - 1U);
    p->hits[t] = 0;
    p->late_count[t] = 0;
}
static unsigned clip_part(const struct song *s, unsigned cell) { return song_owner(s, SONG_TRACKS + cell); }
/* A cell's clip, pass after pass from *base, up to `to` in base's clocks.
 * What it keeps past its end stays quiet. */
SONG_SMALL static void emit_loop(struct song_player *p, unsigned t, unsigned cell, uint32_t *base, uint32_t to)
{
    const struct song *s = p->song;
    unsigned part = clip_part(s, cell), count;
    uint32_t length = s->clip_length[cell];
    const struct song_event *e = song_track(s, part, &count);
    count = song_find(s, part, length);
    while (length) {
        uint32_t stop = *base + length;
        emit_run(p, t, e, count, to < stop ? to : stop, *base);
        if (to <= stop) return;
        *base = stop;
        p->cursor[t] = 0;
        p->late_count[t] = 0;
    }
}
/* A clip's pass holding `at` (base's clocks), and the cursor there. */
SONG_SMALL static void pass_at(struct song_player *p, unsigned t, unsigned cell, uint32_t *base, uint32_t at)
{
    uint32_t length = p->song->clip_length[cell];
    if (length && at > *base) *base += (at - *base) / length * length;
    p->cursor[t] = (uint16_t)song_find(p->song, clip_part(p->song, cell), at - *base);
}

#include "player_blocks.inc" /* a track on the song inside a clip block */
/* A track's lanes as of song clock `at`, from what it plays there. */
SONG_SMALL static void chase_track(struct song_player *p, unsigned t, uint32_t at)
{
    const struct song *s = p->song;
    if (t == song_editing(s)) song_chase(s, t, view(p, at), chased, p);
    else if (!on_song(p, t)) return; /* clips chase as they launch */
    else if (p->blk[t] < SONG_GRID_ROWS)
        song_chase(s, clip_part(s, t * SONG_GRID_ROWS + p->blk[t]), at - p->base[t], chased, p);
    else if (p->blk[t] == SONG_FROM_NONE) song_chase(s, t, at, chased, p);
}
/* The lanes as of `clock`, in the editor's time. Stopped, each track on the
 * song finds whether a block holds it there first. */
SONG_SMALL void song_player_chase(struct song_player *p, uint32_t clock)
{
    uint32_t at = p->song->open ? timeline(p) : clock;
    for (unsigned t = 0; t < SONG_TRACKS; ++t) {
        if (!p->playing && on_song(p, t)) song_place(p, t, at);
        if (t == song_editing(p->song)) song_chase(p->song, t, clock, chased, p);
        else chase_track(p, t, at);
    }
}
/* The cursor in a track's source as of transport clock `at`. A launched
 * clip deleted while playing stops the track. */
SONG_SMALL static void refind(struct song_player *p, unsigned t, uint32_t at)
{
    const struct song *s = p->song;
    p->late_count[t] = 0;
    if (t == song_editing(s)) { pass_at(p, t, s->open - SONG_TRACKS, &p->edit_base, at); return; }
    if (on_song(p, t)) { song_place(p, t, at - p->shift); return; }
    if (p->from[t] >= SONG_GRID_ROWS) return;
    unsigned cell = t * SONG_GRID_ROWS + p->from[t];
    if (!s->clip_length[cell]) { song_player_cut(p, t); p->from[t] = SONG_FROM_STOP; return; }
    pass_at(p, t, cell, &p->base[t], at);
}
/* The waiting launch takes track t over at its clock, or now if that has
 * passed (a launch made while stopped), with the source's lanes chased.
 * While a performance is recorded, it goes in the log. */
SONG_SMALL static void take_over(struct song_player *p, unsigned t)
{
    uint32_t now = p->next + p->shift, at = p->launch_at[t] > now ? p->launch_at[t] : now;
    song_player_cut(p, t);
    p->from[t] = p->waiting[t];
    p->waiting[t] = SONG_FROM_NONE;
    p->base[t] = p->from[t] < SONG_GRID_ROWS ? at : 0;
    refind(p, t, at);
    if (on_song(p, t)) chase_track(p, t, at - p->shift);
    else if (p->from[t] < SONG_GRID_ROWS)
        song_chase(p->song, clip_part(p->song, t * SONG_GRID_ROWS + p->from[t]), at - p->base[t], chased, p);
    if (p->perf) {
        p->perf = 2; /* the take starts with the first launch */
        p->log[p->log_in % 16U] = (struct song_launch){at - p->shift, (uint8_t)t, p->from[t]};
        ++p->log_in;
    }
}
/* What track t plays up to transport clock `to`. */
SONG_SMALL static void emit_source(struct song_player *p, unsigned t, uint32_t to)
{
    const struct song *s = p->song;
    if (t == song_editing(s)) emit_loop(p, t, s->open - SONG_TRACKS, &p->edit_base, to);
    else if (on_song(p, t)) emit_song(p, t, to - p->shift);
    else if (p->from[t] < SONG_GRID_ROWS) emit_loop(p, t, t * SONG_GRID_ROWS + p->from[t], &p->base[t], to);
}
/* Up to song clock `end`, with a launch due before then taking over at its
 * exact clock. The open clip's track takes a jump instead: its pass moves
 * at the bar line so the clip's clock is the target there. */
SONG_SMALL static void emit_track(struct song_player *p, unsigned t, uint32_t end)
{
    uint32_t to = end + p->shift, now = p->next + p->shift;
    if (t == song_editing(p->song)) {
        if (p->jumping && p->jump_at < end) {
            uint32_t at = (p->jump_at > p->next ? p->jump_at : p->next) + p->shift;
            emit_source(p, t, at);
            song_player_cut(p, t);
            p->edit_base = at - p->jump;
            p->jumping = 0;
            refind(p, t, at);
            song_chase(p->song, t, p->jump, chased, p);
        }
    } else if (p->waiting[t] != SONG_FROM_NONE && p->launch_at[t] < to) {
        emit_source(p, t, p->launch_at[t] > now ? p->launch_at[t] : now);
        take_over(p, t);
    }
    emit_source(p, t, to);
}
/* Any track on a clip, or waiting for one, or a clip open in the editor:
 * then the song goes round at its end instead of stopping. */
SONG_SMALL static unsigned clips_playing(const struct song_player *p)
{
    unsigned any = p->song->open != 0;
    for (unsigned t = 0; t < SONG_TRACKS; ++t) any |= (p->from[t] < SONG_GRID_ROWS) | (p->waiting[t] < SONG_GRID_ROWS);
    return any;
}
/* The launch quantize's next point at or after song clock `clock`: now, the
 * next beat, or the next bar line whose bar is a multiple of 1, 2 or 4.
 * The song loop's wrap and the song's end, where it goes round, are launch
 * points as well, so a loop that ends mid-bar keeps launches in time. */
SONG_SMALL static uint32_t launch_point(const struct song_player *p, uint32_t clock)
{
    const struct song *s = p->song;
    uint32_t start, end, point = timeline_end(p), loop_start;
    if (!s->launch_q) return clock;
    if (song_loop(p, &loop_start, &end) && clock <= end && end < point) point = end;
    if (s->launch_q == 1) end = next_beat(s, clock);
    else {
        unsigned bar = song_bar_at(s, clock, &start, NULL), every = 1U << (s->launch_q - 2U);
        bar += start != clock;
        end = song_bar_clock(s, (bar + every - 1U) / every * every);
    }
    return end < point ? end : point;
}
SONG_SMALL void song_player_launch(struct song_player *p, unsigned track, unsigned to)
{
    if (track >= SONG_TRACKS) return;
    if (to == p->from[track] && to >= SONG_GRID_ROWS) { p->waiting[track] = SONG_FROM_NONE; return; } /* already there */
    p->waiting[track] = (uint8_t)to;
    p->launch_at[track] = p->playing ? launch_point(p, p->next) + p->shift : 0;
}
SONG_SMALL void song_player_launch_rec(struct song_player *p, unsigned track, unsigned row)
{
    song_player_launch(p, track, row);
    if (track < SONG_TRACKS && p->playing && !p->song->launch_q) p->launch_at[track] = next_beat(p->song, p->next) + p->shift;
}
SONG_SMALL uint32_t song_player_transport(const struct song_player *p) { return timeline(p) + p->shift; }
SONG_SMALL void song_player_edit(struct song_player *p, unsigned t, unsigned opening)
{
    const struct song *s = p->song;
    p->jumping = 0;
    if (!p->playing) {
        if (opening) { p->song_at = p->playhead; p->playhead = 0; }
        else p->playhead = p->song_at;
        return;
    }
    uint32_t now = p->next + p->shift, start, launched = p->from[t] < SONG_GRID_ROWS ? s->clip_length[t * SONG_GRID_ROWS + p->from[t]] : 0;
    unsigned row = s->open ? (s->open - SONG_TRACKS) % SONG_GRID_ROWS : SONG_FROM_NONE;
    /* The clip the track plays as launched goes on in its pass, open or not. */
    unsigned same = opening ? p->from[t] == row : launched && (p->edit_base - p->base[t]) % launched == 0;
    if (!same) song_player_cut(p, t);
    if (opening && same) p->edit_base = p->base[t];
    else if (opening) {
        uint32_t length = s->clip_length[s->open - SONG_TRACKS];
        song_bar_at(s, p->next, &start, NULL);
        p->edit_base = now - (length ? (p->next - start) % length : 0);
    }
    refind(p, t, now);
    if (!same) chase_track(p, t, p->next);
}
