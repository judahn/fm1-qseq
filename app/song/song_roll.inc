/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* song-mode: the roll (0.5.0), included by song_step.inc. One track, or a
 * take's, as a small piano roll over a span ALGO zooms: 4 bars, 2 bars, 1
 * bar or half a bar, starting on a bar line in pages of the span from the
 * start of the song or clip. Its pitch range comes from the notes in the
 * span. Beats are shaded, a sweep's range is a brighter band, and lines
 * mark bars, beats and, at 1 bar or closer, 16ths (and 32nds at half a
 * bar), each level only where its lines are 4 px apart or more. Under the
 * roll a dash per cell, the pointer's lit. Notes end 1 px short, so two in
 * a row on one pitch read as two. The marking pass reads what the roll
 * shows once (step_now, as song_now is read), and the playhead is drawn
 * apart from the rest, so a frame while playing repaints only its columns. */

enum {
    STEP_GX = 8, STEP_GW = 224, STEP_GY = 50, STEP_GH = 108, STEP_SHADE = 0x0861,
    STEP_BAR = 0x5acb, STEP_BEAT = 0x4228, STEP_FINE = 0x2104, STEP_BAND = 0x0884, STEP_BAND_ODD = 0x10a6
};
static const char *const step_spans[4] = {"4 BARS", "2 BARS", "1 BAR", "1/2 BAR"};
static uint8_t step_zoom = 2; /* 1 bar, kept while the FM-1 is on */
static struct {
    uint32_t start, length, pointer, from, to, band, revision, pick[4]; /* pointer ~0U: not shown, band: the sweep's end shown */
    uint16_t cell;                                                 /* a dash's clocks */
    uint8_t part, track, low, high, zoom, take, picked, audible, key;
} step_now;
static uint32_t step_clock = ~0U, step_drawn_x = ~0U; /* the playhead, ~0U when none */

static void step_zoom_turn(int delta) { if (delta) step_zoom = (uint8_t)clamp(step_zoom + delta, 0, 3); }
static unsigned step_x(uint32_t clock)
{
    uint32_t start = step_now.start, length = step_now.length;
    if (clock <= start) return STEP_GX;
    return clock - start >= length ? STEP_GX + STEP_GW : STEP_GX + (clock - start) * STEP_GW / length;
}
static int step_inside(uint32_t clock) { return clock >= step_now.start && clock - step_now.start < step_now.length; }
/* The span holding `at`. */
static void step_span(const struct song *s, uint32_t at)
{
    uint32_t start, length;
    unsigned bar = song_bar_at(s, at, &start, &length), n = 4U >> step_zoom;
    if (step_zoom == 3) {
        step_now.length = length / 2U;
        step_now.start = start + (at - start >= length / 2U ? length / 2U : 0);
        return;
    }
    step_now.start = song_bar_clock(s, bar / n * n);
    step_now.length = song_bar_clock(s, bar / n * n + n) - step_now.start;
}
/* What the roll shows: step entry's track, or a take's (its clip while one
 * is open, or a cell's clip still waiting to launch), and its clocks. */
static void step_read(const struct song_step *st)
{
    const struct song *s = st->song;
    const struct song_player *p = song_port_player();
    unsigned waiting = song_grid_rec_state() && !s->open, count, low = 127, high = 0, fine;
    uint32_t from, to;
    memset(&step_now, 0, sizeof step_now);
    step_now.take = song_port_recording() != 0;
    step_now.part = (uint8_t)(page == SONG_STEP ? st->track : s->open ? song_editing(s)
        : waiting ? SONG_TRACKS + song_grid_rec_cell() : song_port_recorder()->track);
    step_now.track = (uint8_t)(step_now.part < SONG_TRACKS ? step_now.part : (step_now.part - SONG_TRACKS) / SONG_GRID_ROWS);
    step_clock = p->playing && !waiting ? song_player_clock(p) : ~0U;
    step_now.zoom = step_zoom;
    step_span(s, step_clock != ~0U ? step_clock : step_now.take ? 0 : st->pointer);
    step_now.pointer = !step_now.take && step_inside(st->pointer) ? st->pointer : ~0U;
    if (!step_now.take && step_range(st, &from, &to)) { /* the band covers the pointer's step too */
        step_now.from = from;
        step_now.to = to;
        step_now.band = to - 1U + step_clocks(st);
    }
    /* The pitch range and the dash's size from what is in the span. */
    const struct song_event *e = song_track(s, step_now.part, &count);
    unsigned first = song_find(s, step_now.part, step_now.start), last = song_find(s, step_now.part, step_now.start + step_now.length);
    fine = step_now.pointer != ~0U && step_now.pointer % 60U;
    for (unsigned i = first > 16 ? first - 16 : 0; i < last; ++i) { /* notes still sounding from before, too */
        uint32_t at = song_event_start(&e[i]);
        if (!song_is_note(&e[i]) || at + e[i].length <= step_now.start) continue;
        if (e[i].a < low) low = e[i].a;
        if (e[i].a > high) high = e[i].a;
        fine |= at % 60U == 30U || (at + e[i].length) % 60U == 30U;
    }
    if (low > high) low = high = 60;
    while (high - low < 11) { if (high < 127) ++high; if (high - low < 11 && low > 0) --low; }
    step_now.low = (uint8_t)low;
    step_now.high = (uint8_t)high;
    uint32_t cell = step_zoom == 0 ? 240U : step_zoom == 1 ? 120U : fine ? 30U : 60U;
    while (cell * STEP_GW < 3U * step_now.length) cell *= 2U; /* a dash at least 2 px */
    step_now.cell = (uint16_t)cell;
    step_now.revision = s->revision;
    step_now.picked = st->picked;
    memcpy(step_now.pick, st->pick, sizeof st->pick);
    step_now.audible = frame_views[step_now.track].audible;
    step_now.key = (uint8_t)(s->key_root | s->key_scale << 4);
}
/* The finest lines in a bar that are 4 px apart: 32nds at half a bar, 16ths
 * at 1 bar, else beats, or 0 for bar lines alone. */
