/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* song-mode: the song view's input, included by song_panel.inc: transport,
 * REC (song_rec.inc), snap and zoom (K4, and ALGO since 0.5.0), the loop,
 * markers, the selection and arranging keys, and the knobs recording
 * parameters during a take, when ALGO zooms the roll the take shows. Every
 * move of the playhead here sets the start point (song_port_jump). The
 * automation view runs its controls through here too (song_auto.inc). */
static void song_zoom_turn(int delta)
{
    unsigned zoom = (unsigned)clamp((int)song_zoom + delta, 0, SONG_ZOOMS - 1);
    if (zoom != song_zoom) { song_zoom = zoom; song_window = ~0U; }
}
static int song_view_input(const struct fm1_controls *c, uint32_t *pressed, uint32_t key_down, uint32_t now)
{
    struct song *s = song_port_song();
    struct song_player *p = song_port_player();
    unsigned shift = (c->buttons & BUTTON(FX)) != 0, clip = s->open != 0; /* a clip in the editor: no markers */
    unsigned picking = (c->buttons & BUTTON(LFO)) != 0 && !clip;
    if (*pressed & BUTTON(SAVE)) { /* SAVE, and SHIFT+SAVE: the song list, as SAVE comes up */
        *pressed &= ~BUTTON(SAVE);
        song_save_press();
    }
    if ((song_port_recording() || song_perf_state()) && (*pressed & BUTTON(REC))) { /* REC ends the take at once and playing goes on (PLAY: song_buttons.inc) */
        song_perf_end();
        song_port_record_end(1);
        *pressed &= ~BUTTON(REC);
    }
    if (*pressed & BUTTON(PLAY)) {
        if (shift) song_port_jump(s->loop_on ? s->loop_start : 0);
        else if (p->playing) song_port_stop();
        else song_start(now);
    }
    if (*pressed & BUTTON(REC)) { /* SHIFT+REC: step entry. REC: a take as it comes up, or held, the settings (0.4.0) */
        *pressed &= ~BUTTON(REC);
        if (shift) song_rec_go(page, 1, now);
        else song_rec_down(now);
        if (page == SONG_STEP) return 1;
    }
    if (*pressed & BUTTON(ENV)) {
        if (!shift) s->loop_on ^= 1U;
        else { /* SHIFT+ENV: metronome off, on, while recording */
            static const char *const clicks[3] = {"METRONOME OFF", "METRONOME ON", "METRONOME WHILE RECORDING"};
            s->metronome = (uint8_t)((s->metronome + 1U) % 3U);
            p->click = s->metronome == 1 || (s->metronome && song_port_recorder()->active);
            notice = clicks[s->metronome];
            notice_until = now + 1500U;
        }
    }
    if ((*pressed & BUTTON(LFO)) && shift) { /* SHIFT+LFO: the automation view */
        page = music_page = SONG_AUTO;
        cache->shown_page = ~0U;
        return 1;
    }
    if ((*pressed & BUTTON(SCL)) && shift) song_rec_toggle(now); /* SHIFT+SEL: REPLACE or OVERDUB (0.4.0) */
    else if ((*pressed & BUTTON(SCL)) && !clip) song_marker_key(now);
    if ((*pressed & BUTTON(EDIT)) && shift) { /* SHIFT+EDIT: the edit jobs (EDIT alone stays sound editing) */
        *pressed &= ~BUTTON(EDIT);
        song_job_open();
        return 1;
    }
    *pressed &= ~(BUTTON(PLAY) | BUTTON(FX) | BUTTON(ENV) | BUTTON(ARP) | BUTTON(SCL) | BUTTON(LFO));
    if (*pressed & (BUTTON(HOME) | BUTTON(EDIT) | BUTTON(SAVE)))
        return 0;
    octave = clamp(octave + !!(*pressed & BUTTON(OCT_UP)) - !!(*pressed & BUTTON(OCT_DOWN)), -2, 2);
    select_track(c->encoders[0]);
    if (c->encoders[1] && page == SONG && song_port_recording()) step_zoom_turn(c->encoders[1]); /* the take's roll */
    else if (c->encoders[1]) song_zoom_turn(c->encoders[1]);
    if (song_port_recorder()->active) { /* K1 to K4 record parameters on the take track */
        for (unsigned k = 0; k < 4; ++k)
            if (c->encoders[2 + k]) song_knob_turn(k, fm_encoder_delta(2 + k, c->encoders[2 + k]), now);
        return 1;
    }
    int move = c->encoders[2];
    if (move && shift && !clip) {
        /* Marker to marker, from a pending jump or the playhead. */
        uint32_t at = p->playing ? (p->jumping ? p->jump : song_player_clock(p)) : p->playhead;
        int direction = move > 0 ? 1 : -1, found = -1;
        for (; move; move -= direction) {
            int next = song_marker_step(s, at, direction);
            if (next < 0) break;
            found = next;
            at = s->marker[next].clock;
        }
        if (found >= 0) song_port_jump(at);
    } else if (move && !shift)
        song_port_jump(song_snap_move(s, song_player_target(p), song_zoom, move));
    if (c->encoders[3] && shift) { if (!clip) song_sections(c->encoders[3]); }
    else if (c->encoders[3]) {
        uint32_t at = song_snap_move(s, s->loop_start, song_zoom, c->encoders[3]);
        if (at < s->loop_end) s->loop_start = at;
    }
    if (c->encoders[4]) {
        uint32_t at = song_snap_move(s, s->loop_end, song_zoom, c->encoders[4]);
        if (at > s->loop_start) s->loop_end = at;
    }
    if (c->encoders[5]) song_zoom_turn(c->encoders[5]);
    uint32_t down = key_down & ~blocked_keys;
    if (shift) {
        /* SHIFT + the lower white keys, F3 to E4: copy, cut, paste inserting,
         * paste over, duplicate, insert bars, delete bars. The upper ones,
         * F4 to G5: markers 1 to 9. */
        for (unsigned i = 0; i < 7; ++i)
            if (down & (1U << white_keys[i])) song_action(i, now);
        static const uint8_t mangle_keys[3] = {1, 3, 5}; /* F#3, G#3, A#3 */
        for (unsigned i = 0; i < 3; ++i)
            if (down & (1U << mangle_keys[i])) song_mangle(i, now);
        for (unsigned i = 0; i < 9 && !clip; ++i)
            if (down & (1U << white_keys[7 + i])) {
                if (i < s->markers) song_port_jump(s->marker[i].clock);
                else { notice = "NO MARKER THERE"; notice_until = now + 1500U; }
            }
        blocked_keys |= key_down; /* SHIFT keys never play notes */
    } else if (picking) {
        song_pick_tracks(down);
        blocked_keys |= key_down;
    }
    return 1;
}
