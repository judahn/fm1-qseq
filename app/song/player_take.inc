/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* song-mode: the player's side of recording, included by player.c. The
 * metronome, the count-in before the first beat, and the clock at an
 * earlier frame for keys played live. */
#if SONG_UI
/* Metronome: a rim click on every beat of the song's grid, louder on the
 * downbeat, 60 clocks long. */
SONG_SMALL static uint32_t next_beat(const struct song *s, uint32_t clock)
{
    uint32_t start;
    unsigned bar = song_bar_at(s, clock, &start, NULL);
    uint32_t beat = SONG_WHOLE / song_meter_at(s, bar)->unit;
    return start + (clock - start + beat - 1U) / beat * beat;
}
SONG_SMALL static void click_off(struct song_player *p)
{
    if (p->click_off != NEVER) send(p, p->click_track, CLICK, 0);
    p->click_off = NEVER;
}
SONG_SMALL static void clicks(struct song_player *p, uint32_t end)
{
    for (;;) {
        if (p->click_off < end && p->click_off <= p->click_next) { click_off(p); continue; }
        if (!p->click || p->click_next >= end) break;
        uint32_t start;
        song_bar_at(p->song, p->click_next, &start, NULL);
        click_off(p);
        send(p, p->click_track, CLICK, start == p->click_next ? 127U : 80U);
        p->click_off = p->click_next + 60U;
        p->click_next = next_beat(p->song, p->click_next + 1U);
    }
}
/* Count-in clicks come before the song's own first frame. */
SONG_SMALL static void pre_roll(struct song_player *p, uint64_t frame)
{
    while (p->playing && p->pre_left) {
        uint64_t at = p->pre_origin + p->pre_done * p->pre_units / p->pre_rate;
        if (at >= frame) break;
        click_off(p);
        send(p, p->click_track, CLICK, p->pre_done % p->pre_beats ? 80U : 127U);
        p->click_off = 0; /* ends at the next click, or as the song starts */
        ++p->pre_done;
        --p->pre_left;
    }
}
SONG_SMALL void song_player_count_in(struct song_player *p, uint64_t frame, unsigned bars)
{
    song_player_start(p, frame);
    if (!bars || !p->playing) return;
    const struct song *s = p->song;
    const struct song_meter *m = song_meter_at(s, song_bar_at(s, p->next, NULL, NULL));
    p->pre_beats = m->beats;
    p->pre_left = (uint16_t)(bars * m->beats);
    p->pre_done = 0;
    p->pre_rate = song_rate(s->tempo[p->segment].bpm100);
    p->pre_units = (uint64_t)(SONG_WHOLE / m->unit) * SONG_UNITS_PER_CLOCK;
    p->pre_origin = frame;
    p->frame = frame + p->pre_left * p->pre_units / p->pre_rate;
}
SONG_SMALL uint32_t song_player_clock_at(const struct song_player *p, uint64_t frame)
{
    if (!p->playing || frame >= p->frame) return song_player_clock(p);
    const struct song *s = p->song;
    unsigned k = p->segment;
    uint64_t back = (p->frame - frame) * song_rate(s->tempo[k].bpm100), inside = p->pos - s->tempo_units[k];
    return s->tempo[k].clock + (uint32_t)(back < inside ? (inside - back) / SONG_UNITS_PER_CLOCK : 0);
}
#endif
