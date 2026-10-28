/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* song-mode: the hardware diagnostics page, in the hardware test build
 * (SONG_HW_TEST), where SEQ opens it. Control loop timing, note lateness
 * per delivery method, audio health and memory, each as one line of text. */
#include "diag.h"

static void diag_num(char *out, const char *label, uint32_t value)
{
    char n[12];
    decimal(n, (int)(value > 99999999U ? 99999999U : value), 1);
    strcat(out, label);
    strcat(out, n);
}
static void diag_tenths(char *out, const char *label, uint32_t x10) /* one decimal place */
{
    char n[12];
    diag_num(out, label, x10 / 10U);
    strcat(out, ".");
    decimal(n, (int)(x10 % 10U), 1);
    strcat(out, n);
}
/* A memory probe's result in KB. At the top of the search it shows the top,
 * with "+": that much or more. Returns 1 there. */
static int diag_kb(char *out, const char *label, uint32_t bytes)
{
    int top = bytes + 16U >= song_diag_top();
    diag_tenths(out, label, (top ? song_diag_top() : bytes) * 10U / 1024U);
    if (top) strcat(out, "+");
    return top;
}
/* One text line per row. Rows repaint only when their text changes. */
static void diag_row(unsigned row, unsigned y, const char *line, uint16_t color)
{
    if (paint_rendering && (y >= paint_top + PAINT_ROWS || y + 9 <= paint_top)) return;
    uint32_t sig = signature(color, line, (unsigned)strlen(line));
    uint32_t *slot = row < 8 ? &cache->row_cache[row] : &cache->cell_cache[0][row - 8];
    if (sig == *slot) return;
    *slot = sig;
    paint_fill(0, y, 240, 9, BACK);
    text(6, y + 1, line, 1, color, BACK);
}
static void song_diag_draw(void)
{
    static const char *const methods[2] = {"A: CONTROL LOOP", "B: A + WAKE EACH BLOCK"};
    const struct song_diag *d = &song_diag;
    unsigned method = song_port_method();
    char line[48];
    if ((!paint_rendering || paint_top < 30) && cache->header_cache != 0xd1a9U) {
        cache->header_cache = 0xd1a9U;
        paint_fill(0, 0, 240, 27, BACK);
        text(8, 7, "HW DIAGNOSTICS", 2, ACCENT, BACK);
    }
    uint32_t us[3], blocks[3];
    song_diag_window(us, blocks);
    line[0] = 0; diag_tenths(line, "LOOP MS ", (us[0]) / 100U); diag_tenths(line, " / ", (us[1]) / 100U); diag_tenths(line, " / ", (us[2]) / 100U);
    diag_row(0, 32, line, TEXT);
    line[0] = 0; diag_tenths(line, "BLOCKS ", blocks[0]); diag_tenths(line, " / ", blocks[1]); diag_tenths(line, " / ", blocks[2]);
    diag_row(1, 42, line, TEXT);
    line[0] = 0; diag_tenths(line, "RUN MAX ", (d->run_max_us) / 100U); diag_num(line, " MS  ", d->run_max_blocks); strcat(line, " BLK");
    diag_row(2, 52, line, DIM);
    line[0] = 0;
    diag_num(line, "POLLS ", d->polls); diag_tenths(line, "  TIMER CHECK ", d->tick_khz / 100U); strcat(line, " MHZ");
    diag_row(3, 62, line, DIM);
    diag_row(4, 74, methods[method & 1U], GREEN);
    line[0] = 0; diag_num(line, "LATE 0:", d->late[0]); diag_num(line, " 1:", d->late[1]);
    diag_num(line, " 2:", d->late[2]); diag_num(line, " 3+:", d->late[3]);
    diag_row(5, 84, line, TEXT);
    line[0] = 0; diag_num(line, "NOTES ", d->notes); diag_num(line, "  MAX LATE ", d->late_max); strcat(line, " BLK");
    diag_row(6, 94, line, DIM);
    struct fm1_audio_stats dma; struct fm1_render_stats render; struct fm1_engine_stats engine;
    fm1_audio_stats_read(&dma); groove_audio_stats(&render); fm1_engine_stats_read(&engine);
    line[0] = 0; diag_num(line, "OVERRUN ", dma.overruns); diag_num(line, " SHORT ", dma.short_intervals);
    diag_num(line, " PEAK ", dma.peak_ticks);
    diag_row(7, 106, line, dma.overruns ? AMBER : TEXT);
    line[0] = 0; diag_num(line, "RENDER ", render.render_ticks); diag_num(line, " PEAK ", render.peak_ticks);
    diag_num(line, " OF ", render.deadline_ticks);
    diag_row(8, 116, line, TEXT);
    line[0] = 0; diag_num(line, "ADAPT ", render.adapted); diag_num(line, " OVERLOAD ", render.overloads);
    diag_num(line, " UNDER ", engine.underruns);
    diag_row(9, 126, line, render.overloads || engine.underruns ? AMBER : TEXT);
    line[0] = 0; diag_num(line, "MIDI DROP ", engine.midi_dropped); diag_num(line, "  BLOCKS ", dma.blocks);
    diag_row(10, 136, line, DIM);
    /* Memory: what the probe can vouch for, and nothing when malloc gave a
     * block outside the heap. */
    line[0] = 0;
    if (d->outside) strcpy(line, "MALLOC GAVE A BLOCK OUTSIDE THE HEAP");
    else if (!d->largest_free) strcpy(line, "LARGEST FREE: ENV TO PROBE");
    else {
        int top = diag_kb(line, "LARGEST FREE ", d->largest_free);
        if (top) line[strlen(line) - 1] = 0; /* the words instead of "+" */
        strcat(line, top ? " KB OR MORE" : " KB");
    }
    diag_row(11, 148, line, d->outside ? AMBER : TEXT);
    line[0] = 0;
    if (d->heap) { diag_tenths(line, "HEAP ", d->heap * 10U / 1024U); strcat(line, " KB  "); }
    if (d->heaviest >= 0 && !d->outside) strcat(line, "ENGINES (+ OR MORE)");
    diag_row(12, 158, line, DIM);
    for (unsigned half = 0; half < 2; ++half) {
        static const char *const names[6] = {"DX7 ", " 303 ", " 808 ", "PSG ", " PLK ", " SMP "};
        line[0] = 0;
        for (unsigned t = half * 3; d->heaviest >= 0 && !d->outside && t < half * 3 + 3; ++t) diag_kb(line, names[t], d->engine_free[t]);
        diag_row(13 + half, 168 + half * 10, line, DIM);
    }
    if (paint_rendering && paint_top + PAINT_ROWS <= 195) return;
    knob(0, "METHOD", method == SONG_DELIVER_A ? "A" : "B", colors[0]);
    knob(1, "SONG", song_port_player()->playing ? "PLAY" : "STOP", colors[1]);
    knob(2, "-", "--", colors[2]);
    knob(3, "-", "--", colors[3]);
    if (notice && (int32_t)(frame_time - notice_until) >= 0) notice = NULL;
    const char *hint = notice ? notice : "PLAY SONG SEL RESET ENV MEM HOME BACK";
    uint32_t sig = signature(0, hint, (unsigned)strlen(hint));
    if (sig == cache->hint_cache) return;
    cache->hint_cache = sig;
    paint_fill(6, 231, 228, 7, BACK);
    text(6, 231, hint, 1, DIM, BACK);
}

