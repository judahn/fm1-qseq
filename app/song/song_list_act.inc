/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* song-mode: the song list's actions, included by song_list.inc. The hint
 * that says what YES will do and to what, and YES itself. Loading or a new
 * song asks first when unsaved changes would go, deleting always asks, and
 * a second YES goes ahead. On the recovery row only load (it restores) and
 * delete (it drops the copy) work. */

static void song_list_trim(char name[17])
{
    name[16] = 0;
    for (unsigned n = (unsigned)strlen(name); n && name[n - 1] == ' ';) name[--n] = 0;
}
static void song_list_from_name(char out[17])
{
    const char *name = song_list_source >= 0 ? groove_factory_name((unsigned)song_list_source) : NULL;
    strcpy(out, name ? name : "BLANK");
}
/* What K2's action works on, with its name: 0 nothing (no songs), 1 the
 * song K1 picked, 2 the recovery copy, 3 the song being worked on, or for
 * a new song what it starts from. */
static unsigned song_list_target(char name[17])
{
    memset(name, 0, 17);
    if (song_list_act == LIST_NEW) { song_list_from_name(name); return 3; }
    if (song_list_act == LIST_SAVE || song_list_act == LIST_SAVE_AS) {
        memcpy(name, song_port_song()->name, 12);
        song_list_trim(name);
        if (!name[0]) strcpy(name, "THIS SONG");
        return 3;
    }
    if (!song_list_rows()) return 0;
    if (song_store_recovery() && !song_list_at) { strcpy(name, "UNSAVED SONG"); return 2; }
    song_store_name(song_list_id(song_list_at), name);
    song_list_trim(name);
    return 1;
}
/* The hint: "OCT+ LOAD VERSE   OCT- BACK", the name cut to fit the line. */
static const char *song_list_hint(void)
{
    static const char *const verbs[LIST_ACTIONS] = {"LOAD ", "SAVE ", "SAVE ", "NEW FROM ", "RENAME ", "DUPLICATE ", "DELETE "};
    static char line[40];
    char name[17];
    unsigned on = song_list_target(name), a = song_list_act;
    const char *no = song_asking() ? "   OCT- CANCEL" : "   OCT- BACK", *after = a == LIST_SAVE_AS ? " AS NEW" : "";
    const char *verb = on == 2 && a == LIST_LOAD ? "RESTORE " : verbs[a];
    if (!on) strcpy(line, "NO SONGS YET");
    else if (on == 2 && a != LIST_LOAD && a != LIST_DELETE) strcpy(line, "LOAD OR DELETE ONLY HERE");
    else {
        unsigned room = 33U - (unsigned)(strlen(verb) + strlen(after) + strlen(no)); /* 38 on the line, "OCT+ " first */
        name[room < 16U ? room : 16U] = 0;
        song_list_trim(name);
        strcpy(line, "OCT+ "); strcat(line, verb); strcat(line, name); strcat(line, after);
    }
    strcat(line, no);
    return line;
}
static void song_list_done(int r, const char *done, uint32_t now)
{
    notice = song_store_message(r, done);
    notice_until = now + 1500U;
    unsigned rows = song_list_rows();
    if (song_list_at >= rows) song_list_at = rows ? rows - 1U : 0;
}
/* YES: K2's action on K1's song. A question already open means this is
 * the second YES. Flash is written only while stopped. */
static void song_list_yes(uint32_t now)
{
    char name[17], ask[SONG_ASK_CHARS + 1];
    unsigned on = song_list_target(name), a = song_list_act, asked = song_asking();
    unsigned id = on == 1 ? song_list_id(song_list_at) : 0;
    song_ask_close();
    if (song_port_player()->playing) { song_list_done(SONG_STORE_PLAYING, NULL, now); return; }
    if (a == LIST_SAVE || a == LIST_SAVE_AS) {
        song_list_done(song_store_save(a == LIST_SAVE_AS), a == LIST_SAVE ? "SONG SAVED" : "SAVED AS A NEW SONG", now);
        return;
    }
    if (!on || (on == 2 && a != LIST_LOAD && a != LIST_DELETE)) {
        notice = on ? "NOT FOR THE UNSAVED COPY" : "NO SONGS YET";
        notice_until = now + 1500U;
        return;
    }
    strcpy(ask, a == LIST_NEW ? "NEW SONG FROM " : a == LIST_DELETE ? "DELETE " : on == 2 ? "RESTORE " : "LOAD ");
    strcat(ask, name);
    strcat(ask, "?");
    if (a == LIST_LOAD || a == LIST_NEW) {
        if (!asked && song_store_unsaved()) { song_ask(ask, "UNSAVED CHANGES GO", 1); return; }
        int r = a == LIST_NEW ? song_store_new(song_list_source) : on == 2 ? song_store_restore() : song_store_load(id);
        song_list_done(r, a == LIST_NEW ? "NEW SONG, NOT SAVED YET" : on == 2 ? "UNSAVED SONG RESTORED" : "SONG LOADED", now);
    } else if (a == LIST_DELETE) {
        if (!asked) { song_ask(ask, NULL, 1); return; }
        int r = 0;
        if (on == 2) song_store_skip_recovery();
        else r = song_store_delete(id);
        song_list_done(r, "DELETED", now);
    } else if (a == LIST_DUPLICATE) song_list_done(song_store_duplicate(id), "DUPLICATED", now);
    else { /* rename: K1 the letter's place, K2 the letter, until YES or NO */
        memset(song_list_name, ' ', sizeof song_list_name);
        for (unsigned i = 0; i < 12 && name[i]; ++i) song_list_name[i] = name[i];
        song_list_cursor = 0;
        song_name_open();
    }
}
static void song_list_renamed(uint32_t now)
{
    song_ask_close();
    song_list_done(song_store_rename(song_list_id(song_list_at), song_list_name), "RENAMED", now);
}
