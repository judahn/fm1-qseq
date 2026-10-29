/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* song-mode: the song list (SHIFT+SAVE, phase 5a). Songs in flash with the
 * memory gauge on top. K1 or SELECT picks a song, K2 the action, K3 the kit
 * (the pads rebind at once) and K4 the sounds a new song starts from. YES
 * (OCT+) does the action, and the hint says what it will do and to what.
 * NO (OCT-) cancels a question or a naming, and otherwise goes back to the
 * song view, as HOME does. SAVE saves. The keys do nothing here. A recovery
 * copy found at boot sits on top, and loading it restores it. The actions
 * are in song_list_act.inc. */

enum { LIST_LOAD, LIST_SAVE, LIST_SAVE_AS, LIST_NEW, LIST_RENAME, LIST_DUPLICATE, LIST_DELETE, LIST_ACTIONS };
static const char *const song_list_actions[LIST_ACTIONS] = {"LOAD", "SAVE", "SAVE AS", "NEW", "RENAME", "DUPLICATE", "DELETE"};
static unsigned song_list_at, song_list_from, song_list_act, song_list_cursor; /* the action is kept while the device is on */
static int song_list_source = -1; /* blank, or a factory sound set */
static char song_list_name[12];
static unsigned song_list_rows(void) { return song_store_count() + song_store_recovery(); }
static unsigned song_list_id(unsigned row) { return song_store_id(row - song_store_recovery()); }
static const char *song_store_message(int r, const char *done)
{
    return !r ? done : r == SONG_STORE_PLAYING ? "STOP FIRST" : r == SONG_STORE_FULL ? "MEMORY FULL" : "COULDN'T, FLASH ERROR";
}
#include "song_list_act.inc"

static void song_kb(char out[12], uint32_t bytes) { decimal(out, (int)((bytes + 1023U) / 1024U), 1); strcat(out, "K"); }
/* Flash use as a bar: songs, then samples and kits, then free. */
static void song_gauge_bar(unsigned x, unsigned y, unsigned w, unsigned h, const struct song_gauge *g)
{
    unsigned songs = (unsigned)((uint64_t)g->songs * w / g->pool), samples = (unsigned)((uint64_t)g->samples * w / g->pool);
    paint_fill(x, y, w, h, PANEL);
    paint_fill(x, y, songs, h, GREEN);
    paint_fill(x + songs, y, samples, h, AMBER);
}
static void song_list_draw(void)
{
    struct song_gauge g;
    song_store_gauge(&g);
    char line[40], part[12];
    unsigned naming = song_naming();
    uint32_t sig = signature(song_list_at ^ naming << 9 ^ song_list_cursor << 10 ^ (uint32_t)song_store_kit() << 16, &g, sizeof g);
    sig = signature(sig, song_list_name, sizeof song_list_name) ^ song_list_rows() << 24 ^ song_port_song()->id ^ song_ask_signature();
    if ((!paint_rendering || paint_top < 196) && sig != cache->middle_cache) {
        cache->middle_cache = sig;
        paint_fill(0, 0, 240, 196, BACK);
        if (paint_rendering) {
            text(8, 6, "SONGS", 2, ACCENT, BACK);
            strcpy(line, "FREE "); song_kb(part, g.free); strcat(line, part);
            text(232 - 6 * (unsigned)strlen(line), 10, line, 1, DIM, BACK);
            song_gauge_bar(8, 26, 224, 4, &g);
            strcpy(line, "SONGS "); song_kb(part, g.songs); strcat(line, part);
            strcat(line, "  KITS "); song_kb(part, g.samples); strcat(line, part);
            strcat(line, "  OF "); song_kb(part, g.pool); strcat(line, part);
            text(8, 34, line, 1, DIM, BACK);
            strcpy(line, "SONG RAM "); decimal(part, (int)g.events, 1); strcat(line, part);
            strcat(line, "/"); decimal(part, SONG_EVENTS, 1); strcat(line, part); strcat(line, " EVENTS");
            text(8, 45, line, 1, DIM, BACK);
            unsigned rows = song_list_rows();
            if (song_list_at < song_list_from) song_list_from = song_list_at;
            if (song_list_at >= song_list_from + 7U) song_list_from = song_list_at - 6U;
            for (unsigned r = song_list_from; r < rows && r < song_list_from + 7U; ++r) {
                unsigned y = 58 + (r - song_list_from) * 19, on = r == song_list_at, id = song_list_id(r);
                char name[17];
                paint_fill(4, y, 232, 17, on ? PANEL : BACK);
                if (song_store_recovery() && !r) { text(10, y + 5, "RESTORE THE UNSAVED SONG", 1, AMBER, on ? PANEL : BACK); continue; }
                song_store_name(id, name);
                if (on && naming) { memcpy(name, song_list_name, 12); name[12] = 0; }
                text(10, y + 5, id == song_port_song()->id ? ">" : " ", 1, AMBER, on ? PANEL : BACK);
                text(20, y + 5, name, 1, on ? TEXT : DIM, on ? PANEL : BACK);
                if (on && naming) paint_fill(20 + song_list_cursor * 6, y + 13, 5, 1, AMBER);
                song_kb(part, song_store_bytes(id));
                text(232 - 6 * (unsigned)strlen(part), y + 5, part, 1, DIM, on ? PANEL : BACK);
            }
            if (!rows) text(10, 63, "NO SONGS YET: SAVE STORES THIS ONE", 1, DIM, BACK);
            song_ask_draw();
        }
    }
    if (paint_rendering && paint_top + PAINT_ROWS <= 195) return;
    char kit[17] = "-", from[17];
    if (song_store_kit()) song_store_kit_name(song_store_kit(), kit);
    song_list_from_name(from);
    kit[9] = from[9] = 0; /* the knob's width, so the kit's name stops short of K4's */
    knob(0, naming ? "CURSOR" : "SONG", naming ? "< >" : "", colors[0]);
    knob(1, naming ? "LETTER" : "ACTION", naming ? "A-Z" : song_list_actions[song_list_act], colors[1]);
    knob(2, "KIT", kit, colors[2]);
    knob(3, "NEW FROM", from, colors[3]);
    song_notice_expire();
    song_hint(notice ? notice : naming ? SONG_NAMING_HINT : song_list_hint());
}

