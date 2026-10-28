/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* song-mode: the song view. screen.c includes this file so the view uses its
 * band painter, text, knob footer and draw caches. As on every stock page,
 * the marking pass records damage and the band pass repaints from a frame
 * snapshot. Transport, snap and zoom, markers, the loop and the selection.
 * The header, playhead and recording knobs are in song_header.inc, step
 * entry in song_step.inc, settings in song_setup.inc, REC's chooser in
 * song_rec.inc, arranging in song_select.inc, the automation view in
 * song_auto.inc and input in song_panel.inc. */
#include "song_port.h"

enum { SONG = STARTUP + 1, SONG_STEP, SONG_SETUP, SONG_DIAG, SONG_REC, SONG_AUTO, SONG_LIST }; /* song view, step entry,
                                                              settings, diagnostics, REC, automation, songs */
enum {
    SONG_X = 22, SONG_W = 214, SONG_RULER = 32, SONG_ROWS = 48, SONG_ROW = 18,
    SONG_BOTTOM = SONG_ROWS + 8 * SONG_ROW - 2,
    SONG_BAR_LINE = 0x4228, SONG_BEAT_LINE = 0x2104, SONG_LOOP = 0x1147, SONG_SELECT = 0x0884
};
static const char *const song_snaps[SONG_ZOOMS] = {"1 BAR", "1/2 BAR", "BEAT", "1/8"};
static const char *const song_spans[SONG_ZOOMS] = {"16 BARS", "8 BARS", "4 BARS", "2 BARS"};
static unsigned song_zoom;
static uint32_t song_window = ~0U, song_drawn_x = ~0U, song_ranged = ~0U;
static uint8_t song_low[SONG_TRACKS], song_high[SONG_TRACKS];
static struct {
    uint32_t clock, target, ghost_from, ghost_to;
    uint8_t playing, jumping, shift, recording, selected, ghost_tracks, glitching;
} song_now; /* taken by the marking pass, used by every band */
static void select_track(int delta);
static void song_start(uint32_t now);
#if SONG_UI
#include "song_select.inc"
#include "song_glitch.inc"
#include "lanes.h"

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
            if (!song_is_note(&e[i])) continue;
            if (e[i].a < low) low = e[i].a;
            if (e[i].a > high) high = e[i].a;
        }
        song_low[t] = (uint8_t)(low <= high ? low : 60);
        song_high[t] = (uint8_t)(low <= high ? high : 60);
    }
}
#include "song_header.inc"

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
    unsigned picked = song_now.selected >> t & 1U, ghost = song_now.ghost_tracks >> t & 1U;
    sig = signature(sig, &s->loop_start, 8) ^ s->loop_on ^ (picked << 1) ^ (ghost << 2);
    if (ghost) sig = signature(sig, &song_now.ghost_from, 8);
    if (sig == cache->row_cache[t]) return;
    cache->row_cache[t] = sig;
    paint_fill(0, y, 240, SONG_ROW - 1, BACK);
    if (!paint_rendering) return; /* the whole row is marked */
    uint16_t bg = t == track ? colors[t] : PANEL;
    paint_fill(3, y, 16, SONG_ROW - 2, bg);
    number(8, y + 5, (int)t + 1, 1, 1, t == track ? BACK : colors[t], bg);
    uint32_t end = song_window + song_span();
    if (picked && s->loop_end > song_window && s->loop_start < end) /* the selection, brighter when looping */
        paint_fill(song_x(s->loop_start), y, song_x(s->loop_end) - song_x(s->loop_start), SONG_ROW - 2,
                   s->loop_on ? SONG_LOOP : SONG_SELECT);
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
        if (at + e[i].length <= song_window || !song_is_note(&e[i])) continue;
        unsigned x = song_x(at), w = song_x(at + e[i].length) - x;
        w = w > 2 ? w - 1 : 1;
        unsigned rise = span ? (unsigned)(e[i].a - song_low[t]) * 13U / span : 6U;
        paint_fill(x, y + 14 - rise, w, 2, color);
    }
    /* Where a paste would land: an outline from the playhead. */
    if (ghost && song_now.ghost_to > song_window && song_now.ghost_from < end) {
        unsigned x1 = song_x(song_now.ghost_from), x2 = song_x(song_now.ghost_to);
        frame(x1, y, x2 > x1 + 1 ? x2 - x1 : 2, SONG_ROW - 2, TEXT);
    }
}

static void song_draw(void)
{
    struct song *s = song_port_song();
    if (!paint_rendering) song_now_read(s);
    song_header(s);
    song_memory_bars();
    song_ruler(s);
    for (unsigned t = 0; t < SONG_TRACKS; ++t) song_row(s, t);
    song_playhead_line();
    if (paint_rendering && paint_top + PAINT_ROWS <= 195) return;
    song_footer(s, "MARKER");
    if (notice && (int32_t)(frame_time - notice_until) >= 0) notice = NULL;
    char selection[40], part[12];
    if (song_now.recording) {
        strcpy(selection, "REC ");
        strcat(selection, song_rec_modes[song_port_recorder()->mode]);
        strcat(selection, "  PLAY OR REC STOPS");
    } else {
        uint32_t length = s->loop_end - s->loop_start;
        strcpy(selection, "SEL ");
        decimal(part, (int)(length / SONG_WHOLE), 1); strcat(selection, part); strcat(selection, ".");
        decimal(part, (int)(length % SONG_WHOLE * 10U / SONG_WHOLE), 1); strcat(selection, part);
        strcat(selection, " BARS:");
        unsigned picked = song_selected();
        if (picked == 0xff) strcat(selection, " ALL");
        else for (unsigned t = 0; t < SONG_TRACKS && strlen(selection) < 34; ++t)
            if (picked >> t & 1U) { strcat(selection, " T"); decimal(part, (int)t + 1, 1); strcat(selection, part); }
    }
    char glitch[40];
    const char *hint = notice ? notice : song_now.glitching ? song_stutter_hint(glitch)
        : song_now.jumping ? "JUMP WAITS FOR THE NEXT BAR"
        : song_now.shift ? "F3-E4 EDIT F#3-A#3 MANGLE F4-G5 MARK"
        : (previous_buttons & BUTTON(LFO)) ? "LFO + F3-F4 TRACKS  G4 ALL" : selection;
    uint32_t sig = signature(0, hint, (unsigned)strlen(hint));
    if (sig == cache->hint_cache) return;
    cache->hint_cache = sig;
    paint_fill(6, 231, 228, 7, BACK);
    text(6, 231, hint, 1, DIM, BACK);
}

#include "song_step.inc"
#include "song_setup.inc"
#include "song_rec.inc"
#include "song_auto.inc"
#include "song_list.inc"
#endif
#if SONG_HW_TEST
#include "song_diag.inc"
#endif
static void song_page_draw(unsigned visible)
{
#if SONG_HW_TEST
    if (visible == SONG_DIAG) { song_diag_draw(); return; }
#endif
#if SONG_UI
    if (visible == SONG_STEP) song_step_draw();
    else if (visible == SONG_SETUP) song_setup_draw();
    else if (visible == SONG_REC) song_rec_draw();
    else if (visible == SONG_AUTO) song_auto_draw();
    else if (visible == SONG_LIST) song_list_draw();
    else song_draw();
#endif
    (void)visible;
}
#include "song_panel.inc"
