/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* song-mode: the song list (SHIFT+SAVE, phase 5a). Songs in flash with the
 * memory gauge on top. K1 or SELECT picks a song, K2 the sounds a new song
 * starts from, K3 the kit (the pads rebind at once). F3 to E4: load, save,
 * save as, new, rename, duplicate, delete (SHIFT shows the last three).
 * Load and delete ask for the key again. A recovery copy found at boot sits on top,
 * and loading it restores it. HOME goes back to the song view. */

static unsigned song_list_at, song_list_from, song_list_confirm, song_list_naming, song_list_cursor;
static int song_list_source = -1; /* blank, or a factory sound set */
static char song_list_name[12];
static unsigned song_list_rows(void) { return song_store_count() + song_store_recovery(); }
static unsigned song_list_id(unsigned row) { return song_store_id(row - song_store_recovery()); }

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
    uint32_t sig = signature(song_list_at ^ song_list_confirm << 8 ^ song_list_naming << 9 ^ song_list_cursor << 10 ^
                             (uint32_t)song_store_kit() << 16, &g, sizeof g);
    sig = signature(sig, song_list_name, sizeof song_list_name) ^ song_list_rows() << 24 ^ song_port_song()->id;
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
                if (on && song_list_naming) { memcpy(name, song_list_name, 12); name[12] = 0; }
                text(10, y + 5, id == song_port_song()->id ? ">" : " ", 1, AMBER, on ? PANEL : BACK);
                text(20, y + 5, name, 1, on ? TEXT : DIM, on ? PANEL : BACK);
                if (on && song_list_naming) paint_fill(20 + song_list_cursor * 6, y + 13, 5, 1, AMBER);
                song_kb(part, song_store_bytes(id));
                text(232 - 6 * (unsigned)strlen(part), y + 5, part, 1, DIM, on ? PANEL : BACK);
            }
            if (!rows) text(10, 63, "NO SONGS YET: G3 SAVES THIS ONE", 1, DIM, BACK);
        }
    }
    if (paint_rendering && paint_top + PAINT_ROWS <= 195) return;
    char kit[17] = "-", from[17] = "BLANK";
    if (song_store_kit()) song_store_kit_name(song_store_kit(), kit);
    if (song_list_source >= 0 && groove_factory_name((unsigned)song_list_source)) strcpy(from, groove_factory_name((unsigned)song_list_source));
    kit[10] = from[10] = 0;
    knob(0, song_list_naming ? "CURSOR" : "SONG", song_list_naming ? "< >" : "", colors[0]);
    knob(1, song_list_naming ? "LETTER" : "NEW FROM", song_list_naming ? "A-Z" : from, colors[1]);
    knob(2, "KIT", kit, colors[2]);
    knob(3, "-", "", colors[3]);
    song_notice_expire();
    const char *hint = notice ? notice : song_list_naming ? "K1 K2 NAME  C4 OR SAVE DONE"
        : (previous_buttons & BUTTON(FX)) ? "C4 RENAME  D4 DUPLICATE  E4 DELETE" : "F3 LOAD G3 SAVE A3 AS B3 NEW C4 NAME";
    song_hint(hint);
}

