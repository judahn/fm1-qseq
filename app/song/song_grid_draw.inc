/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* song-mode: drawing the clip grid, included by song_grid.inc. Each cell is
 * empty, a clip, playing with its progress, or blinking while a launch
 * waits. A cell being recorded into (0.3.0) blinks red while armed and is
 * red while recording, with its progress through its length (a FREE take
 * with no end yet shows the bar it is in). The footer has the row, K2's
 * REC LENGTH, the cell's length and K4's launch quantize. */

enum { GRID_REC = 0xf800 }; /* the REC red of the header and the REC chooser */
static const char *const grid_rec_names[6] = {"1 BAR", "2 BARS", "4 BARS", "8 BARS", "16 BARS", "FREE"};
static uint8_t grid_rec_len = 2; /* 4 bars, kept while the FM-1 is on */
static unsigned grid_bars(void) { return grid_rec_len < 5 ? 1U << grid_rec_len : 0U; }

/* A cell: 0 empty, 1 a clip, 2 playing, 3 waiting to (blinks), 4 armed to
 * record (blinks), 5 recording. The SONG row's cells are the song for each
 * track. */
static unsigned grid_state(const struct song_player *p, unsigned t, unsigned row)
{
    unsigned rec = song_grid_rec_state();
    if (rec && song_grid_rec_cell() == song_cell(t, row)) return rec & 2U ? 5 : 4;
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
/* How far a cell's clip has got, in pixels across the cell. */
static unsigned grid_progress(const struct song_player *p, unsigned t, unsigned row, unsigned state, uint32_t now)
{
    uint32_t length = song_port_song()->clip_length[song_cell(t, row)];
    if (state == 5) { /* recording: the clip is open behind the grid, its clock the player's */
        uint32_t at = song_player_clock(p), bar = song_bar_clock(song_port_song(), 1);
        if (song_grid_rec_state() & 4U) { length = bar; at %= bar; }
        return length ? (unsigned)((uint64_t)at * (GRID_W - 2U) / length) : 0U;
    }
    if (state != 2 || !length || !p->playing) return 0;
    return (unsigned)((now > p->base[t] ? (now - p->base[t]) % length : 0) * (GRID_W - 2U) / length);
}
static const char *grid_hint(void)
{
    static char line[40];
    unsigned rec = song_grid_rec_state();
    if (rec & 2U) {
        if (!(rec & 8U)) return "OVERDUB  PLAY OR REC ENDS";
        if (rec & 4U) return "REC FREE  REC ENDS AT THE NEXT BAR";
        if (!grid_bars()) return "REC ENDS AT THE BAR LINE";
        strcpy(line, "REC ");
        strcat(line, grid_rec_names[grid_rec_len]);
        strcat(line, "  PLAY OR REC ENDS");
        return line;
    }
    if (song_port_recorder()->active) return "COUNT IN  PLAY OR REC CALLS IT OFF";
    return song_port_clip_armed() ? "OVERDUB FROM THE CLIP'S NEXT PASS" : "REC ARMED  STARTS AT THE LAUNCH";
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
        const char *named = r && here && song_naming() ? grid_name : r ? s->row_name[row] : "SONG";
        uint32_t sig = signature(row << 1 | here, named, r ? SONG_MARKER_NAME : 4);
        if (sig != cache->tile_cache[r]) { /* the row's number and name */
            cache->tile_cache[r] = sig;
            paint_fill(0, y - 1, GRID_X - 1, GRID_H, BACK);
            char name[SONG_MARKER_NAME + 1] = "SONG";
            if (r) { memcpy(name, named, 4); name[4] = 0; number(4, y + 3, (int)row + 1, 2, 1, here ? AMBER : DIM, BACK); }
            text(r ? 20 : 4, y + 3, name, 1, here ? AMBER : TEXT, BACK);
        }
        for (unsigned t = 0; t < SONG_TRACKS; ++t) {
            unsigned state = grid_state(p, t, row), cursor = here && t == track, x = GRID_X + t * GRID_W;
            unsigned width = r ? grid_progress(p, t, row, state, now) : 0;
            if (state == 3 && blink) state = 1;
            if (state == 4 && blink) state = song_grid_rec_state() & 8U ? 0 : 1; /* a new clip blinks from empty */
            sig = state | cursor << 3 | width << 4 | (uint32_t)p->playing << 10 | (uint32_t)row << 11;
            if (sig == cache->cell_cache[t][r]) continue;
            cache->cell_cache[t][r] = sig;
            paint_fill(x - 1U, y - 1U, GRID_W, GRID_H, BACK);
            if (!paint_rendering) continue;
            uint16_t lit = colors[t], dim = (uint16_t)(colors[t] >> 1 & 0x7befU);
            paint_fill(x, y, GRID_W - 2U, GRID_H - 2U, state >= 4 ? GRID_REC : !state ? GRID_EMPTY : state == 1 || !p->playing ? dim : lit);
            if (!r) number(x + 8, y + 3, (int)t + 1, 1, 1, BACK, state == 1 || !p->playing ? dim : lit);
            if (width) paint_fill(x, y + GRID_H - 4U, width, 2, TEXT);
            if (cursor) frame(x - 1U, y - 1U, GRID_W, GRID_H, TEXT);
        }
    }
    if (paint_rendering && paint_top + PAINT_ROWS <= 178) return;
    if (cache->seq_cache != 0x9a1eU) { /* the function buttons */
        cache->seq_cache = 0x9a1eU;
        paint_fill(0, 180, 240, 12, BACK);
        text(8, 181, "SEL OPEN  ENV NAME  SHIFT+REC PERFORM", 1, DIM, BACK);
    }
    if (paint_rendering && paint_top + PAINT_ROWS <= 195) return;
    char line[12], value[12] = "SONG";
    unsigned row = grid_row ? grid_row - 1U : 0;
    if (song_naming()) { /* the name in the footer, the letter being set shown even when blank */
        char name[SONG_MARKER_NAME + 1] = {0};
        memcpy(name, grid_name, SONG_MARKER_NAME);
        for (unsigned i = 0; i < grid_letter; ++i) if (!name[i]) name[i] = ' ';
        if (!name[grid_letter] || name[grid_letter] == ' ') name[grid_letter] = '_';
        decimal(value, grid_letter + 1, 1);
        knob(0, "LETTER", value, AMBER);
        knob(1, "NAME", name, AMBER);
    } else {
        if (grid_row) decimal(value, (int)grid_row, 1);
        knob(0, "ROW", value, colors[0]);
        knob(1, "REC LEN", grid_rec_names[grid_rec_len], song_grid_rec_state() ? GRID_REC : colors[1]);
    }
    grid_length(grid_row ? s->clip_length[song_cell(track, row)] : 0, line);
    knob(2, "CLIP", grid_row ? line : "-", colors[2]);
    knob(3, "LAUNCH Q", song_launch_names[s->launch_q % 5U], colors[3]);
    song_notice_expire();
    const char *hint = notice ? notice : song_naming() ? SONG_NAMING_HINT : song_grid_rec_state() ? grid_hint()
        : song_perf_state() == 2 ? "REC: LAUNCHES BECOME BLOCKS" : song_perf_state() ? "REC ARMED: LAUNCH OR PLAY"
        : (previous_buttons & BUTTON(FX)) ? "F3 COPY G3 CUT A3 PASTE E4 DELETE"
        : grid_page ? "F3-F4 LAUNCH  G4-G5 ROWS 9-16" : "F3-F4 LAUNCH  G4-G5 ROWS 1-8";
    song_hint(hint);
}

