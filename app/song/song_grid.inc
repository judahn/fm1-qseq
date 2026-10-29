/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* song-mode: the clip grid (design 8f), included by song_view.inc. SEQ flips
 * between it and the song view. 8 tracks across, 16 rows in two pages of 8,
 * and the SONG row on top. SELECT picks the track, K1 the row, K2 the REC
 * length, K4 the launch quantize. The keys launch here: F3 to F4 the
 * track's cell in the cursor row (an empty cell stops the track, the SONG
 * row hands it back to the song), G4 to G5 rows 1 to 8 of the page. SCL
 * opens the clip at the cursor, ENV names the row (YES keeps the name, NO
 * leaves the old one), and SHIFT plus F3, G3, A3 or B3, and E4 copy, cut,
 * paste and delete the cell. REC records into the cell at the cursor
 * (song_grid_take.c), and while it is armed or recording the keys play its
 * track instead of launching. SHIFT+REC records a performance
 * (song_perf.c). Drawing is in song_grid_draw.inc. */
#include "clips.h"

enum { GRID_X = 46, GRID_W = 24, GRID_SONG = 34, GRID_Y = 52, GRID_H = 16, GRID_EMPTY = 0x2104 };
static uint8_t grid_row = 1, grid_page, grid_letter; /* row 0 is SONG */
static char grid_name[SONG_MARKER_NAME]; /* the row's name while naming, kept on YES */

#include "song_grid_draw.inc"

/* SHIFT plus a lower white key on the cell at the cursor. */
static void grid_action(unsigned which, uint32_t now)
{
    struct song *s = song_port_song();
    struct song_clip *c = song_port_clip();
    unsigned cell = song_cell(track, grid_row - 1U);
    int r = which == 0 ? song_clip_copy(s, c, cell) : which == 1 ? song_clip_cut(s, c, cell)
        : which <= 3 ? (c->length ? song_clip_paste(s, c, cell) : -2) : song_clip_delete(s, cell);
    static const char *const done[4] = {"CLIP COPIED", "CLIP CUT", "PASTED", "PASTED"};
    notice = r == -2 ? "COPY SOMETHING FIRST" : r ? which >= 2 && which <= 3 ? "DOESN'T FIT IN A CLIP" : "NO CLIP THERE"
        : which < 4 ? done[which] : "CLIP DELETED";
    notice_until = now + 1500U;
}
/* YES: the name goes to the row, without trailing blanks. */
static void grid_name_done(void)
{
    char *name = song_port_song()->row_name[grid_row - 1U];
    memcpy(name, grid_name, SONG_MARKER_NAME);
    for (unsigned i = SONG_MARKER_NAME; i-- > 0 && (name[i] == ' ' || !name[i]);) name[i] = 0;
    song_ask_close();
}
/* Launch a track to a row, or stop it when the cell is empty. */
static void grid_launch(unsigned t, unsigned row)
{
    unsigned to = row == SONG_FROM_SONG || song_port_song()->clip_length[song_cell(t, row)] ? row : SONG_FROM_STOP;
    song_port_launch(t, to);
}
/* REC: a performance running or armed ends, a cell's take goes on to its
 * next step (song_grid_rec_key), SHIFT+REC arms a performance, and REC
 * records into the cell at the cursor. */
