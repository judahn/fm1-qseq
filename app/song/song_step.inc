/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* song-mode: the step entry view, and a live take's (0.5.0). The track as a
 * small piano roll (song_roll.inc), the pointer, the playhead while it
 * plays, and the QY-style function labels. A take into one track shows the
 * roll too, on the page it started from (song_shown): the take's track, the
 * playhead and the recording knobs, and no pointer. The input is in
 * song_step_input.inc. */

/* The glitch layer: ARP steps through its three pages and back off, and K1
 * to K3 set the selected notes' glitch (or the entry's), PRESETS still
 * moving the pointer and K4 the pitch (0.5.2). */
static uint8_t song_glitch_layer;
static const uint8_t song_glitch_pages[3][3] = {
    {GLITCH_REPEATS, GLITCH_DRIFT, GLITCH_FADE}, {GLITCH_CHANCE, GLITCH_OCTAVE, GLITCH_RANGE},
    {GLITCH_ACCENT, GLITCH_FLAM, GLITCH_SWAP}};
static const char *grid_hint(void); /* song_grid_draw.inc */
#include "song_roll.inc"

/* The selection: note names, as many as fit, or a sweep's count. A sampler
 * note's TUNE follows its name, as in C2+3 (0.6.6). */
static void step_names(const struct song_step *st, char out[40])
{
    unsigned sampler = groove_source_type(st->track) == GROOVE_SAMPLE;
    unsigned index[STEP_CHORD], n = step_selection(st, index);
    uint32_t from, to;
    out[0] = 0;
    if (step_range(st, &from, &to)) {
        n = step_count(st);
        decimal(out, (int)n, 1);
        strcat(out, n == 1 ? " NOTE" : " NOTES");
        return;
    }
    if (!n) { strcpy(out, "EMPTY"); return; }
    for (unsigned i = 0; i < n && strlen(out) < 28; ++i) {
        char name[12], tune[12];
        uint32_t word = song_glitch_word(st->song, &st->song->event[index[i]]);
        note_name(st->song->event[index[i]].a, name);
        if (i) strcat(out, " ");
        strcat(out, name);
        if (sampler && song_glitch_tune(word)) { song_glitch_value(word, GLITCH_TUNE, tune); strcat(out, tune); }
    }
}
/* A take's line: armed for a clip's next pass, the count-in, or its mode. */
static const char *step_take_hint(void)
{
    static char line[40];
    if (page == SONG_GRID) return grid_hint();
    if (song_port_clip_armed()) return "REC FROM THE CLIP'S NEXT PASS";
    if (song_port_live_clock() < 0) return "COUNT IN  PLAY OR REC CALLS IT OFF";
    strcpy(line, "REC ");
    strcat(line, song_rec_modes[song_port_recorder()->mode % SONG_REC_MODES]);
    strcat(line, "  PLAY OR REC ENDS");
    return line;
}
/* Position, STEP or REC, PLAY while playing, the track, the time
 * signature and the span. While playing only the position changes from
 * frame to frame, and only its corner is repainted. */
