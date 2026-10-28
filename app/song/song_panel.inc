/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* song-mode: panel input, button lights and the control poll for the song
 * view. Included by song_view.inc, so it shares screen.c's panel state. */

/* Button lights: SEQ in the view, PLAY for the song, ENV for the loop. */
static uint32_t song_leds(uint32_t bits, unsigned visible)
{
    const struct song_player *p = song_port_player();
    if (p->playing) bits |= BUTTON(PLAY);
    if (visible != SONG) return bits;
    bits &= ~(BUTTON(HOME) | BUTTON(ENV) | BUTTON(FX));
    return bits | BUTTON(SEQ) | (p->song->loop_on ? BUTTON(ENV) : 0) |
           ((previous_buttons & BUTTON(FX)) ? BUTTON(FX) : 0);
}

static void song_start(uint32_t now)
{
    struct groove_view v = {0};
    groove_get_view(track, cursor, &v);
    if (v.playing) (void)post(0xfc, 0, 0, 1); /* the step sequencer stays stopped */
    writing = 0;
    if (song_port_start()) { notice = "SONG PLAYBACK: PREVIEW ONLY FOR NOW"; notice_until = now + 2000U; }
}

/* Panel input. Returns 1 when the song view consumed this poll. SEQ opens
 * the song view, and from there the stock step page. On the step page SEQ
 * keeps its stock long press (actions), and a tap comes back to the song. */
static unsigned song_seq_on_steps;
static int song_input(const struct fm1_controls *c, uint32_t *pressed, uint32_t key_down, uint32_t now)
{
    struct song *s = song_port_song();
    struct song_player *p = song_port_player();
    if ((*pressed & BUTTON(SEQ)) && !(c->buttons & BUTTON(HOME))) {
        if (page == STEPS) { song_seq_on_steps = 1; return 0; }
        if (page != SONG) { page = music_page = SONG; writing = 0; }
        else { page = music_page = STEPS; seq_page = 0; step_follow = 1; }
        cache->shown_page = ~0U;
        return 1;
    }
    if (song_seq_on_steps && !(c->buttons & BUTTON(SEQ))) {
        song_seq_on_steps = 0;
        if (page == STEPS && now - seq_ms < 600U) {
            page = music_page = SONG; writing = 0;
            cache->shown_page = ~0U;
            return 1;
        }
    }
    if (page != SONG) {
        if ((*pressed & BUTTON(PLAY)) && p->playing) { song_port_stop(); *pressed &= ~BUTTON(PLAY); }
        return 0;
    }
    music_page = SONG;
    if (glo_down) return 0; /* the stock mixer layer */
    unsigned shift = (c->buttons & BUTTON(FX)) != 0;
    if (*pressed & BUTTON(PLAY)) {
        if (shift) song_player_jump(p, s->loop_on ? s->loop_start : 0);
        else if (p->playing) song_port_stop();
        else song_start(now);
    }
    if ((*pressed & BUTTON(ENV)) && !shift) s->loop_on ^= 1U;
    *pressed &= ~(BUTTON(PLAY) | BUTTON(FX) | BUTTON(ENV) | BUTTON(REC) | BUTTON(ARP));
    if (*pressed & (BUTTON(HOME) | BUTTON(EDIT) | BUTTON(SCL) | BUTTON(LFO) | BUTTON(GLO) | BUTTON(SAVE)))
        return 0;
    octave = clamp(octave + !!(*pressed & BUTTON(OCT_UP)) - !!(*pressed & BUTTON(OCT_DOWN)), -2, 2);
    select_track(c->encoders[0]);
    int move = c->encoders[2];
    if (move && shift) {
        /* Marker to marker, from a pending jump or the playhead. */
        uint32_t at = p->playing ? (p->jumping ? p->jump : song_player_clock(p)) : p->playhead;
        int direction = move > 0 ? 1 : -1, found = -1;
        for (; move; move -= direction) {
            int next = song_marker_step(s, at, direction);
            if (next < 0) break;
            found = next;
            at = s->marker[next].clock;
        }
        if (found >= 0) song_player_jump(p, at);
    } else if (move)
        song_player_jump(p, song_snap_move(s, song_player_target(p), song_zoom, move));
    if (c->encoders[3]) {
        uint32_t at = song_snap_move(s, s->loop_start, song_zoom, c->encoders[3]);
        if (at < s->loop_end) s->loop_start = at;
    }
    if (c->encoders[4]) {
        uint32_t at = song_snap_move(s, s->loop_end, song_zoom, c->encoders[4]);
        if (at > s->loop_start) s->loop_end = at;
    }
    if (c->encoders[5]) {
        unsigned zoom = (unsigned)clamp((int)song_zoom + c->encoders[5], 0, SONG_ZOOMS - 1);
        if (zoom != song_zoom) { song_zoom = zoom; song_window = ~0U; }
    }
    if (shift) {
        /* SHIFT + the upper white keys, F4 to G5: markers 1 to 9. */
        for (unsigned i = 0; i < 9; ++i)
            if ((key_down & ~blocked_keys) & (1U << white_keys[7 + i])) {
                if (i < s->markers) song_player_jump(p, s->marker[i].clock);
                else { notice = "NO MARKER THERE"; notice_until = now + 1500U; }
            }
        blocked_keys |= key_down; /* SHIFT keys never play notes */
    }
    return 1;
}

static void song_poll(const struct groove_view *transport)
{
    song_port_poll();
    if (song_port_player()->playing && transport->playing) (void)post(0xfc, 0, 0, 1);
}