static void grid_rec(unsigned shift, uint32_t now)
{
    notice_until = now + 1500U;
    if (song_perf_state()) { song_perf_end(); notice = "TAKE ENDED"; }
    else if (song_grid_rec_state()) song_grid_rec_key();
    else if (shift) notice = song_perf_arm() ? "NOT NOW" : "REC: LAUNCH OR PLAY TO START";
    else if (!grid_row) notice = "PICK A ROW TO RECORD INTO";
    else if (song_grid_rec(song_cell(track, grid_row - 1U), grid_bars())) notice = "NOT NOW";
}
static int song_grid_input(const struct fm1_controls *c, uint32_t *pressed, uint32_t key_down, uint32_t now)
{
    struct song *s = song_port_song();
    unsigned shift = (c->buttons & BUTTON(FX)) != 0, launched = 0, naming = song_naming(), taking = song_grid_rec_state() != 0;
    int answer = taking ? 0 : song_yes_no(pressed); /* only naming takes YES or NO here */
    uint32_t down = key_down & ~blocked_keys;
    if (!taking || shift) blocked_keys |= key_down; /* the keys launch here, and play only into a take */
    if (*pressed & BUTTON(SAVE)) { /* the song list as SAVE comes up, nothing while naming */
        *pressed &= ~BUTTON(SAVE);
        if (!naming) song_save_press();
    }
    if (*pressed & BUTTON(PLAY)) { if (song_port_player()->playing) song_port_stop(); else song_start(now); }
    if (*pressed & BUTTON(REC)) grid_rec(shift, now);
    if ((*pressed & BUTTON(ENV)) && grid_row && !naming && !taking) { /* name the row: YES keeps it, NO leaves the old one */
        memcpy(grid_name, s->row_name[grid_row - 1U], SONG_MARKER_NAME);
        grid_letter = 0;
        song_name_open();
        naming = 1;
    }
    if ((*pressed & BUTTON(SCL)) && grid_row && !naming && !taking) {
        if (song_port_clip_open(song_cell(track, grid_row - 1U))) return 1;
        page = music_page = SONG;
        cache->shown_page = ~0U;
        return 1;
    }
    *pressed &= ~(BUTTON(PLAY) | BUTTON(REC) | BUTTON(FX) | BUTTON(SCL) | BUTTON(ENV) | BUTTON(LFO) | BUTTON(ARP));
    if (*pressed & (BUTTON(HOME) | BUTTON(EDIT) | BUTTON(GLO))) return 0;
    if (naming) { /* K1 the letter's place, K2 the letter */
        grid_letter = (uint8_t)clamp(grid_letter + c->encoders[2], 0, SONG_MARKER_NAME - 1);
        if (c->encoders[3]) {
            for (unsigned i = 0; i < grid_letter; ++i) if (!grid_name[i]) grid_name[i] = ' ';
            grid_name[grid_letter] = edit_character(grid_name[grid_letter] ? grid_name[grid_letter] : ' ', c->encoders[3]);
        }
        if (answer > 0) grid_name_done();
        else if (answer < 0) song_ask_close();
        return 1;
    }
    grid_row = (uint8_t)clamp(grid_row + c->encoders[2], 0, SONG_GRID_ROWS);
    if (grid_row) grid_page = (uint8_t)((grid_row - 1U) / 8U);
    s->launch_q = (uint8_t)clamp(s->launch_q + c->encoders[5], 0, 4);
    if (taking) { /* the track stays, and the keys play it at the keyboard's octave */
        octave = clamp(octave + !!(*pressed & BUTTON(OCT_UP)) - !!(*pressed & BUTTON(OCT_DOWN)), -2, 2);
        if (shift && down) { notice = "STOP RECORDING FIRST"; notice_until = now + 1500U; }
        return 1;
    }
    select_track(c->encoders[0]);
    grid_rec_len = (uint8_t)clamp(grid_rec_len + c->encoders[3], 0, 5);
    if (shift) {
        static const uint8_t keys[5] = {0, 1, 2, 3, 6}; /* F3 G3 A3 B3 E4 */
        for (unsigned i = 0; i < 5 && grid_row; ++i)
            if (down & (1U << white_keys[keys[i]])) grid_action(keys[i], now);
        return 1;
    }
    for (unsigned t = 0; t < SONG_TRACKS; ++t)
        if (down & (1U << white_keys[t])) { grid_launch(t, grid_row ? grid_row - 1U : SONG_FROM_SONG); launched = 1; }
    for (unsigned i = 0; i < 8; ++i) /* a scene: the row on every track */
        if (down & (1U << white_keys[8 + i])) {
            for (unsigned t = 0; t < SONG_TRACKS; ++t) grid_launch(t, grid_page * 8U + i);
            launched = 1;
        }
    if (launched && !song_port_player()->playing) song_start(now);
    return 1;
}
