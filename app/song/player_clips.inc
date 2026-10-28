/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* song-mode: clips in the player (design 8f), included by player.c. Each
 * track plays the song, a clip of its column, or nothing. A clip loops at
 * its own length on the transport clock, which runs on through the song's
 * loops and jumps, and its events go through the song's emitter, timed
 * from the pass's start, so swing, humanize and glitch apply as on the song.
 * A launch waits for its transport clock and takes the track over exactly
 * there, ending what the track was sounding. */

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
static unsigned clip_cell(const struct song_player *p, unsigned t) { return t * SONG_GRID_ROWS + p->from[t]; }
static unsigned clip_part(const struct song_player *p, unsigned t) { return song_owner(p->song, SONG_TRACKS + clip_cell(p, t)); }
/* A clip, pass after pass from base, up to transport clock `to`. What it
 * keeps past its end stays quiet. */
SONG_SMALL static void emit_clip(struct song_player *p, unsigned t, uint32_t to)
{
    const struct song *s = p->song;
    unsigned part = clip_part(p, t), count;
    uint32_t length = s->clip_length[clip_cell(p, t)];
    const struct song_event *e = song_track(s, part, &count);
    count = song_find(s, part, length);
    while (length) {
        uint32_t stop = p->base[t] + length;
        emit_run(p, t, e, count, to < stop ? to : stop, p->base[t]);
        if (to <= stop) return;
        p->base[t] = stop;
        p->cursor[t] = 0;
        p->late_count[t] = 0;
    }
}
/* The cursor in a track's source as of transport clock `at`, and for a clip
 * the pass that holds it. A clip deleted while playing stops the track. */
SONG_SMALL static void refind(struct song_player *p, unsigned t, uint32_t at)
{
    const struct song *s = p->song;
    p->late_count[t] = 0;
    if (on_song(p, t)) { p->cursor[t] = (uint16_t)song_find(s, t, at - p->shift); return; }
    if (p->from[t] >= SONG_GRID_ROWS) return;
    uint32_t length = s->clip_length[clip_cell(p, t)];
    if (!length) { song_player_cut(p, t); p->from[t] = SONG_FROM_STOP; return; }
    if (at > p->base[t]) p->base[t] += (at - p->base[t]) / length * length;
    p->cursor[t] = (uint16_t)song_find(s, clip_part(p, t), at - p->base[t]);
}
/* The waiting launch takes track t over at its clock, or now if that has
 * passed (a launch made while stopped), with the source's lanes chased. */
SONG_SMALL static void take_over(struct song_player *p, unsigned t)
{
    uint32_t now = p->next + p->shift, at = p->launch_at[t] > now ? p->launch_at[t] : now;
    song_player_cut(p, t);
    p->from[t] = p->waiting[t];
    p->waiting[t] = SONG_FROM_NONE;
    p->base[t] = p->from[t] < SONG_GRID_ROWS ? at : 0;
    refind(p, t, at);
    if (on_song(p, t)) song_chase(p->song, t, at - p->shift, chased, p);
    else if (p->from[t] < SONG_GRID_ROWS) song_chase(p->song, clip_part(p, t), at - p->base[t], chased, p);
}
/* What track t plays up to transport clock `to`. */
SONG_SMALL static void emit_source(struct song_player *p, unsigned t, uint32_t to)
{
    unsigned count;
    const struct song_event *e = song_track(p->song, t, &count);
    if (on_song(p, t)) emit_run(p, t, e, count, to - p->shift, 0);
    else if (p->from[t] < SONG_GRID_ROWS) emit_clip(p, t, to);
}
/* Up to song clock `end`, with a launch due before then taking over at its
 * exact clock. The clip open in the editor plays as its track's song. */
SONG_SMALL static void emit_track(struct song_player *p, unsigned t, uint32_t end)
{
    uint32_t to = end + p->shift, now = p->next + p->shift;
    if (p->waiting[t] != SONG_FROM_NONE && p->launch_at[t] < to && t != song_editing(p->song)) {
        emit_source(p, t, p->launch_at[t] > now ? p->launch_at[t] : now);
        take_over(p, t);
    }
    emit_source(p, t, to);
}
/* Any track on a clip, or waiting for one. */
SONG_SMALL static unsigned clips_playing(const struct song_player *p)
{
    unsigned any = 0;
    for (unsigned t = 0; t < SONG_TRACKS; ++t)
        any |= (p->from[t] < SONG_GRID_ROWS && t != song_editing(p->song)) | (p->waiting[t] < SONG_GRID_ROWS);
    return any;
}
/* The launch quantize's next point at or after song clock `clock`: now, the
 * next beat, or the next bar line whose bar is a multiple of 1, 2 or 4. */
SONG_SMALL static uint32_t launch_point(const struct song *s, uint32_t clock)
{
    uint32_t start;
    if (s->launch_q == 1) return next_beat(s, clock);
    if (!s->launch_q) return clock;
    unsigned bar = song_bar_at(s, clock, &start, NULL), every = 1U << (s->launch_q - 2U);
    bar += start != clock;
    return song_bar_clock(s, (bar + every - 1U) / every * every);
}
SONG_SMALL void song_player_launch(struct song_player *p, unsigned track, unsigned to)
{
    if (track >= SONG_TRACKS) return;
    if (to == p->from[track] && to >= SONG_GRID_ROWS) { p->waiting[track] = SONG_FROM_NONE; return; } /* already there */
    p->waiting[track] = (uint8_t)to;
    p->launch_at[track] = p->playing ? launch_point(p->song, p->next) + p->shift : 0;
}
SONG_SMALL uint32_t song_player_transport(const struct song_player *p) { return song_player_clock(p) + p->shift; }
SONG_SMALL void song_player_seek(struct song_player *p, uint32_t clock)
{
    if (!p->playing) { p->playhead = clock; return; }
    p->jump = clock;
    p->jumping = 1;
    p->jump_at = p->next; /* the next clock, so the jump keeps exact time */
}
