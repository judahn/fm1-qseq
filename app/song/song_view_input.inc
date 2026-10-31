/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* song-mode: the song view's input, included by song_panel.inc: transport,
 * REC (song_rec.inc), the playhead on PRESETS and the zoom on ALGO, side by
 * side on the panel, then K1 the tempo, K2 input quantize and K3 and K4 the
 * loop (0.5.2: K1 moved the playhead, K2 and K3 were the loop and K4
 * zoomed), markers, the selection and arranging keys, and the knobs
 * recording parameters during a take, when ALGO zooms the roll the take
 * shows and PRESETS does nothing. Every move of the playhead here sets the
 * start point (song_port_jump). The automation view runs its controls
 * through here too, its own K1 and K2 first (song_auto.inc). */

/* ALGO's zoom, and the span and snap in a notice, since the footer no
 * longer shows them (0.5.2). A take's footer is the recording knobs. */
static void song_zoom_turn(int delta, uint32_t now)
{
    static char line[32];
    unsigned zoom = (unsigned)clamp((int)song_zoom + delta, 0, SONG_ZOOMS - 1);
    if (zoom != song_zoom) { song_zoom = zoom; song_window = ~0U; }
    if (song_port_recorder()->active) return;
    strcpy(line, "ZOOM "); strcat(line, song_spans[zoom]);
    strcat(line, "  SNAP "); strcat(line, song_snaps[zoom]);
    notice = line;
    notice_until = now + 1500U;
}
static int song_view_input(const struct fm1_controls *c, uint32_t *pressed, uint32_t key_down, uint32_t now)
{
    struct song *s = song_port_song();
    struct song_player *p = song_port_player();
    unsigned shift = (c->buttons & BUTTON(FX)) != 0, clip = s->open != 0; /* a clip in the editor: no markers */
    unsigned picking = (c->buttons & BUTTON(SCL)) != 0 && !clip && page == SONG; /* SEL held picks tracks (0.5.6, LFO before) */
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
    if ((*pressed & BUTTON(LFO)) && !clip && page == SONG) song_marker_key(now); /* LFO: markers (0.5.6, SEL before) */
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
    else if (c->encoders[1]) song_zoom_turn(c->encoders[1], now);
    if (song_port_recorder()->active) { /* K1 to K4 record parameters on the take track */
        for (unsigned k = 0; k < 4; ++k)
            if (c->encoders[2 + k]) song_knob_turn(k, fm_encoder_delta(2 + k, c->encoders[2 + k]), now);
        return 1;
    }
    int move = c->encoders[6]; /* PRESETS, beside ALGO: where the playhead is (0.5.2) */
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
    /* K1: the tempo the header shows, the starting tempo or the change the
     * playhead is past, where the song is with a clip open (song_tempo_now),
     * whole BPM or half with SHIFT, heard at once and the engine's following
     * (song_tempo_follow). Settings page 1 keeps the
     * starting one. K2: input quantize, the record settings' K3, and the
     * last take again with it. */
    if (c->encoders[2]) {
        const struct song_tempo *t = &s->tempo[song_tempo_now()];
        (void)song_port_set_tempo(t->clock, song_tempo_step(t->bpm100, c->encoders[2], shift));
    }
    if (c->encoders[3]) {
        unsigned quantize = s->quantize;
        s->quantize = (uint8_t)clamp(s->quantize + c->encoders[3], 0, 14);
        if (song_rec_retake(quantize != s->quantize)) { notice = "REQUANTIZED THE LAST TAKE"; notice_until = now + 1500U; }
    }
    if (c->encoders[4] && shift) { if (!clip) song_sections(c->encoders[4]); }
    else if (c->encoders[4]) {
        uint32_t at = song_snap_move(s, s->loop_start, song_zoom, c->encoders[4]);
        if (at < s->loop_end) s->loop_start = at;
    }
    if (c->encoders[5]) {
        uint32_t at = song_snap_move(s, s->loop_end, song_zoom, c->encoders[5]);
        if (at > s->loop_start) s->loop_end = at;
    }
    uint32_t down = key_down & ~blocked_keys;
    if (shift) {
        /* SHIFT + the lower white keys, F3 to E4: copy, cut, paste, paste,
         * duplicate, insert bars, delete the selection. D#4 clears the
         * clipboard. The upper white keys, F4 to G5: markers 1 to 9. */
        for (unsigned i = 0; i < 7; ++i)
            if (down & (1U << white_keys[i])) song_action(i, now);
        if (down & (1U << 10)) song_clipboard_clear(now); /* D#4 */
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
