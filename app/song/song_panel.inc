/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* song-mode: panel input, lights and the control poll for the song pages.
 * Included by song_view.inc, so it shares screen.c's panel state. */

/* Button lights: the page's own buttons, and PLAY while the song plays. */
static uint32_t song_leds(uint32_t bits, unsigned visible)
{
    const struct song_player *p = song_port_player();
    if (p->playing) bits |= BUTTON(PLAY);
    if (visible < SONG) return bits;
    bits &= ~(BUTTON(HOME) | BUTTON(ENV) | BUTTON(FX) | BUTTON(GLO) | BUTTON(REC) | BUTTON(LFO));
    if (previous_buttons & BUTTON(FX)) bits |= BUTTON(FX);
    if (visible == SONG) return bits | BUTTON(SEQ) | (p->song->loop_on ? BUTTON(ENV) : 0);
    if (visible == SONG_STEP) return bits | BUTTON(REC) | (previous_buttons & BUTTON(LFO));
    return bits | BUTTON(GLO);
}
/* Key lights: the scale on pitched tracks, or the swinging tracks on the
 * settings page. Held keys stay lit. */
static uint32_t song_key_leds(uint32_t bits, unsigned visible)
{
    const struct song *s = song_port_song();
    if (visible == SONG_SETUP) {
        bits = 0;
        for (unsigned t = 0; t < SONG_TRACKS; ++t)
            if (s->swing_tracks >> t & 1U) bits |= 1U << white_keys[t];
        return bits;
    }
    if (visible < SONG || !s->key_scale || groove_pad_count(track)) return bits;
    unsigned mask = song_scale_mask(s);
    for (unsigned key = 0; key < (visible == SONG_STEP ? 24U : 27U); ++key) {
        unsigned note = groove_key_note(track, key, octave);
        if (note < 128 && (mask >> (note % 12U) & 1U)) bits |= 1U << key;
    }
    return bits;
}
/* Out-of-scale keys snap up to the next scale note on pitched tracks. */
static unsigned song_key_note(unsigned note)
{
    if (note > 127 || page < SONG || groove_pad_count(track)) return note;
    return song_scale_snap(song_port_song(), note);
}

static void song_start(uint32_t now)
{
    struct groove_view v = {0};
    groove_get_view(track, cursor, &v);
    if (v.playing) (void)post(0xfc, 0, 0, 1); /* the step sequencer stays stopped */
    writing = 0;
    if (song_port_start()) { notice = "SONG PLAYBACK: PREVIEW ONLY FOR NOW"; notice_until = now + 2000U; }
}

/* The song view: transport, snap and zoom, loop and markers. */
static int song_view_input(const struct fm1_controls *c, uint32_t *pressed, uint32_t key_down, uint32_t now)
{
    struct song *s = song_port_song();
    struct song_player *p = song_port_player();
    unsigned shift = (c->buttons & BUTTON(FX)) != 0;
    if (*pressed & BUTTON(PLAY)) {
        if (shift) song_player_jump(p, s->loop_on ? s->loop_start : 0);
        else if (p->playing) song_port_stop();
        else song_start(now);
    }
    if (*pressed & BUTTON(REC)) {
        *pressed &= ~BUTTON(REC);
        if (!p->playing) { song_step_enter(); return 1; }
        notice = "STOP FIRST - LIVE REC COMES LATER";
        notice_until = now + 2000U;
    }
    if ((*pressed & BUTTON(ENV)) && !shift) s->loop_on ^= 1U;
    *pressed &= ~(BUTTON(PLAY) | BUTTON(FX) | BUTTON(ENV) | BUTTON(ARP));
    if (*pressed & (BUTTON(HOME) | BUTTON(EDIT) | BUTTON(SCL) | BUTTON(LFO) | BUTTON(SAVE)))
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

/* Panel input. Returns 1 when a song page consumed this poll. SEQ opens the
 * song view, and from there the stock step page. On the step page SEQ keeps
 * its stock long press (actions), and a tap comes back to the song. GLO on a
 * song page: a tap opens or closes the settings, a hold is still the stock
 * mixer layer, and SHIFT+GLO opens the stock device page. */
static unsigned song_seq_on_steps;
static int song_input(const struct fm1_controls *c, uint32_t *pressed, uint32_t key_down, uint32_t now)
{
    if ((*pressed & BUTTON(SEQ)) && !(c->buttons & BUTTON(HOME))) {
        if (page == STEPS) { song_seq_on_steps = 1; return 0; }
        if (page == SONG_STEP) song_step_leave(0, now);
        else if (page != SONG) { page = music_page = SONG; writing = 0; }
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
    if ((*pressed & BUTTON(GLO)) && page >= SONG) {
        if (!(c->buttons & BUTTON(FX))) { song_glo_from = page; return 0; }
        step_preview_stop(song_port_step());
        page = BLE; ble_view = ble_forget = ble_action = 0; ble_result = 0;
        cache->shown_page = ~0U;
        return 1;
    }
    if (song_glo_from && !(c->buttons & BUTTON(GLO))) {
        unsigned from = song_glo_from;
        song_glo_from = 0;
        if (page == MIX) { /* a tap: the stock code just opened its mixer page */
            if (from == SONG_SETUP) page = song_setup_back;
            else { song_setup_back = from; page = SONG_SETUP; }
            music_page = page;
            cache->shown_page = ~0U;
            return 1;
        }
    }
    if (page < SONG) {
        if ((*pressed & BUTTON(PLAY)) && song_port_player()->playing) { song_port_stop(); *pressed &= ~BUTTON(PLAY); }
        return 0;
    }
    music_page = page;
    if (glo_down) return 0; /* the stock mixer layer */
    if (page == SONG_STEP) return song_step_input(c, pressed, key_down, now);
    if (page == SONG_SETUP) return song_setup_input(c, pressed, key_down, now);
    return song_view_input(c, pressed, key_down, now);
}

/* Each control poll: the device stub, preview note-offs after 300 ms, and
 * the stock step sequencer kept stopped while the song plays. */
static uint16_t song_preview_serial;
static uint32_t song_preview_since;
static void song_poll(const struct groove_view *transport)
{
    struct song_step *st = song_port_step();
    song_port_poll();
    if (st->preview_count) {
        uint32_t now = fm1_millis();
        if (st->serial != song_preview_serial) { song_preview_serial = st->serial; song_preview_since = now; }
        else if (now - song_preview_since >= 300U) step_preview_stop(st);
    }
    if (song_port_player()->playing && transport->playing) (void)post(0xfc, 0, 0, 1);
}
