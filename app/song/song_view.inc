/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* song-mode: the song view. screen.c includes this file so the view uses its
 * band painter, text, knob footer and draw caches. As on every stock page,
 * the marking pass records damage and the band pass repaints from a frame
 * snapshot. Phase 1: transport, snap and zoom, markers and the loop.
 * Step entry is in song_step.inc, settings in song_setup.inc and input in
 * song_panel.inc. */
#include "song_port.h"

enum { SONG = STARTUP + 1, SONG_STEP, SONG_SETUP }; /* song view, step entry, settings */
enum {
    SONG_X = 22, SONG_W = 214, SONG_RULER = 32, SONG_ROWS = 48, SONG_ROW = 18,
    SONG_BOTTOM = SONG_ROWS + 8 * SONG_ROW - 2,
    SONG_BAR_LINE = 0x4228, SONG_BEAT_LINE = 0x2104, SONG_LOOP = 0x1147
};
static const char *const song_snaps[SONG_ZOOMS] = {"1 BAR", "1/2 BAR", "BEAT", "1/8"};
static const char *const song_spans[SONG_ZOOMS] = {"16 BARS", "8 BARS", "4 BARS", "2 BARS"};
static unsigned song_zoom;
static uint32_t song_window = ~0U, song_drawn_x = ~0U, song_ranged = ~0U;
static uint8_t song_low[SONG_TRACKS], song_high[SONG_TRACKS];
static struct {
    uint32_t clock, target;
    uint8_t playing, jumping, shift;
} song_now; /* taken by the marking pass, used by every band */
static void select_track(int delta);
static void song_start(uint32_t now);

static uint32_t song_span(void) { return (16U >> song_zoom) * (uint32_t)SONG_WHOLE; }
static unsigned song_x(uint32_t clock)
{
    if (clock <= song_window) return SONG_X;
    uint64_t at = clock - song_window;
    return at >= song_span() ? SONG_X + SONG_W : SONG_X + (unsigned)(at * SONG_W / song_span());
}
static int song_visible(uint32_t clock) { return clock >= song_window && clock - song_window < song_span(); }
/* Page so the playhead stays on screen: 16 bars at a time when zoomed out. */
static void song_follow(const struct song *s, uint32_t clock)
{
    if (song_visible(clock)) return;
    uint32_t start;
    unsigned bar = song_bar_at(s, clock, &start, NULL), bars = 16U >> song_zoom;
    song_window = song_bar_clock(s, bar / bars * bars);
    if (!song_visible(clock)) song_window = start;
}
/* Each row maps its own pitch range onto its height. */
static void song_ranges(const struct song *s)
{
    if (song_ranged == s->revision) return;
    song_ranged = s->revision;
    for (unsigned t = 0; t < SONG_TRACKS; ++t) {
        unsigned count, low = 127, high = 0;
        const struct song_event *e = song_track(s, t, &count);
        for (unsigned i = 0; i < count; ++i) {
            if (e[i].a < low) low = e[i].a;
            if (e[i].a > high) high = e[i].a;
        }
        song_low[t] = (uint8_t)(count ? low : 60);
        song_high[t] = (uint8_t)(count ? high : 60);
    }
}
static void song_position(const struct song *s, uint32_t clock, char out[24], unsigned full)
{
    unsigned bbc[3];
    char part[12];
    song_bbc(s, clock, bbc);
    decimal(out, (int)bbc[0], full ? 3 : 1);
    strcat(out, full ? ":" : ".");
    decimal(part, (int)bbc[1], 1);
    strcat(out, part);
    if (!full && !bbc[2]) return;
    strcat(out, full ? ":" : ".");
    decimal(part, (int)bbc[2], 3);
    strcat(out, part);
}

