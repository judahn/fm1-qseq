/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* song-mode: the automation view (SHIFT+LFO, design sections 7 and 8c).
 * The conductor row on top holds tempo and time signature changes, and
 * each track row shows one lane's curve. SELECT picks a row, past track 1
 * up to the conductor, and PRESETS the lane shown, or on the conductor
 * tempo or time signature. While stopped, ALGO sets the change at the
 * playhead: tempo there, the time signature from that bar, or a program
 * change. SCL deletes the change at the playhead and SHIFT+SCL clears the
 * shown lanes in the selection. Everything else works as in the song view,
 * and SHIFT+K1 goes from change to change on the row. */

enum { AUTO_C = SONG_ROWS, AUTO_CH = 13, AUTO_Y = SONG_ROWS + 15, AUTO_ROW = 16 };
static uint8_t song_auto_lanes[SONG_TRACKS]; /* lane shown per track, 0 for the first knob's */
static uint8_t song_auto_conductor, song_auto_mode; /* mode: tempo or time signature */
static int song_view_input(const struct fm1_controls *c, uint32_t *pressed, uint32_t key_down, uint32_t now);

static unsigned song_auto_lane(unsigned t) { return song_auto_lanes[t] ? song_auto_lanes[t] : song_knob_lane(t, 0); }
static const char *song_lane_name(unsigned t, unsigned lane)
{
    if (lane == SONG_LANE_PROGRAM) return "PROGRAM";
    const struct song_param *p = song_param_for(song_port_engine(t), lane);
    return p ? p->name : "CC";
}
static unsigned song_auto_level(unsigned y, unsigned h, unsigned value) { return y + h - 1U - value * (h - 1U) / 127U; }
/* A step line through the lane's changes in the window. */
static void song_auto_curve(const struct song *s, unsigned t, unsigned lane, unsigned y, unsigned h, uint16_t color)
{
    uint32_t end = song_window + song_span();
    int value = song_lane_at(s, t, lane, song_window);
    unsigned count, x0 = SONG_X, i = song_find(s, t, song_window);
    const struct song_event *e = song_track(s, t, &count);
    for (; i < count && song_event_start(&e[i]) < end; ++i) {
        if (!song_is_lane(&e[i]) || song_event_lane(&e[i]) != lane) continue;
        unsigned x = song_x(song_event_start(&e[i])), next = song_event_value(&e[i]);
        unsigned a = song_auto_level(y, h, value < 0 ? next : (unsigned)value), b = song_auto_level(y, h, next);
        if (value >= 0) paint_fill(x0, a, x - x0 + 1U, 1, color);
        paint_fill(x, a < b ? a : b, 1, (a < b ? b - a : a - b) + 1U, color);
        paint_fill(x, b, 2, 2, TEXT);
        value = (int)next;
        x0 = x;
    }
    if (value >= 0) paint_fill(x0, song_auto_level(y, h, (unsigned)value), SONG_X + SONG_W - x0, 1, color);
}
static void song_auto_row(const struct song *s, unsigned t)
{
    unsigned y = AUTO_Y + t * AUTO_ROW, lane = song_auto_lane(t), picked = song_now.selected >> t & 1U;
    if (paint_rendering && (y >= paint_top + PAINT_ROWS || y + AUTO_ROW <= paint_top)) return;
    uint32_t sig = signature(song_window ^ (song_zoom << 28) ^ ((t == track && !song_auto_conductor) << 30) ^
                             (lane << 20) ^ (picked << 31), &s->revision, sizeof s->revision);
    sig = signature(sig, &s->loop_start, 8) ^ s->loop_on;
    if (sig == cache->row_cache[t]) return;
    cache->row_cache[t] = sig;
    paint_fill(0, y, 240, AUTO_ROW - 1, BACK);
    if (!paint_rendering) return;
    unsigned on = t == track && !song_auto_conductor;
    uint16_t bg = on ? colors[t] : PANEL;
    paint_fill(3, y, 16, AUTO_ROW - 2, bg);
    number(8, y + 4, (int)t + 1, 1, 1, on ? BACK : colors[t], bg);
    uint32_t end = song_window + song_span();
    if (picked && s->loop_end > song_window && s->loop_start < end)
        paint_fill(song_x(s->loop_start), y, song_x(s->loop_end) - song_x(s->loop_start), AUTO_ROW - 2,
                   s->loop_on ? SONG_LOOP : SONG_SELECT);
    for (unsigned bar = song_bar_at(s, song_window, NULL, NULL); bar <= song_bars(s); ++bar) {
        uint32_t at = song_bar_clock(s, bar);
        if (at >= end) break;
        if (at >= song_window) paint_fill(song_x(at), y, 1, AUTO_ROW - 2, SONG_BAR_LINE);
    }
    if (!s->open || t == song_editing(s)) /* a clip in the editor: only its row */
        song_auto_curve(s, t, lane, y + 1, AUTO_ROW - 3, frame_views[t].audible ? colors[t] : DIM);
}
/* Tempo as a step line between the song's slowest and fastest, and the
 * time signature changes as flags. */
