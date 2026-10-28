/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* song-mode: the clip grid (design 8f), included by song_view.inc. SEQ flips
 * between it and the song view. 8 tracks across, 16 rows in two pages of 8,
 * and the SONG row on top. SELECT picks the track, K1 the row, K4 the launch
 * quantize. The keys launch here: F3 to F4 the track's cell in the cursor
 * row (an empty cell stops the track, the SONG row hands it back to the
 * song), G4 to G5 rows 1 to 8 of the page. SCL opens the clip at the cursor,
 * ENV names the row, and SHIFT plus F3, G3, A3 or B3, and E4 copy, cut,
 * paste and delete the cell. REC records a performance (song_perf.c). */
#include "clips.h"

enum { GRID_X = 46, GRID_W = 24, GRID_SONG = 34, GRID_Y = 52, GRID_H = 16, GRID_EMPTY = 0x2104 };
static uint8_t grid_row = 1, grid_page, grid_naming, grid_letter; /* row 0 is SONG */

/* A cell: 0 empty, 1 a clip, 2 playing, 3 waiting to (blinks). The SONG
 * row's cells are the song for each track. */
static unsigned grid_state(const struct song_player *p, unsigned t, unsigned row)
{
    if (p->waiting[t] == row) return 3;
    if (p->from[t] == row) return 2;
    return row == SONG_FROM_SONG || song_port_song()->clip_length[song_cell(t, row)];
}
static void grid_length(uint32_t clocks, char out[12])
{
    const struct song *s = song_port_song();
    uint32_t beat = SONG_WHOLE / s->meter[0].unit, bar = beat * s->meter[0].beats, n = clocks % bar ? clocks / beat : clocks / bar;
    if (!clocks) { strcpy(out, "EMPTY"); return; }
    decimal(out, (int)n, 1);
    strcat(out, clocks % bar ? " BEAT" : " BAR");
    if (n > 1) strcat(out, "S");
}
static void song_grid_draw(void)
{
    struct song *s = song_port_song();
    const struct song_player *p = song_port_player();
    if (!paint_rendering) song_now_read(s);
    song_header(s);
    uint32_t now = song_player_transport(p);
    unsigned blink = frame_time / 200U & 1U;
    for (unsigned r = 0; r <= 8; ++r) {
        unsigned y = r ? GRID_Y + (r - 1U) * GRID_H : GRID_SONG, row = r ? grid_page * 8U + r - 1U : SONG_FROM_SONG;
        unsigned here = r ? grid_row == row + 1U : !grid_row;
        if (paint_rendering && (y + GRID_H <= paint_top || y >= paint_top + PAINT_ROWS)) continue;
        uint32_t sig = signature(row << 1 | here, r ? s->row_name[row] : "SONG", r ? SONG_MARKER_NAME : 4);
        if (sig != cache->tile_cache[r]) { /* the row's number and name */
            cache->tile_cache[r] = sig;
            paint_fill(0, y - 1, GRID_X - 1, GRID_H, BACK);
            char name[SONG_MARKER_NAME + 1] = "SONG";
            if (r) { memcpy(name, s->row_name[row], 4); name[4] = 0; number(4, y + 3, (int)row + 1, 2, 1, here ? AMBER : DIM, BACK); }
            text(r ? 20 : 4, y + 3, name, 1, here ? AMBER : TEXT, BACK);
        }
        for (unsigned t = 0; t < SONG_TRACKS; ++t) {
            unsigned state = grid_state(p, t, row), cursor = here && t == track, x = GRID_X + t * GRID_W, width = 0;
            uint32_t length = r ? s->clip_length[song_cell(t, row)] : 0;
            if (state == 2 && length && p->playing) /* how far the clip has got */
                width = (unsigned)((now > p->base[t] ? (now - p->base[t]) % length : 0) * (GRID_W - 2U) / length);
            if (state == 3 && blink) state = 1;
            sig = state | cursor << 2 | width << 3 | (uint32_t)p->playing << 9 | (uint32_t)row << 10;
            if (sig == cache->cell_cache[t][r]) continue;
            cache->cell_cache[t][r] = sig;
            paint_fill(x - 1U, y - 1U, GRID_W, GRID_H, BACK);
            if (!paint_rendering) continue;
            uint16_t lit = colors[t], dim = (uint16_t)(colors[t] >> 1 & 0x7befU);
            paint_fill(x, y, GRID_W - 2U, GRID_H - 2U, !state ? GRID_EMPTY : state == 1 || !p->playing ? dim : lit);
            if (!r) number(x + 8, y + 3, (int)t + 1, 1, 1, BACK, state == 1 || !p->playing ? dim : lit);
            if (width) paint_fill(x, y + GRID_H - 4U, width, 2, TEXT);
            if (cursor) frame(x - 1U, y - 1U, GRID_W, GRID_H, TEXT);
        }
    }
    if (paint_rendering && paint_top + PAINT_ROWS <= 178) return;
    if (cache->seq_cache != 0x9a1dU) { /* the function buttons */
        cache->seq_cache = 0x9a1dU;
        paint_fill(0, 180, 240, 12, BACK);
        text(8, 181, "SCL OPEN  ENV NAME  SHIFT+SEQ SONG", 1, DIM, BACK);
    }
    if (paint_rendering && paint_top + PAINT_ROWS <= 195) return;
    char line[12], value[12] = "SONG";
    unsigned row = grid_row ? grid_row - 1U : 0;
    if (grid_naming) { /* the name in the footer, the letter being set shown even when blank */
        char name[SONG_MARKER_NAME + 1] = {0};
        memcpy(name, s->row_name[row], SONG_MARKER_NAME);
        for (unsigned i = 0; i < grid_letter; ++i) if (!name[i]) name[i] = ' ';
        if (!name[grid_letter] || name[grid_letter] == ' ') name[grid_letter] = '_';
        decimal(value, grid_letter + 1, 1);
        knob(0, "LETTER", value, AMBER);
        knob(1, "NAME", name, AMBER);
    } else {
        if (grid_row) decimal(value, (int)grid_row, 1);
        knob(0, "ROW", value, colors[0]);
        grid_length(grid_row ? s->clip_length[song_cell(track, row)] : 0, line);
        knob(1, "CLIP", grid_row ? line : "-", colors[1]);
    }
    knob(2, "-", "", colors[2]);
    knob(3, "LAUNCH Q", song_launch_names[s->launch_q % 5U], colors[3]);
    song_notice_expire();
    const char *hint = notice ? notice : grid_naming ? "ENV OR SAVE DONE" : song_perf_state() == 2 ? "REC: LAUNCHES BECOME BLOCKS"
        : song_perf_state() ? "REC ARMED: LAUNCH OR PLAY" : (previous_buttons & BUTTON(FX))
        ? "F3 COPY G3 CUT A3 PASTE E4 DELETE" : grid_page ? "F3-F4 LAUNCH  G4-G5 ROWS 9-16" : "F3-F4 LAUNCH  G4-G5 ROWS 1-8";
    song_hint(hint);
}

