/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* song-mode: panel input and the control poll for the song pages.
 * Included by song_view.inc, so it shares screen.c's panel state. */

#include "song_lights.inc"

static void song_start(uint32_t now)
{
    struct groove_view v = {0};
    groove_get_view(track, cursor, &v);
    if (v.playing) (void)post(0xfc, 0, 0, 1); /* the step sequencer stays stopped */
    writing = 0;
    if (song_port_start()) { notice = "SONG PLAYBACK: PREVIEW ONLY FOR NOW"; notice_until = now + 2000U; }
}

#if SONG_UI
/* The song view: transport, snap and zoom, loop and markers. */
static int song_view_input(const struct fm1_controls *c, uint32_t *pressed, uint32_t key_down, uint32_t now)
{
    struct song *s = song_port_song();
    struct song_player *p = song_port_player();
    unsigned shift = (c->buttons & BUTTON(FX)) != 0;
    unsigned picking = (c->buttons & BUTTON(LFO)) != 0;
    unsigned glitching = song_stutter_input(c, *pressed, key_down, now);
    if (*pressed & BUTTON(SAVE)) { /* SAVE saves the song, SHIFT+SAVE opens the list */
        *pressed &= ~BUTTON(SAVE);
        if (shift) { song_list_open(); return 1; }
        song_save_key(now);
    }
    if (song_port_recorder()->active && (*pressed & (BUTTON(PLAY) | BUTTON(REC)))) {
        song_port_record_stop(); /* PLAY or REC ends the take */
        *pressed &= ~(BUTTON(PLAY) | BUTTON(REC));
    }
    if (*pressed & BUTTON(PLAY)) {
        if (shift) song_port_jump(s->loop_on ? s->loop_start : 0);
        else if (p->playing) song_port_stop();
        else song_start(now);
    }
    if (*pressed & BUTTON(REC)) { /* the mode chooser */
        *pressed &= ~BUTTON(REC);
        if (!p->playing) { page = music_page = SONG_REC; cache->shown_page = ~0U; return 1; }
        notice = "STOP FIRST TO RECORD";
        notice_until = now + 2000U;
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
    if (*pressed & BUTTON(SCL)) song_add_marker_here(now);
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
    if (song_port_recorder()->active) { /* K1 to K4 record parameters on the take track */
        for (unsigned k = glitching; k < 4; ++k) /* K1 drifts the stutter while GLITCH is down */
            if (c->encoders[2 + k]) song_knob_turn(k, fm_encoder_delta(2 + k, c->encoders[2 + k]), now);
        return 1;
    }
    int move = glitching ? 0 : c->encoders[2];
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
        if (found >= 0) song_port_jump(at);
    } else if (move)
        song_port_jump(song_snap_move(s, song_player_target(p), song_zoom, move));
    if (c->encoders[3] && shift) song_sections(c->encoders[3]);
    else if (c->encoders[3]) {
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
        for (unsigned i = 0; i < 9; ++i)
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
#endif

/* Panel input. Returns 1 when a song page consumed this poll. SEQ opens the
 * song view, and from there the stock step page. On the step page SEQ keeps
 * its stock long press (actions), and a tap comes back to the song. GLO on a
 * song page: a tap opens or closes the settings, a hold is still the stock
 * mixer layer, and SHIFT+GLO opens the stock device page. */
static unsigned song_seq_on_steps;
enum { SONG_HOME = SONG_UI ? SONG : SONG_DIAG }; /* where SEQ goes */
static int song_input_pages(const struct fm1_controls *c, uint32_t *pressed, uint32_t key_down, uint32_t now)
{
    if ((*pressed & BUTTON(SEQ)) && !(c->buttons & BUTTON(HOME))) {
        if (page == STEPS) { song_seq_on_steps = 1; return 0; }
#if SONG_UI && SONG_HW_TEST
        if (page == SONG && (c->buttons & BUTTON(FX))) { /* SHIFT+SEQ: hardware diagnostics */
            page = music_page = SONG_DIAG;
            cache->shown_page = ~0U;
            return 1;
        }
#endif
#if SONG_UI
        if (page == SONG_STEP) song_step_leave(0, now);
        else
#endif
        if (page != SONG_HOME) { page = music_page = SONG_HOME; writing = 0; }
        else { page = music_page = STEPS; seq_page = SONG_DROP_STEPS ? 4 : 0; step_follow = 1; } /* sync and MIDI pages only, without (a) */
        cache->shown_page = ~0U;
#if SONG_UI
        static uint8_t offered; /* a recovery copy from before: the list offers it, once */
        if (page == SONG && song_store_recovery() && !offered) { offered = 1; song_list_at = 0; song_list_open(); }
#endif
        return 1;
    }
    if (song_seq_on_steps && !(c->buttons & BUTTON(SEQ))) {
        song_seq_on_steps = 0;
        if (page == STEPS && now - seq_ms < 600U) {
            page = music_page = SONG_HOME; writing = 0;
            cache->shown_page = ~0U;
            return 1;
        }
    }
#if SONG_UI
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
#endif
    if (page < SONG) {
        if ((*pressed & BUTTON(PLAY)) && song_port_player()->playing) { song_port_stop(); *pressed &= ~BUTTON(PLAY); }
#if SONG_UI
        /* SAVE on the stock pages saves the song too, never a stock project
         * slot, which now lies in the song pool. */
        if ((*pressed & BUTTON(SAVE)) && page != PROJECT && page != BLE && page != SOURCE && page != ACTIONS) {
            *pressed &= ~BUTTON(SAVE);
            if (c->buttons & BUTTON(FX)) song_list_open();
            else song_save_key(now);
            return 1;
        }
#endif
        return 0;
    }
    music_page = page;
    if (glo_down) return 0; /* the stock mixer layer */
#if SONG_HW_TEST
    if (page == SONG_DIAG) return song_diag_input(c, pressed, key_down, now);
#endif
#if SONG_UI
    if (page == SONG_STEP) return song_step_input(c, pressed, key_down, now);
    if (page == SONG_SETUP) return song_setup_input(c, pressed, key_down, now);
    if (page == SONG_REC) return song_rec_input(c, pressed, key_down, now);
    if (page == SONG_AUTO) return song_auto_input(c, pressed, key_down, now);
    if (page == SONG_LIST) return song_list_input(c, pressed, key_down, now);
    if (page == SONG_JOB) return song_job_input(c, pressed, key_down, now);
    return song_view_input(c, pressed, key_down, now);
#else
    return 0;
#endif
}
/* Every poll's edits are one undo step. A take keeps its own step open. */
static int song_input(const struct fm1_controls *c, uint32_t *pressed, uint32_t key_down, uint32_t now)
{
#if SONG_UI
    if (page >= SONG) arp_down = 0; /* ARP is GLITCH here, never the stock arpeggiator page */
    if (!(c->buttons & (BUTTON(HOME) | BUTTON(SAVE)))) song_combo_from = 0;
    else if ((*pressed & (BUTTON(HOME) | BUTTON(SAVE))) && page >= SONG) song_combo_from = page;
    song_undo_begin(song_port_undo(), song_port_song());
    int result = song_input_pages(c, pressed, key_down, now);
    song_undo_end(song_port_undo(), song_port_song());
    return result;
#else
    return song_input_pages(c, pressed, key_down, now);
#endif
}

/* Each control poll: the device stub, preview note-offs after 300 ms, and
 * the stock step sequencer kept stopped while the song plays. */
#if SONG_UI
static uint16_t song_preview_serial;
static uint32_t song_preview_since;
#endif
static void song_poll(const struct groove_view *transport)
{
    song_port_poll();
#if SONG_UI
    struct song_step *st = song_port_step();
    if (st->preview_count) {
        uint32_t now = fm1_millis();
        if (st->serial != song_preview_serial) { song_preview_serial = st->serial; song_preview_since = now; }
        else if (now - song_preview_since >= 300U) step_preview_stop(st);
    }
#endif
    if (song_port_player()->playing && transport->playing) (void)post(0xfc, 0, 0, 1);
}