/* K1 picks A or B, while stopped. HOME goes back to the stock overview. */
static int song_diag_input(const struct fm1_controls *c, uint32_t *pressed, uint32_t key_down, uint32_t now)
{
    (void)key_down;
    struct song_player *p = song_port_player();
    if (*pressed & BUTTON(HOME)) { page = music_page = OVERVIEW; cache->shown_page = ~0U; *pressed &= ~BUTTON(HOME); return 1; }
    if (*pressed & BUTTON(PLAY)) { if (p->playing) song_port_stop(); else song_start(now); }
    if (*pressed & BUTTON(SCL)) song_diag_reset();
    if (*pressed & BUTTON(LFO)) (void)song_diag_largest_free();
    if (*pressed & BUTTON(ENV)) {
        if (p->playing) { notice = "STOP THE SONG FIRST"; notice_until = now + 1500U; }
        else { notice = song_diag_engines() ? "ENGINE PROBE STOPPED" : "ENGINE PROBE DONE (KB FREE)"; notice_until = now + 2500U; }
    }
    *pressed &= ~(BUTTON(PLAY) | BUTTON(FX) | BUTTON(SCL) | BUTTON(ENV) | BUTTON(LFO) | BUTTON(REC) | BUTTON(ARP));
    if (*pressed & (BUTTON(EDIT) | BUTTON(GLO) | BUTTON(SAVE))) return 0;
    if (c->encoders[2] && song_port_set_method(c->encoders[2] > 0 ? SONG_DELIVER_B : SONG_DELIVER_A)) {
        notice = "STOP THE SONG TO CHANGE METHOD";
        notice_until = now + 1500U;
    }
    return 1;
}
