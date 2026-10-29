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
#include "song_buttons.inc"
#include "song_view_input.inc"
#endif

/* Panel input. Returns 1 when a song page consumed this poll. SEQ opens the
 * song view, and from there the clip grid (8f), and back. With a clip open in
 * the editor SEQ closes it and goes to the grid, and SHIFT+SEQ hands every
 * track back to the song. Without the song pages SEQ opens the diagnostics
 * page and from there the stock step page. On the step page SEQ keeps its
 * stock long press (actions), and a tap comes back to the song. GLO on a
 * song page: a tap opens or closes the settings, a hold is still the stock
 * mixer layer, and SHIFT+GLO opens the device settings (song_device_turn). */
static unsigned song_seq_on_steps;
enum { SONG_HOME = SONG_UI ? SONG : SONG_DIAG }; /* where SEQ goes */
#if SONG_UI
/* The device settings (8a): Bluetooth on SHIFT+GLO, then ALGO turns on to
 * the stock sync and MIDI pages (the step page's 5 to 8), and back. Within
 * the step page's pages the stock code turns. */
static int song_device_turn(int turn)
{
    unsigned to = turn > 0 && page == BLE ? 4U : turn < 0 && page == STEPS && seq_page == 4 ? 1U : 0U;
    if (!to) return 0;
    if (to == 1) { page = BLE; ble_view = ble_forget = ble_action = 0; ble_result = 0; }
    else { page = STEPS; seq_page = to; step_follow = 1; }
    cache->shown_page = ~0U;
    return 1;
}
#endif
static int song_input_pages(const struct fm1_controls *c, uint32_t *pressed, uint32_t key_down, uint32_t now)
{
#if SONG_UI
    if ((page == BLE || page == STEPS) && song_device_turn(c->encoders[1])) return 1;
#endif
    if ((*pressed & BUTTON(SEQ)) && !(c->buttons & BUTTON(HOME))) {
        if (page == STEPS) { song_seq_on_steps = 1; return 0; }
#if SONG_UI
        if (song_port_recorder()->active || song_grid_rec_state()) { notice = "STOP RECORDING FIRST"; notice_until = now + 1500U; return 1; }
        if (page >= SONG && (c->buttons & BUTTON(FX))) { /* SHIFT+SEQ: every track back to the song */
            for (unsigned t = 0; t < SONG_TRACKS; ++t) song_port_launch(t, SONG_FROM_SONG);
            notice = "EVERY TRACK BACK TO THE SONG";
            notice_until = now + 1500U;
            return 1;
        }
        unsigned from = page;
        if (page == SONG_STEP) song_step_leave(0, now);
        if (song_port_song()->open) { song_port_clip_close(); page = SONG_GRID; }
        else page = from == SONG ? SONG_GRID : SONG;
        music_page = page;
        writing = 0;
#else
        if (page != SONG_HOME) { page = music_page = SONG_HOME; writing = 0; }
        else { page = music_page = STEPS; seq_page = SONG_DROP_STEPS ? 4 : 0; step_follow = 1; } /* sync and MIDI pages only, without (a) */
#endif
        cache->shown_page = ~0U;
#if SONG_UI
        static uint8_t offered; /* a recovery copy from before: the list offers it, once */
        if (page == SONG && song_store_recovery() && !offered) { offered = 1; song_list_at = 0; song_list_act = LIST_LOAD; song_list_open(); }
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
        /* SAVE on the stock pages opens the song list too, as it comes up,
         * never a stock project slot, which now lies in the song pool. */
        if ((*pressed & BUTTON(SAVE)) && page != PROJECT && page != BLE && page != SOURCE && page != ACTIONS) {
            *pressed &= ~BUTTON(SAVE);
            song_save_press();
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
    if (page == SONG_GRID) return song_grid_input(c, pressed, key_down, now);
    return song_view_input(c, pressed, key_down, now);
#else
    return 0;
#endif
}
/* Every poll's edits are one undo step. A take keeps its own step open.
 * The engine's tempo and the song's meet here (song_tempo.c): a change
 * made to the engine's since the last poll goes into the song first, and
 * after the page's input the engine follows the song. */
static int song_input(const struct fm1_controls *c, uint32_t *pressed, uint32_t key_down, uint32_t now)
{
#if SONG_UI
    if (!(c->buttons & (BUTTON(HOME) | BUTTON(SAVE)))) song_combo_from = 0;
    else if ((*pressed & (BUTTON(HOME) | BUTTON(SAVE))) && page >= SONG) song_combo_from = page;
    song_port_battery(c->battery, now);
    song_undo_begin(song_port_undo(), song_port_song());
    song_tempo_adopt();
    song_play_ends_take(pressed);
    int result = song_save_release(c, now);
    result |= song_fx_tap(c, *pressed, key_down, now);
    result |= song_arp(c, pressed, now);
    if (!result) result = song_input_pages(c, pressed, key_down, now);
    if (song_port_take_full()) { notice = "SONG FULL, RECORDING ENDED"; notice_until = now + 2500U; }
    if (page != song_open_page) song_ask_close(); /* leaving a page drops its question or naming */
    song_clip_follow(song_port_song()); /* an open clip's length is its loop end */
    song_tempo_follow(1);
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