static void song_auto_conductor_row(const struct song *s)
{
    if (paint_rendering && (AUTO_C >= paint_top + PAINT_ROWS || AUTO_C + AUTO_CH <= paint_top)) return;
    uint32_t sig = signature(song_window ^ (song_zoom << 27) ^ (song_auto_conductor << 29) ^ (song_auto_mode << 30),
                             &s->revision, sizeof s->revision);
    if (sig == cache->seq_cache) return;
    cache->seq_cache = sig;
    paint_fill(0, AUTO_C, 240, AUTO_CH + 1, BACK);
    if (!paint_rendering) return;
    uint16_t bg = song_auto_conductor ? ACCENT : PANEL;
    paint_fill(3, AUTO_C, 16, AUTO_CH, bg);
    text(8, AUTO_C + 3, "C", 1, song_auto_conductor ? BACK : ACCENT, bg);
    unsigned low = SONG_TEMPO_MAX, high = SONG_TEMPO_MIN;
    for (unsigned k = 0; k < s->tempos; ++k) {
        if (s->tempo[k].bpm100 < low) low = s->tempo[k].bpm100;
        if (s->tempo[k].bpm100 > high) high = s->tempo[k].bpm100;
    }
    uint32_t end = song_window + song_span();
    unsigned k = song_tempo_at(s, song_window), x0 = SONG_X, h = AUTO_CH - 2;
    for (;; ++k) {
        unsigned level = high > low ? (s->tempo[k].bpm100 - low) * 127U / (high - low) : 64U;
        unsigned x = k + 1U < s->tempos && s->tempo[k + 1].clock < end ? song_x(s->tempo[k + 1].clock) : SONG_X + SONG_W;
        paint_fill(x0, song_auto_level(AUTO_C + 1, h, level), x - x0, 1, song_auto_mode ? DIM : TEXT);
        if (x >= SONG_X + SONG_W) break;
        paint_fill(x, AUTO_C + 1, 1, h, song_auto_mode ? DIM : TEXT);
        x0 = x;
    }
    for (unsigned m = 0; m < s->meters; ++m) {
        uint32_t at = song_bar_clock(s, s->meter[m].bar);
        if (!song_visible(at)) continue;
        char sig_text[12], unit[12];
        decimal(sig_text, s->meter[m].beats, 1); strcat(sig_text, "/"); decimal(unit, s->meter[m].unit, 1); strcat(sig_text, unit);
        paint_fill(song_x(at), AUTO_C, 1, AUTO_CH, AMBER);
        text(song_x(at) + 2, AUTO_C + 3, sig_text, 1, song_auto_mode ? AMBER : DIM, BACK);
    }
}
static void song_auto_draw(void)
{
    struct song *s = song_port_song();
    if (!paint_rendering) song_now_read(s);
    song_header(s);
    song_ruler(s);
    song_auto_conductor_row(s);
    for (unsigned t = 0; t < SONG_TRACKS; ++t) song_auto_row(s, t);
    song_playhead_line();
    if (paint_rendering && paint_top + PAINT_ROWS <= 195) return;
    char hint[40], part[24];
    song_footer(s, "CHANGE");
    song_notice_expire();
    if (song_auto_conductor && !song_auto_mode) {
        unsigned bpm = s->tempo[song_tempo_at(s, song_now.clock)].bpm100;
        strcpy(hint, "TEMPO "); decimal(part, (int)(bpm / 100), 1); strcat(hint, part);
        strcat(hint, "."); decimal(part, (int)(bpm % 100), 2); strcat(hint, part);
        strcat(hint, "  ALGO SET  SEL DEL");
    } else if (song_auto_conductor) {
        const struct song_meter *m = song_meter_at(s, song_bar_at(s, song_now.clock, NULL, NULL));
        strcpy(hint, "TIME SIG "); decimal(part, m->beats, 1); strcat(hint, part); strcat(hint, "/");
        decimal(part, m->unit, 1); strcat(hint, part); strcat(hint, "  ALGO SET  SEL DEL");
    } else {
        unsigned lane = song_auto_lane(track);
        int value = song_lane_at(s, track, lane, song_now.clock);
        strcpy(hint, "T"); decimal(part, (int)track + 1, 1); strcat(hint, part); strcat(hint, " ");
        strcat(hint, song_lane_name(track, lane));
        if (value >= 0) { strcat(hint, " "); decimal(part, value + (lane == SONG_LANE_PROGRAM), 1); strcat(hint, part); }
        strcat(hint, "  PRESETS LANE");
    }
    const char *shown = notice ? notice : hint;
    song_hint(shown);
}