static void step_header(const struct song_step *st)
{
    if (paint_rendering && paint_top >= 30) return;
    const struct song *s = st->song;
    unsigned t = step_now.track, playing = step_clock != ~0U;
    uint32_t at = playing ? step_clock : step_now.take ? step_now.start : st->pointer;
    char position[24];
    song_position(s, at, position, 1);
    uint32_t sig = signature(0, position, (unsigned)strlen(position));
    if (sig != cache->middle_cache) {
        cache->middle_cache = sig;
        paint_fill(0, 0, 122, 27, BACK);
        text(6, 6, position, 2, TEXT, BACK);
    }
    const struct song_meter *m = song_meter_at(s, song_bar_at(s, at, NULL, NULL));
    sig = signature(t | (uint32_t)frame_views[t].type << 8 | (uint32_t)step_now.take << 12 | (uint32_t)playing << 13 |
                    (uint32_t)step_zoom << 14, m, sizeof *m);
    if (sig == cache->header_cache) return;
    cache->header_cache = sig;
    paint_fill(122, 0, 118, 27, BACK);
    if (step_now.take) { paint_fill(124, 4, 22, 11, 0xf800); text(126, 6, "REC", 1, TEXT, 0xf800); }
    else {
        paint_fill(124, 4, 30, 11, AMBER);
        text(127, 6, "STEP", 1, BACK, AMBER);
        if (playing) text(160, 6, "PLAY", 1, GREEN, BACK);
    }
    text(124, 18, "T", 1, colors[t], BACK);
    number(130, 18, (int)t + 1, 1, 1, colors[t], BACK);
    text(144, 18, source_names[frame_views[t].type], 1, DIM, BACK);
    number(206, 6, m->beats, 1, 1, DIM, BACK);
    text(206 + (m->beats > 9 ? 12 : 6), 6, "/", 1, DIM, BACK);
    number(212 + (m->beats > 9 ? 12 : 6), 6, m->unit, 1, 1, DIM, BACK);
    text(234 - 6 * (unsigned)strlen(step_spans[step_zoom]), 18, step_spans[step_zoom], 1, DIM, BACK); /* ALGO's span */
}
static void song_step_draw(void)
{
    const struct song_step *st = song_port_step();
    const struct song *s = st->song;
    char line[40];
    if (!paint_rendering) step_read(st);
    step_header(st);
    /* The selection line and the roll change together. */
    if (!paint_rendering || (paint_top < STEP_GY + STEP_GH + 6 && paint_top + PAINT_ROWS > 30)) {
        uint32_t sig = signature(0x5e1U, &step_now, sizeof step_now);
        if (sig != cache->title_cache) {
            cache->title_cache = sig;
            paint_fill(0, 30, 240, STEP_GY + STEP_GH + 6 - 30, BACK);
            if (paint_rendering) {
                if (!step_now.take) {
                    step_names(st, line);
                    text(8, 34, st->picked ? "PICK" : "SEL", 1, DIM, BACK);
                    text(38, 34, line, 1, st->picked ? AMBER : TEXT, BACK);
                }
                if (s->key_scale && !groove_pad_count(step_now.track)) {
                    text(232 - 6 * (unsigned)strlen(song_scale_names[s->key_scale]), 34, song_scale_names[s->key_scale], 1, DIM, BACK);
                    text(232 - 6 * (unsigned)(strlen(song_scale_names[s->key_scale]) + 1 + strlen(song_key_names[s->key_root])), 34,
                         song_key_names[s->key_root], 1, DIM, BACK);
                }
                step_grid(st);
            }
        }
    }
    step_playhead();
    if (!paint_rendering || (paint_top < 192 && paint_top + PAINT_ROWS > 170)) {
        if (cache->seq_cache != 0x57e9U + step_now.take) {
            cache->seq_cache = 0x57e9U + step_now.take;
            paint_fill(0, 168, 240, 24, BACK);
            text(8, 170, step_now.take ? "ALGO ZOOM  K1-K4 RECORD THEIR LANES" : "ENV DEL  LFO BACK DEL  ALGO ZOOM", 1, DIM, BACK);
            if (!step_now.take) text(8, 181, "SHIFT+ C# REST  D# TIE  A# REPEAT", 1, DIM, BACK);
        }
    }
    if (paint_rendering && paint_top + PAINT_ROWS <= 195) return;
    song_notice_expire();
    if (step_now.take) {
        song_knob_footer(step_now.track);
        song_hint(notice ? notice : step_take_hint());
        return;
    }
    unsigned shift = (previous_buttons & BUTTON(FX)) != 0, picking = (previous_buttons & BUTTON(SCL)) != 0;
    char length[8], velocity[12], gate[8];
    step_length_name(st, length);
    if (st->random_level) { strcpy(velocity, "R1"); velocity[1] = (char)('0' + st->random_level); }
    else { decimal(velocity, st->velocity, 1); strcat(velocity, " "); strcat(velocity, step_velocity_name(st->velocity)); }
    decimal(gate, step_gate_percent[st->gate], 1);
    strcat(gate, "%");
    if (song_glitch_layer)
        for (unsigned k = 0; k < 3; ++k) {
            unsigned field = song_glitch_pages[song_glitch_layer - 1][k];
            char value[12];
            song_glitch_value(st->glitch, field, value);
            knob(k, song_glitch_names[field], value, AMBER);
        }
    else {
        /* SHIFT: MOVE by the length, and NUDGE showing how far the pointer
         * sits from the nearest line of the length's grid (0.6.7). */
        char offset[12] = "+";
        if (shift) { int off = step_offset(st); decimal(offset + (off > 0), off, 1); }
        knob(0, shift ? "MOVE" : "LENGTH", length, colors[0]);
        knob(1, shift ? "NUDGE" : "VELOCITY", shift ? offset : velocity, colors[1]);
        knob(2, "GATE", gate, colors[2]);
    }
    char tune[12] = "< >";
    unsigned tuning = shift && groove_source_type(step_now.track) == GROOVE_SAMPLE; /* SHIFT+K4 on a sampler: TUNE (0.6.6) */
    if (tuning) song_glitch_value(st->glitch, GLITCH_TUNE, tune);
    knob(3, tuning ? "TUNE" : groove_pad_count(step_now.track) ? "PAD" : shift ? "OCTAVE" : "PITCH", tune, colors[3]); /* the pitch knob (0.5.2) */
    char page_hint[40] = "GLITCH 1/3  ARP NEXT";
    page_hint[7] = (char)('0' + song_glitch_layer);
    if (song_glitch_layer == 1) strcpy(page_hint, "GLITCH 1/3  SHIFT+K2 SCALE  ARP NEXT");
    uint32_t from, to;
    const char *hint = notice ? notice : song_glitch_layer ? page_hint : shift ? "SHIFT: F-E LENGTH  F# TRIP  G# DOT"
        : picking ? step_range(st, &from, &to) ? "SEL: KEYS TOGGLE PITCHES IN THE SWEEP" : "SEL: KEYS PICK  PRESETS SWEEPS"
        : step_clock != ~0U ? "PLAYING: KEYS ONLY SOUND  PLAY STOPS" : "SHIFT+REC LEAVES  PRESETS MOVES";
    song_hint(hint);
}
#ifdef FM1_OPEN_WEB
/* For the preview check (song_probe.inc): the roll's zoom, the glitch
 * page, what the screen shows, the sweep, and the song view's zoom (0.5.2),
 * which the footer no longer shows. */
uint32_t song_panel_probe(unsigned what);
uint32_t song_panel_probe(unsigned what)
{
    const struct song_step *st = song_port_step();
    switch (what) {
    case 0: return step_zoom;
    case 1: return song_glitch_layer;
    case 2: return song_shown(page);
    case 3: return st->ranged | (uint32_t)st->picked << 1;
    case 4: return st->anchor;
    case 5: return step_count(st);
    case 6: return song_zoom;
    case 7: return song_low[0] | (uint32_t)song_high[0] << 8; /* track 1's pitch range in the song view (0.6.3) */
    case 8: return song_all | song_window << 1; /* ALL zoom, and the window's start (0.6.4) */
    default: return 0;
    }
}
#endif

#include "song_step_input.inc"
