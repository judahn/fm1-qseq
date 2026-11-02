/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* song-mode: notes moved while playing (0.6.7), included by player.c.
 * Step entry's MOVE and NUDGE edit as it plays, as its other knobs do, and
 * the player finds its place again after any edit. A note moved on past
 * the playhead has sounded this pass already, though, and would sound
 * again where it lands, so it waits there, quiet, for the next pass. Moved
 * again it goes on waiting where it went, or stops waiting once it is back
 * behind the playhead, where it can't sound this pass anyway. A note still
 * sounding ends when it was due to, from the held list, as after any edit,
 * so nothing hangs. */

SONG_SMALL void song_player_moved(struct song_player *p, unsigned t, unsigned pitch, uint32_t from, uint32_t to)
{
    uint32_t reached = t == song_editing(p->song) ? view(p, p->next) : p->next; /* in the track's own clocks */
    if (!p->playing) return;
    for (unsigned i = 0; i < p->moved_count; ++i) {
        struct song_moved *m = &p->moved[i];
        if (m->track != t || m->pitch != pitch || m->at != from) continue;
        if (to >= reached) m->at = to;
        else *m = p->moved[--p->moved_count];
        return;
    }
    if (from < reached && to >= reached && p->moved_count < SONG_MOVED)
        p->moved[p->moved_count++] = (struct song_moved){to, (uint8_t)t, (uint8_t)pitch};
}
/* A waiting note comes up: quiet, and it waits no more. */
SONG_SMALL static int moved_out(struct song_player *p, unsigned t, const struct song_event *e)
{
    for (unsigned i = 0; i < p->moved_count; ++i)
        if (p->moved[i].track == t && p->moved[i].pitch == e->a && p->moved[i].at == song_event_start(e)) {
            p->moved[i] = p->moved[--p->moved_count];
            return 1;
        }
    return 0;
}
/* A track's pass is over: its notes wait no more. */
SONG_SMALL static void moved_pass(struct song_player *p, unsigned t)
{
    for (unsigned i = p->moved_count; i-- > 0;)
        if (p->moved[i].track == t) p->moved[i] = p->moved[--p->moved_count];
}