#include "song_auto_edit.inc"
static int song_auto_input(const struct fm1_controls *c, uint32_t *pressed, uint32_t key_down, uint32_t now)
{
    struct song *s = song_port_song();
    struct fm1_controls k = *c;
    unsigned shift = (c->buttons & BUTTON(FX)) != 0;
    if ((*pressed & BUTTON(LFO)) && shift) { /* back to the song view */
        page = music_page = SONG;
        cache->shown_page = ~0U;
        return 1;
    }
    if ((*pressed & BUTTON(SCL)) && !song_port_recorder()->active) song_auto_delete(s, shift, now);
    *pressed &= ~BUTTON(SCL);
    if (s->open) { song_auto_conductor = 0; k.encoders[0] = 0; } /* a clip in the editor keeps its track, off the conductor */
    if (k.encoders[0] < 0 && !track && !song_auto_conductor) { song_auto_conductor = 1; k.encoders[0] = 0; }
    else if (k.encoders[0] > 0 && song_auto_conductor) { song_auto_conductor = 0; k.encoders[0] = 0; }
    else if (song_auto_conductor) k.encoders[0] = 0;
    if (c->encoders[6]) {
        if (song_auto_conductor) song_auto_mode = (uint8_t)clamp(song_auto_mode + c->encoders[6], 0, 1);
        else { /* the engine's parameters, then programs where it has them */
            const uint8_t *index;
            unsigned n = song_params(song_port_engine(track), &index), lane = song_auto_lane(track), at = 0;
            unsigned all = n + (song_programs(song_port_engine(track)) != 0);
            while (at < n && song_param(index[at])->cc != lane) ++at;
            at = (unsigned)clamp((int)at + c->encoders[6], 0, (int)all - 1);
            song_auto_lanes[track] = (uint8_t)(at < n ? song_param(index[at])->cc : SONG_LANE_PROGRAM);
        }
    }
    if (c->encoders[1]) song_auto_set(s, c->encoders[1], shift, now);
    if (shift && k.encoders[2] && !song_port_recorder()->active) { song_auto_step(s, k.encoders[2]); k.encoders[2] = 0; }
    return song_view_input(&k, pressed, key_down, now);
}
