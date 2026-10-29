/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* song-mode: buttons that work across the song pages (0.3.0), included by
 * song_panel.inc. SAVE opens the song list as it comes up, so HOME+SAVE and
 * SHIFT+HOME+SAVE (undo and redo) never show it whichever goes down first
 * (the undo combo in screen.c lets go of it). On the list SAVE saves. On the
 * song view, the grid, the automation view and the settings, a quick tap of
 * FX opens the stock effects page, as GLO's tap opens the settings, and
 * held it is still SHIFT. ARP there is the stock arpeggiator's: the stock
 * code opens its settings on a hold, a tap turns it on or off for the
 * track, and HOME on its settings comes back. PLAY during a take ends the
 * recording and playing goes on. REC on the song view, the automation view
 * and the grid tells a tap from a hold (0.4.0): a tap records, a hold opens
 * the record settings, and SHIFT+REC is step entry. */
static uint8_t song_save_down, song_fx_from, song_fx_used, song_arp_from, song_arp_back;
static uint32_t song_fx_ms;
static int release_keys(void);

static unsigned song_stock_buttons(unsigned p) { return p == SONG || p == SONG_GRID || p == SONG_AUTO || p == SONG_SETUP; }
static void song_save_press(void) { song_save_down = 1; }
/* Where SAVE comes up after a press that saved before 0.3.0: the list. */
static int song_save_release(const struct fm1_controls *c, uint32_t now)
{
    if (!song_save_down || (c->buttons & BUTTON(SAVE))) return 0;
    song_save_down = 0;
    if (song_port_recording()) { notice = "STOP RECORDING FIRST"; notice_until = now + 1500U; return 0; }
    song_list_open();
    return 1;
}
/* FX tapped, nothing else used meanwhile: the effects page, whose own FX
 * comes back to the page it was opened from (open_subpage). */
static int song_fx_tap(const struct fm1_controls *c, uint32_t pressed, uint32_t key_down, uint32_t now)
{
    if ((pressed & BUTTON(FX)) && song_stock_buttons(page)) { song_fx_from = (uint8_t)page; song_fx_used = 0; song_fx_ms = now; }
    if (!song_fx_from) return 0;
    if ((c->buttons & ~BUTTON(FX)) || (pressed & ~BUTTON(FX)) || key_down || encoder_active(c->encoders) ||
        now - song_fx_ms >= 600U) song_fx_used = 1;
    if (c->buttons & BUTTON(FX)) return 0;
    unsigned from = song_fx_from;
    song_fx_from = 0;
    if (song_fx_used || page != from) return 0;
    effects_page = 0;
    open_subpage(EFFECTS);
    cache->shown_page = ~0U;
    return 1;
}
/* ARP on the pages above: a tap toggles the track's arpeggiator, as on the
 * stock pages. A hold has the stock code open its settings, and from there
 * HOME comes back to the song page. Elsewhere on the song pages ARP is
 * theirs (step entry's glitch layer) and never opens the settings. */
static int song_arp(const struct fm1_controls *c, uint32_t *pressed, uint32_t now)
{
    if (page != ARP_EDIT) song_arp_back = 0;
    if (page >= SONG && !song_stock_buttons(page)) { arp_down = 0; song_arp_from = 0; return 0; }
    if ((*pressed & BUTTON(ARP)) && song_stock_buttons(page)) song_arp_from = (uint8_t)page;
    if (page == ARP_EDIT && song_arp_from) { song_arp_back = song_arp_from; song_arp_from = 0; }
    if (page == ARP_EDIT && song_arp_back && (*pressed & BUTTON(HOME))) {
        *pressed &= ~BUTTON(HOME);
        page = music_page = song_arp_back;
        song_arp_back = 0;
        cache->shown_page = ~0U;
        return 1;
    }
    if (!song_arp_from || (c->buttons & BUTTON(ARP))) return 0;
    unsigned tapped = page == song_arp_from;
    song_arp_from = 0;
    if (!tapped) return 0;
    (void)release_keys();
    blocked_keys |= c->keys;
    groove_arp_set(track, !groove_arp_enabled(track));
    notice = groove_arp_enabled(track) ? "ARP ON / HOLD ARP FOR SETTINGS" : "ARP OFF / LIVE KEYS";
    notice_until = now + 2000U;
    return 0;
}
/* PLAY during any take, or an armed one: the recording ends, playing goes
 * on. In a count-in the take is called off and playing stops. */
static void song_play_ends_take(uint32_t *pressed)
{
    if (!(*pressed & BUTTON(PLAY)) || !(song_port_recording() || (song_perf_state() && song_port_player()->playing))) return;
    *pressed &= ~BUTTON(PLAY);
    song_perf_end();
    song_port_record_end(1);
}
/* What a tap of REC (step 0) or SHIFT+REC (step 1) does on a page (0.4.0).
 * The song view and the automation view: stopped, a take in the song's
 * live mode with its count-in, or step entry at the playhead (in the open
 * clip, with one in the editor). Playing, REC arms an open clip's next pass
 * and otherwise says to stop first, and step entry waits for a stop too.
 * The grid has its own (grid_rec_go). */
static void song_rec_go(unsigned from, unsigned step, uint32_t now)
{
    struct song *s = song_port_song();
    unsigned playing = song_port_player()->playing;
    notice = NULL;
    notice_until = now + 2000U;
    if (from == SONG_GRID) grid_rec_go(step, now);
    else if (step && playing) notice = "STOP FIRST FOR STEP ENTRY";
    else if (step) song_step_enter();
    else if (playing) notice = s->open && !song_port_record_clip(0) ? "REC FROM THE CLIP'S NEXT PASS" : "STOP FIRST TO RECORD";
    else if (song_port_record(s->rec_mode, track)) notice = "STOP FIRST";
}
/* REC down with nothing to end (song_rec_down): let go within 600 ms it is
 * a tap, acted on as it comes up, and held it opens the record settings.
 * Leaving the page meanwhile drops it. 1 when the settings opened. */
static int song_rec_held(const struct fm1_controls *c, uint32_t now)
{
    unsigned from = song_rec_from, held = (c->buttons & BUTTON(REC)) != 0;
    if (!from || (held && now - song_rec_ms < 600U)) return 0;
    song_rec_from = 0;
    if (page != from) return 0;
    if (held) { song_rec_open(from); return 1; }
    song_rec_go(from, 0, now);
    return 0;
}