/* SHIFT plus a lower white key on the cell at the cursor. */
static void grid_action(unsigned which, uint32_t now)
{
    struct song *s = song_port_song();
    struct song_clip *c = song_port_clip();
    unsigned cell = song_cell(track, grid_row - 1U);
    uint32_t state = song_port_lock();
    int r = which == 0 ? song_clip_copy(s, c, cell) : which == 1 ? song_clip_cut(s, c, cell)
        : which <= 3 ? (c->length ? song_clip_paste(s, c, cell) : -2) : song_clip_delete(s, cell);
    song_port_unlock(state);
    static const char *const done[4] = {"CLIP COPIED", "CLIP CUT", "PASTED", "PASTED"};
    notice = r == -2 ? "COPY SOMETHING FIRST" : r ? which >= 2 && which <= 3 ? "DOESN'T FIT IN A CLIP" : "NO CLIP THERE"
        : which < 4 ? done[which] : "CLIP DELETED";
    notice_until = now + 1500U;
}
static void grid_name_done(void)
{
    char *name = song_port_song()->row_name[grid_row - 1U];
    for (unsigned i = SONG_MARKER_NAME; i-- > 0 && (name[i] == ' ' || !name[i]);) name[i] = 0;
    grid_naming = 0;
}
/* Launch a track to a row, or stop it when the cell is empty. */
static void grid_launch(unsigned t, unsigned row)
{
    unsigned to = row == SONG_FROM_SONG || song_port_song()->clip_length[song_cell(t, row)] ? row : SONG_FROM_STOP;
    song_port_launch(t, to);
}
static int song_grid_input(const struct fm1_controls *c, uint32_t *pressed, uint32_t key_down, uint32_t now)
{
    struct song *s = song_port_song();
    unsigned shift = (c->buttons & BUTTON(FX)) != 0, launched = 0;
    uint32_t down = key_down & ~blocked_keys;
    blocked_keys |= key_down; /* the keys launch here, never play */
    if (*pressed & BUTTON(SAVE)) {
        *pressed &= ~BUTTON(SAVE);
        if (grid_naming) grid_name_done();
        else if (shift) { song_list_open(); return 1; }
        else song_save_key(now);
    }
    if (*pressed & BUTTON(PLAY)) { if (song_port_player()->playing) song_port_stop(); else song_start(now); }
    if (*pressed & BUTTON(REC)) { /* a performance: armed, then every launch is a block in the song */
        if (song_perf_state()) { song_perf_end(); notice = "TAKE ENDED"; }
        else notice = song_perf_arm() ? "NOT NOW" : "REC: LAUNCH OR PLAY TO START";
        notice_until = now + 1500U;
    }
    if ((*pressed & BUTTON(ENV)) && grid_row) {
        if (grid_naming) grid_name_done();
        else { grid_naming = 1; grid_letter = 0; }
    }
    if ((*pressed & BUTTON(SCL)) && grid_row && !grid_naming) {
        if (song_port_clip_open(song_cell(track, grid_row - 1U))) return 1;
        page = music_page = SONG;
        cache->shown_page = ~0U;
        return 1;
    }
    *pressed &= ~(BUTTON(PLAY) | BUTTON(REC) | BUTTON(FX) | BUTTON(SCL) | BUTTON(ENV) | BUTTON(LFO) | BUTTON(ARP));
    if (*pressed & (BUTTON(HOME) | BUTTON(EDIT) | BUTTON(GLO))) return 0;
    if (grid_naming) { /* K1 the letter's place, K2 the letter */
        char *name = s->row_name[grid_row - 1U];
        grid_letter = (uint8_t)clamp(grid_letter + c->encoders[2], 0, SONG_MARKER_NAME - 1);
        if (c->encoders[3]) {
            for (unsigned i = 0; i < grid_letter; ++i) if (!name[i]) name[i] = ' ';
            name[grid_letter] = edit_character(name[grid_letter] ? name[grid_letter] : ' ', c->encoders[3]);
        }
        return 1;
    }
    select_track(c->encoders[0]);
    grid_row = (uint8_t)clamp(grid_row + c->encoders[2], 0, SONG_GRID_ROWS);
    if (grid_row) grid_page = (uint8_t)((grid_row - 1U) / 8U);
    s->launch_q = (uint8_t)clamp(s->launch_q + c->encoders[5], 0, 4);
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
