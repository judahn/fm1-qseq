/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* song-mode: editing in the automation view, included by song_auto.inc.
 * Setting and deleting changes at the playhead, clearing the shown lanes in
 * the selection, and stepping from change to change. */

/* SHIFT+ALGO on the conductor row or a program lane, while stopped. Tempo
 * goes in whole BPM (0.5.0: no hundredths here). */
static void song_auto_set(struct song *s, int delta, uint32_t now)
{
    struct song_player *p = song_port_player();
    uint32_t at = p->playhead, start;
    unsigned bar = song_bar_at(s, at, &start, NULL);
    notice_until = now + 1500U;
    if (p->playing) { notice = "STOP TO EDIT CHANGES"; return; }
    if (song_auto_conductor && !song_auto_mode) {
        int bpm = s->tempo[song_tempo_at(s, at)].bpm100 + delta * 100;
        notice = song_port_set_tempo(at, (unsigned)clamp(bpm, SONG_TEMPO_MIN, SONG_TEMPO_MAX)) ? "TEMPO CHANGES FULL" : NULL;
    } else if (song_auto_conductor) { /* QY70 range, listed x/4, x/8, x/16 */
        const struct song_meter *m = song_meter_at(s, bar);
        int i = clamp((m->unit == 4 ? 0 : m->unit == 8 ? 8 : 24) + m->beats - 1 + delta, 0, 39);
        unsigned beats = i < 8 ? (unsigned)i + 1U : i < 24 ? (unsigned)i - 7U : (unsigned)i - 23U;
        notice = song_set_meter(s, bar, beats, i < 8 ? 4U : i < 24 ? 8U : 16U) ? "TIME SIG CHANGES FULL" : NULL;
    } else if (song_auto_lane(track) == SONG_LANE_PROGRAM) {
        unsigned programs = song_programs(song_port_engine(track));
        int now_at = song_lane_at(s, track, SONG_LANE_PROGRAM, at);
        if (!programs) { notice = "NO PROGRAMS ON THIS ENGINE"; return; }
        if (now_at < 0) { struct groove_view v = {0}; groove_get_view(track, 0, &v); now_at = v.program; }
        unsigned program = (unsigned)clamp(now_at + delta, 0, (int)programs - 1);
        notice = song_lane_set(s, track, SONG_LANE_PROGRAM, at, program, 0) ? "SONG FULL" : NULL;
        if (!notice && p->out.control) p->out.control(p->out.context, track, 0xc0, program, 0);
    } else notice = "RECORD THIS LANE WITH K1-K4";
}
static void song_auto_delete(struct song *s, unsigned shift, uint32_t now)
{
    uint32_t at = song_port_player()->playhead;
    notice_until = now + 1500U;
    if (shift) {
        unsigned picked = song_selected(), cleared = 0;
        for (unsigned t = 0; t < SONG_TRACKS; ++t)
            if (picked >> t & 1U) cleared += song_lane_clear(s, 1U << t, song_auto_lane(t), s->loop_start, s->loop_end);
        notice = cleared ? "AUTOMATION CLEARED" : "NO AUTOMATION THERE";
    } else if (song_auto_conductor) {
        struct song_place place;
        song_player_hold(song_port_player(), &place);
        notice = (song_auto_mode == 1 ? song_remove_meter(s, song_bar_at(s, at, NULL, NULL)) : song_remove_tempo(s, at))
                     ? "NO CHANGE HERE" : "CHANGE DELETED";
        song_player_resume(song_port_player(), &place);
    }
    else {
        int i = song_lane_find(s, track, song_auto_lane(track), at);
        notice = i < 0 || song_remove(s, track, (unsigned)i) ? "NO CHANGE HERE" : "CHANGE DELETED";
    }
}
/* SHIFT+K1: the next or previous change on the row. */
static void song_auto_step(const struct song *s, int direction)
{
    uint32_t at = song_port_player()->playhead;
    int64_t to = -1;
    if (!song_auto_conductor) to = song_lane_next(s, track, song_auto_lane(track), at, direction);
    else if (song_auto_mode)
        for (unsigned m = 0; m < s->meters; ++m) {
            uint32_t c = song_bar_clock(s, s->meter[m].bar);
            if (direction > 0 ? c > at && to < 0 : c < at) to = c;
        }
    else
        for (unsigned k = 0; k < s->tempos; ++k)
            if (direction > 0 ? s->tempo[k].clock > at && to < 0 : s->tempo[k].clock < at) to = s->tempo[k].clock;
    if (to >= 0) song_port_jump((uint32_t)to);
}