static void song_list_open(void)
{
    song_port_clip_close(); /* a clip in the editor goes back first: songs may load */
    page = music_page = SONG_LIST;
    song_ask_close();
    cache->shown_page = ~0U;
}
/* SAVE in the song view, and on the stock pages too: the song is the project. */
static void song_save_key(uint32_t now)
{
    notice = song_store_message(song_store_save(0), "SONG SAVED");
    notice_until = now + 1500U;
}
static int song_list_input(const struct fm1_controls *c, uint32_t *pressed, uint32_t key_down, uint32_t now)
{
    int answer = song_yes_no(pressed);
    unsigned open = song_asking() || song_naming(), rows = song_list_rows();
    blocked_keys |= key_down; /* the keys do nothing here */
    if ((*pressed & BUTTON(HOME)) || (answer < 0 && !open)) { /* back to the song view */
        *pressed &= ~BUTTON(HOME);
        page = music_page = SONG;
        cache->shown_page = ~0U;
        return 1;
    }
    if ((*pressed & BUTTON(SAVE)) && !open) song_save_key(now); /* not while something waits for YES or NO */
    *pressed &= ~(BUTTON(PLAY) | BUTTON(REC) | BUTTON(SAVE) | BUTTON(FX) | BUTTON(SCL) | BUTTON(ENV) | BUTTON(LFO) | BUTTON(ARP));
    if (*pressed & (BUTTON(EDIT) | BUTTON(GLO))) return 0;
    if (answer < 0) { song_ask_close(); return 1; } /* NO: the question or the naming goes, the old name stays */
    if (song_naming()) { /* K1 or SELECT the letter's place, K2 the letter, YES keeps the name */
        song_list_cursor = (unsigned)clamp((int)song_list_cursor + c->encoders[2] + c->encoders[0], 0, 11);
        if (c->encoders[3]) song_list_name[song_list_cursor] = edit_character(song_list_name[song_list_cursor], c->encoders[3]);
        if (answer > 0) song_list_renamed(now);
        return 1;
    }
    unsigned at = rows ? (unsigned)clamp((int)song_list_at + c->encoders[2] + c->encoders[0], 0, (int)rows - 1) : 0;
    int source = clamp(song_list_source + c->encoders[5], -1, GROOVE_FACTORY_PROJECTS - 1);
    if (at != song_list_at || c->encoders[3] || source != song_list_source) song_ask_close(); /* another song, action or start */
    song_list_at = at;
    song_list_act = (unsigned)clamp((int)song_list_act + c->encoders[3], 0, LIST_ACTIONS - 1);
    song_list_source = source;
    if (c->encoders[4] && song_store_kits()) {
        unsigned k = 0;
        while (k < song_store_kits() && song_store_kit_id(k) != song_store_kit()) ++k;
        k = (unsigned)clamp((int)k + c->encoders[4], 0, (int)song_store_kits() - 1);
        notice = song_store_message(song_store_use_kit(song_store_kit_id(k)), "KIT CHANGED, PADS REBOUND");
        notice_until = now + 1500U;
    }
    if (answer > 0) song_list_yes(now);
    return 1;
}