static uint32_t step_unit(uint32_t beat)
{
    uint32_t unit = step_now.zoom == 3 ? 60U : step_now.zoom == 2 ? 120U : beat, wide = 4U * step_now.length;
    if (unit > beat) unit = beat;
    if (unit * STEP_GW < wide) unit = unit < beat && beat * STEP_GW >= wide ? beat : 0;
    return unit;
}
static void step_grid(const struct song_step *st)
{
    const struct song *s = st->song;
    uint32_t start = step_now.start, end = start + step_now.length, from = step_now.from, to = step_now.to, band = step_now.band;
    unsigned count, low = step_now.low, high = step_now.high, rows = high - low + 1, row = STEP_GH / rows;
    unsigned tall = row > 2 ? row - 1 : row ? row : 1;
    for (unsigned bar = song_bar_at(s, start, NULL, NULL);; ++bar) {
        uint32_t at = song_bar_clock(s, bar), next = song_bar_clock(s, bar + 1), beat = SONG_WHOLE / song_meter_at(s, bar)->unit;
        if (at >= end) break;
        uint32_t unit = step_unit(beat), stop = next < end ? next : end;
        for (uint32_t b = at; b < stop; b += beat) { /* shading, the sweep's band, and a faint line on each C */
            uint32_t b2 = b + beat < stop ? b + beat : stop, lo = b > from ? b : from, hi = b2 < band ? b2 : band;
            unsigned odd = (b - at) / beat & 1U, x = step_x(b);
            if (b2 <= start) continue;
            paint_fill(x, STEP_GY, step_x(b2) - x, STEP_GH, odd ? STEP_SHADE : BACK);
            if (lo < hi) paint_fill(step_x(lo), STEP_GY, step_x(hi) - step_x(lo), STEP_GH, odd ? STEP_BAND_ODD : STEP_BAND);
            for (unsigned p = low; p <= high; ++p)
                if (p % 12 == 0) paint_fill(x, STEP_GY + (high - p) * STEP_GH / rows + row / 2, step_x(b2) - x, 1, PANEL);
        }
        for (uint32_t c = unit ? at + unit : stop; c < stop; c += unit) /* beats a step dimmer, 16ths and 32nds faint */
            if (c > start) paint_fill(step_x(c), STEP_GY, 1, STEP_GH, (c - at) % beat ? STEP_FINE : STEP_BEAT);
        if (at >= start) {
            paint_fill(step_x(at), STEP_GY, 1, STEP_GH, STEP_BAR);
            if (step_now.zoom < 2) number(step_x(at) + 3, STEP_GY - 8, (int)bar + 1, 1, 1, DIM, BACK); /* 2 and 4 bars */
        }
        for (uint32_t c = at; c < stop; c += step_now.cell) { /* the QY's dashes */
            uint32_t c2 = c + step_now.cell < stop ? c + step_now.cell : stop;
            if (c2 <= start) continue;
            unsigned x = step_x(c), w = step_x(c2) - x, on = step_now.pointer >= c && step_now.pointer < c2;
            paint_fill(x, STEP_GY + STEP_GH + 3, w > 1 ? w - 1 : 1, 2, on ? AMBER : (c - at) % beat ? PANEL : DIM);
        }
    }
    unsigned index[STEP_CHORD], selected = step_now.take || from < to ? 0 : step_selection(st, index), part = step_now.part;
    const struct song_event *e = song_track(s, part, &count);
    unsigned first = song_find(s, part, start), last = song_find(s, part, end);
    for (unsigned i = first > 16 ? first - 16 : 0; i < last; ++i) {
        uint32_t at = song_event_start(&e[i]);
        if (!song_is_note(&e[i]) || at + e[i].length <= start) continue;
        unsigned x = step_x(at), w = step_x(at + e[i].length) - x, chosen = at >= from && at < to && step_chosen(st, &e[i]);
        for (unsigned k = 0; k < selected; ++k) chosen |= index[k] == s->first[part] + i;
        paint_fill(x, STEP_GY + (high - e[i].a) * STEP_GH / rows, w > 1 ? w - 1 : 1, tall,
                   chosen ? TEXT : step_now.audible ? colors[step_now.track] : DIM);
        if (song_event_glitch(&e[i])) /* glitched: a mark on its start */
            paint_fill(x, STEP_GY + (high - e[i].a) * STEP_GH / rows - 2, 2, 2, AMBER);
    }
    if (step_now.pointer == ~0U) return;
    unsigned x = step_x(step_now.pointer);
    paint_fill(x, STEP_GY - 2, 1, STEP_GH + 2, AMBER);
    paint_fill(x > 2 ? x - 2 : 0, STEP_GY - 5, 5, 3, AMBER);
}
/* The playhead, in the song view's color: the marking pass damages its old
 * and new columns, and every band draws it over the roll. */
static void step_playhead(void)
{
    unsigned x = step_clock != ~0U && step_inside(step_clock) ? step_x(step_clock) : ~0U;
    if (!paint_rendering && x != step_drawn_x) {
        if (step_drawn_x != ~0U) paint_fill(step_drawn_x, STEP_GY, 1, STEP_GH, BACK);
        if (x != ~0U) paint_fill(x, STEP_GY, 1, STEP_GH, GREEN);
        step_drawn_x = x;
    } else if (paint_rendering && step_drawn_x != ~0U)
        paint_fill(step_drawn_x, STEP_GY, 1, STEP_GH, GREEN);
}