static void song_header(const struct song *s)
{
    if (paint_rendering && paint_top >= 30) return;
    char position[24];
    song_position(s, song_now.clock, position, 1);
    uint32_t sig = signature(0, position, (unsigned)strlen(position));
    if (sig != cache->middle_cache) {
        cache->middle_cache = sig;
        paint_fill(4, 4, 116, 18, BACK);
        text(6, 6, position, 2, TEXT, BACK);
    }
    unsigned bar = song_bar_at(s, song_now.clock, NULL, NULL), bpm = s->tempo[song_tempo_at(s, song_now.clock)].bpm100;
    const struct song_meter *m = song_meter_at(s, bar);
    int marker = song_marker_step(s, song_now.clock + 1U, -1);
    sig = song_now.playing | (s->loop_on << 1) | (song_now.jumping << 2) | ((uint32_t)(marker + 1) << 3) |
          ((uint32_t)bpm << 8);
    sig = signature(sig, m, sizeof *m);
    if (sig == cache->header_cache) return;
    cache->header_cache = sig;
    paint_fill(122, 0, 118, 27, BACK);
    text(124, 5, song_now.playing ? "PLAY" : "STOP", 1, song_now.playing ? GREEN : DIM, BACK);
    text(124, 16, "LOOP", 1, s->loop_on ? ACCENT : PANEL, BACK);
    char name[SONG_MARKER_NAME + 1] = {0};
    if (marker >= 0) memcpy(name, s->marker[marker].name, SONG_MARKER_NAME);
    text(154, 5, name, 1, song_now.jumping ? DIM : AMBER, BACK);
    char meter[12], unit[12];
    decimal(meter, m->beats, 1);
    decimal(unit, m->unit, 1);
    strcat(meter, "/");
    strcat(meter, unit);
    text(154, 16, meter, 1, DIM, BACK);
    if (bpm % 100) {
        char tempo[12], hundredths[12];
        decimal(tempo, (int)(bpm / 100), 1);
        decimal(hundredths, (int)(bpm % 100), 2);
        strcat(tempo, ".");
        strcat(tempo, hundredths);
        text(200, 8, tempo, 1, TEXT, BACK);
    } else number(200, 4, (int)(bpm / 100), 3, 2, TEXT, BACK);
    text(212, 19, "BPM", 1, DIM, BACK);
}

/* Bar numbers, marker flags, the loop bracket and a pending jump. */
static void song_ruler(const struct song *s)
{
    if (paint_rendering && (paint_top >= SONG_ROWS || paint_top + PAINT_ROWS <= SONG_RULER)) return;
    uint32_t sig = signature(song_window ^ (song_zoom << 28), &s->revision, sizeof s->revision);
    sig = signature(sig, &s->loop_start, 8) ^ s->loop_on ^ (song_now.jumping ? song_now.target << 1 : 0);
    if (sig == cache->title_cache) return;
    cache->title_cache = sig;
    paint_fill(0, SONG_RULER, 240, SONG_ROWS - SONG_RULER, BACK);
    uint32_t end = song_window + song_span(), every = song_zoom < 2 ? 4U >> song_zoom : 1;
    for (unsigned bar = song_bar_at(s, song_window, NULL, NULL); bar < s->bars; ++bar) {
        uint32_t at = song_bar_clock(s, bar);
        if (at >= end) break;
        if (at < song_window || bar % every) continue;
        paint_fill(song_x(at), SONG_RULER, 1, 7, SONG_BAR_LINE);
        number(song_x(at) + 2, SONG_RULER, (int)bar + 1, 1, 1, DIM, BACK);
    }
    if (s->loop_end > song_window && s->loop_start < end)
        paint_fill(song_x(s->loop_start), SONG_ROWS - 2, song_x(s->loop_end) - song_x(s->loop_start), 2,
                   s->loop_on ? ACCENT : DIM);
    for (unsigned i = 0; i < s->markers; ++i)
        if (song_visible(s->marker[i].clock)) {
            unsigned x = song_x(s->marker[i].clock);
            paint_fill(x, SONG_RULER + 6, 1, 8, AMBER);
            number(x + 2, SONG_RULER + 7, (int)i + 1, 1, 1, AMBER, BACK);
        }
    if (song_now.jumping && song_visible(song_now.target))
        paint_fill(song_x(song_now.target) - 1, SONG_ROWS - 5, 3, 3, GREEN);
}