static void song_list_open(void)
{
    song_port_clip_close(); /* a clip in the editor goes back first: songs may load */
    page = music_page = SONG_LIST;
    song_list_confirm = song_list_naming = 0;
    cache->shown_page = ~0U;
}
static const char *song_store_message(int r, const char *done)
{
    return !r ? done : r == SONG_STORE_PLAYING ? "STOP FIRST" : r == SONG_STORE_FULL ? "MEMORY FULL" : "COULDN'T, FLASH ERROR";
}
/* SAVE in the song view, and on the stock pages too: the song is the project. */
static void song_save_key(uint32_t now)
{
    notice = song_store_message(song_store_save(0), "SONG SAVED");
    notice_until = now + 1500U;
}
static int song_list_input(const struct fm1_controls *c, uint32_t *pressed, uint32_t key_down, uint32_t now)
{
    uint32_t down = key_down & ~blocked_keys;
    unsigned rows = song_list_rows(), id = rows ? song_list_id(song_list_at) : 0, recovery = song_store_recovery() && !song_list_at;
    unsigned action = 7;
    for (unsigned i = 0; i < 7; ++i) if (down & (1U << white_keys[i])) action = i;
    blocked_keys |= key_down;
    if (*pressed & BUTTON(HOME)) { *pressed &= ~BUTTON(HOME); page = music_page = SONG; cache->shown_page = ~0U; return 1; }
    if (*pressed & BUTTON(SAVE)) action = song_list_naming ? 4U : song_list_confirm ? song_list_confirm - 1U : 1U;
    *pressed &= ~(BUTTON(PLAY) | BUTTON(REC) | BUTTON(SAVE) | BUTTON(FX) | BUTTON(SCL) | BUTTON(ENV) | BUTTON(LFO) | BUTTON(ARP));
    if (*pressed & (BUTTON(EDIT) | BUTTON(GLO))) return 0;
    if (song_list_naming) { /* K1 the letter's place, K2 the letter */
        song_list_cursor = (unsigned)clamp((int)song_list_cursor + c->encoders[2] + c->encoders[0], 0, 11);
        if (c->encoders[3]) song_list_name[song_list_cursor] = edit_character(song_list_name[song_list_cursor], c->encoders[3]);
    } else {
        int move = c->encoders[2] + c->encoders[0];
        if (move && rows) { song_list_at = (unsigned)clamp((int)song_list_at + move, 0, (int)rows - 1); song_list_confirm = 0; }
        song_list_source = clamp(song_list_source + c->encoders[3], -1, GROOVE_FACTORY_PROJECTS - 1);
        if (c->encoders[4] && song_store_kits()) {
            unsigned k = 0;
            while (k < song_store_kits() && song_store_kit_id(k) != song_store_kit()) ++k;
            k = (unsigned)clamp((int)k + c->encoders[4], 0, (int)song_store_kits() - 1);
            notice = song_store_message(song_store_use_kit(song_store_kit_id(k)), "KIT CHANGED, PADS REBOUND");
            notice_until = now + 1500U;
        }
    }
    if (action > 6) return 1;
    int r = 0;
    const char *done = NULL;
    if (action == 0 && (recovery || id)) { /* load, asking first */
        if (song_list_confirm != 1) { song_list_confirm = 1; notice = "F3 AGAIN TO LOAD, UNSAVED CHANGES GO"; notice_until = now + 3000U; return 1; }
        r = recovery ? song_store_restore() : song_store_load(id);
        done = recovery ? "UNSAVED SONG RESTORED" : "SONG LOADED";
    } else if (action == 1) { r = song_store_save(0); done = "SONG SAVED"; }
    else if (action == 2) { r = song_store_save(1); done = "SAVED AS A NEW SONG"; }
    else if (action == 3) { r = song_store_new(song_list_source); done = "NEW SONG, NOT SAVED YET"; }
    else if (action == 4 && id && !recovery) {
        if (!song_list_naming) {
            char name[17];
            song_store_name(id, name);
            memset(song_list_name, ' ', sizeof song_list_name);
            for (unsigned i = 0; i < 12 && name[i]; ++i) song_list_name[i] = name[i];
            song_list_naming = 1;
            song_list_cursor = 0;
            return 1;
        }
        song_list_naming = 0;
        r = song_store_rename(id, song_list_name);
        done = "RENAMED";
    } else if (action == 5 && id && !recovery) { r = song_store_duplicate(id); done = "DUPLICATED"; }
    else if (action == 6 && (id || recovery)) {
        if (song_list_confirm != 7) { song_list_confirm = 7; notice = "E4 AGAIN TO DELETE"; notice_until = now + 3000U; return 1; }
        if (recovery) song_store_skip_recovery();
        else r = song_store_delete(id);
        done = "DELETED";
    } else return 1;
    song_list_confirm = 0;
    notice = song_store_message(r, done);
    notice_until = now + 1500U;
    rows = song_list_rows();
    if (song_list_at >= rows) song_list_at = rows ? rows - 1U : 0;
    return 1;
}