static void song_row(const struct song *s, unsigned t)
{
    unsigned y = SONG_ROWS + t * SONG_ROW;
    if (paint_rendering && (y >= paint_top + PAINT_ROWS || y + SONG_ROW <= paint_top)) return;
    uint32_t sig = signature(song_window ^ (song_zoom << 28) ^ ((t == track) << 30) ^
                             ((uint32_t)frame_views[t].audible << 31), &s->revision, sizeof s->revision);
    sig = signature(sig, &s->loop_start, 8) ^ s->loop_on;
    if (sig == cache->row_cache[t]) return;
    cache->row_cache[t] = sig;
    paint_fill(0, y, 240, SONG_ROW - 1, BACK);
    if (!paint_rendering) return; /* the whole row is marked */
    uint16_t bg = t == track ? colors[t] : PANEL;
    paint_fill(3, y, 16, SONG_ROW - 2, bg);
    number(8, y + 5, (int)t + 1, 1, 1, t == track ? BACK : colors[t], bg);
    uint32_t end = song_window + song_span();
    if (s->loop_on && s->loop_end > song_window && s->loop_start < end)
        paint_fill(song_x(s->loop_start), y, song_x(s->loop_end) - song_x(s->loop_start), SONG_ROW - 2, SONG_LOOP);
    for (unsigned bar = song_bar_at(s, song_window, NULL, NULL); bar <= s->bars; ++bar) {
        uint32_t at = song_bar_clock(s, bar), beat = SONG_WHOLE / song_meter_at(s, bar)->unit;
        if (at >= end) break;
        if (at >= song_window) paint_fill(song_x(at), y, 1, SONG_ROW - 2, SONG_BAR_LINE);
        if (song_zoom >= 2 && bar < s->bars)
            for (uint32_t b = at + beat; b < song_bar_clock(s, bar + 1) && b < end; b += beat)
                if (b > song_window) paint_fill(song_x(b), y, 1, SONG_ROW - 2, SONG_BEAT_LINE);
    }
    /* Notes as marks, placed by pitch. Look back a little for notes that
     * started before the window and still sound inside it. */
    unsigned count, first = song_find(s, t, song_window), span = song_high[t] - song_low[t];
    const struct song_event *e = song_track(s, t, &count);
    uint16_t color = frame_views[t].audible ? colors[t] : DIM;
    for (unsigned i = first > 32 ? first - 32 : 0; i < count; ++i) {
        uint32_t at = song_event_start(&e[i]);
        if (at >= end) break;
        if (at + e[i].length <= song_window) continue;
        unsigned x = song_x(at), w = song_x(at + e[i].length) - x;
        w = w > 2 ? w - 1 : 1;
        unsigned rise = span ? (unsigned)(e[i].a - song_low[t]) * 13U / span : 6U;
        paint_fill(x, y + 14 - rise, w, 2, color);
    }
}

static void song_draw(void)
{
    struct song *s = song_port_song();
    if (!paint_rendering) {
        const struct song_player *p = song_port_player();
        song_now.clock = song_player_clock(p);
        song_now.target = song_player_target(p);
        song_now.playing = p->playing;
        song_now.jumping = p->jumping;
        song_now.shift = (previous_buttons & BUTTON(FX)) != 0;
        song_follow(s, song_now.clock);
        song_ranges(s);
    }
    song_header(s);
    song_ruler(s);
    for (unsigned t = 0; t < SONG_TRACKS; ++t) song_row(s, t);
    /* The playhead: the marking pass damages its old and new columns. */
    unsigned x = song_visible(song_now.clock) ? song_x(song_now.clock) : ~0U;
    if (!paint_rendering && x != song_drawn_x) {
        if (song_drawn_x != ~0U) paint_fill(song_drawn_x, SONG_RULER, 1, SONG_BOTTOM - SONG_RULER, BACK);
        if (x != ~0U) paint_fill(x, SONG_RULER, 1, SONG_BOTTOM - SONG_RULER, GREEN);
        song_drawn_x = x;
    } else if (paint_rendering && song_drawn_x != ~0U)
        paint_fill(song_drawn_x, SONG_RULER, 1, SONG_BOTTOM - SONG_RULER, GREEN);
    if (paint_rendering && paint_top + PAINT_ROWS <= 195) return;
    char start[24], end[24];
    song_position(s, s->loop_start, start, 0);
    song_position(s, s->loop_end, end, 0);
    knob(0, song_now.shift ? "MARKER" : "SNAP", song_now.shift ? "< >" : song_snaps[song_zoom], colors[0]);
    knob(1, "LOOP IN", start, colors[1]);
    knob(2, "LOOP OUT", end, colors[2]);
    knob(3, "ZOOM", song_spans[song_zoom], colors[3]);
    if (notice && (int32_t)(frame_time - notice_until) >= 0) notice = NULL;
    const char *hint = notice ? notice : song_now.jumping ? "JUMP WAITS FOR THE NEXT BAR"
        : song_now.shift ? "SHIFT + F4..G5: MARKERS 1-9" : "FX SHIFT  ENV LOOP  REC STEP  GLO SET";
    uint32_t sig = signature(0, hint, (unsigned)strlen(hint));
    if (sig == cache->hint_cache) return;
    cache->hint_cache = sig;
    paint_fill(6, 231, 228, 7, BACK);
    text(6, 231, hint, 1, DIM, BACK);
}

#include "song_step.inc"
#include "song_setup.inc"
static void song_page_draw(unsigned visible)
{
    if (visible == SONG_STEP) song_step_draw();
    else if (visible == SONG_SETUP) song_setup_draw();
    else song_draw();
}
#include "song_panel.inc"
