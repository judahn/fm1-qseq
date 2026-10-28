/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* song-mode: the step entry view. One bar of the track as a small piano
 * roll over a grid of 32nd cells (64ths when the bar needs them), notes at
 * their real lengths, the pointer, and the QY-style function labels.
 * Everything here changes only on input, so no frame snapshot is needed. */

enum { STEP_GX = 8, STEP_GW = 224, STEP_GY = 50, STEP_GH = 108, STEP_SHADE = 0x0861 };
/* The glitch layer: ARP toggles it, ALGO turns its three pages, and K1 to
 * K3 set the selected notes' glitch (or the entry's), K4 still moving. */
static uint8_t song_glitch_layer;
static const uint8_t song_glitch_pages[3][3] = {
    {GLITCH_REPEATS, GLITCH_DRIFT, GLITCH_FADE}, {GLITCH_CHANCE, GLITCH_OCTAVE, GLITCH_RANGE},
    {GLITCH_ACCENT, GLITCH_FLAM, GLITCH_SWAP}};

static unsigned step_x(uint32_t clock, uint32_t start, uint32_t length)
{
    if (clock <= start) return STEP_GX;
    return clock - start >= length ? STEP_GX + STEP_GW : STEP_GX + (clock - start) * STEP_GW / length;
}
/* Note names for the selection, as many as fit. */
static void step_names(const struct song_step *st, char out[40])
{
    unsigned index[STEP_CHORD], n = step_selection(st, index);
    out[0] = 0;
    if (!n) { strcpy(out, "EMPTY"); return; }
    for (unsigned i = 0; i < n && strlen(out) < 28; ++i) {
        char name[12];
        note_name(st->song->event[index[i]].a, name);
        if (i) strcat(out, " ");
        strcat(out, name);
    }
}
static void step_grid(const struct song_step *st)
{
    const struct song *s = st->song;
    uint32_t start, length;
    unsigned bar = song_bar_at(s, st->pointer, &start, &length), count;
    const struct song_event *e = song_track(s, st->track, &count);
    unsigned first = song_find(s, st->track, start), last = song_find(s, st->track, start + length);
    first = first > 16 ? first - 16 : 0; /* notes still sounding from the bar before */
    /* Pitch range and cell size from what is in the bar. */
    unsigned low = 127, high = 0, fine = (st->pointer - start) % 60U != 0;
    for (unsigned i = first; i < last; ++i) {
        uint32_t at = song_event_start(&e[i]);
        if (at + e[i].length <= start) continue;
        if (e[i].a < low) low = e[i].a;
        if (e[i].a > high) high = e[i].a;
        if (at >= start) fine |= (at - start) % 60U == 30U || (at + e[i].length - start) % 60U == 30U;
    }
    if (low > high) low = high = 60;
    while (high - low < 11) { if (high < 127) ++high; if (high - low < 11 && low > 0) --low; }
    unsigned rows = high - low + 1, row = STEP_GH / rows, tall = row > 2 ? row - 1 : row ? row : 1;
    uint32_t beat = SONG_WHOLE / song_meter_at(s, bar)->unit, cell = fine ? 30U : 60U;
    for (uint32_t b = 0; b < length; b += beat)
        paint_fill(step_x(start + b, start, length), STEP_GY, step_x(start + b + beat, start, length) -
                   step_x(start + b, start, length), STEP_GH, (b / beat) & 1U ? STEP_SHADE : BACK);
    for (unsigned p = low; p <= high; ++p) /* a faint line on each C */
        if (p % 12 == 0) paint_fill(STEP_GX, STEP_GY + (high - p) * STEP_GH / rows + row / 2, STEP_GW, 1, PANEL);
    /* The QY's dashes: one per cell, the pointer's cell lit. */
    for (uint32_t c = 0; c < length; c += cell) {
        unsigned x = step_x(start + c, start, length), w = step_x(start + c + cell, start, length) - x;
        unsigned on = st->pointer >= start + c && st->pointer < start + c + cell;
        paint_fill(x, STEP_GY + STEP_GH + 3, w > 1 ? w - 1 : 1, 2, on ? AMBER : c % beat ? PANEL : DIM);
    }
    unsigned index[STEP_CHORD], selected = step_selection(st, index);
    for (unsigned i = first; i < last; ++i) {
        uint32_t at = song_event_start(&e[i]);
        if (at + e[i].length <= start) continue;
        unsigned x = step_x(at, start, length), w = step_x(at + e[i].length, start, length) - x, chosen = 0;
        for (unsigned k = 0; k < selected; ++k) chosen |= index[k] == s->first[st->track] + i;
        paint_fill(x, STEP_GY + (high - e[i].a) * STEP_GH / rows, w > 1 ? w : 1, tall,
                   chosen ? TEXT : frame_views[st->track].audible ? colors[st->track] : DIM);
        if (song_event_glitch(&e[i])) /* glitched: a mark on its start */
            paint_fill(x, STEP_GY + (high - e[i].a) * STEP_GH / rows - 2, 2, 2, AMBER);
    }
    unsigned x = step_x(st->pointer, start, length);
    paint_fill(x, STEP_GY - 2, 1, STEP_GH + 2, AMBER);
    paint_fill(x > 2 ? x - 2 : 0, STEP_GY - 5, 5, 3, AMBER);
}

static void song_step_draw(void)
{
    const struct song_step *st = song_port_step();
    const struct song *s = st->song;
    char position[24], line[40];
    if (!paint_rendering || paint_top < 30) {
        song_position(s, st->pointer, position, 1);
        uint32_t sig = signature(st->track | (uint32_t)frame_views[st->track].type << 8, position, (unsigned)strlen(position));
        if (sig != cache->header_cache) {
            cache->header_cache = sig;
            paint_fill(0, 0, 240, 27, BACK);
            text(6, 6, position, 2, TEXT, BACK);
            paint_fill(124, 4, 30, 11, AMBER);
            text(127, 6, "STEP", 1, BACK, AMBER);
            text(124, 18, "T", 1, colors[st->track], BACK);
            number(130, 18, st->track + 1, 1, 1, colors[st->track], BACK);
            text(144, 18, source_names[frame_views[st->track].type], 1, DIM, BACK);
            const struct song_meter *m = song_meter_at(s, song_bar_at(s, st->pointer, NULL, NULL));
            number(206, 6, m->beats, 1, 1, DIM, BACK);
            text(206 + (m->beats > 9 ? 12 : 6), 6, "/", 1, DIM, BACK);
            number(212 + (m->beats > 9 ? 12 : 6), 6, m->unit, 1, 1, DIM, BACK);
        }
    }
    /* Selection line and the bar grid change together. */
    if (!paint_rendering || (paint_top < STEP_GY + STEP_GH + 6 && paint_top + PAINT_ROWS > 30)) {
        uint32_t sig = signature(st->pointer ^ (st->track << 28) ^ ((uint32_t)st->picked << 31), &s->revision, 4);
        sig = signature(sig, st->pick, sizeof st->pick) ^ (s->key_root | s->key_scale << 4);
        if (sig != cache->title_cache) {
            cache->title_cache = sig;
            paint_fill(0, 30, 240, STEP_GY + STEP_GH + 6 - 30, BACK);
            if (paint_rendering) {
                step_names(st, line);
                text(8, 34, st->picked ? "PICK" : "SEL", 1, DIM, BACK);
                text(38, 34, line, 1, st->picked ? AMBER : TEXT, BACK);
                if (s->key_scale && !groove_pad_count(st->track)) {
                    text(232 - 6 * (unsigned)strlen(song_scale_names[s->key_scale]), 34, song_scale_names[s->key_scale], 1, DIM, BACK);
                    text(232 - 6 * (unsigned)(strlen(song_scale_names[s->key_scale]) + 1 + strlen(song_key_names[s->key_root])), 34,
                         song_key_names[s->key_root], 1, DIM, BACK);
                }
                step_grid(st);
            }
        }
    }
    if (!paint_rendering || (paint_top < 192 && paint_top + PAINT_ROWS > 170)) {
        if (cache->seq_cache != 0x57e9U) {
            cache->seq_cache = 0x57e9U;
            paint_fill(0, 168, 240, 24, BACK);
            text(8, 170, "SCL DEL   ENV BACK DEL   LFO SELECT", 1, DIM, BACK);
            text(8, 181, "F5 REST   F#5 REPEAT     G5 TIE", 1, DIM, BACK);
        }
    }
    if (paint_rendering && paint_top + PAINT_ROWS <= 195) return;
    unsigned shift = (previous_buttons & BUTTON(FX)) != 0;
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
        knob(0, "LENGTH", length, colors[0]);
        knob(1, "VELOCITY", velocity, colors[1]);
        knob(2, "GATE", gate, colors[2]);
    }
    knob(3, shift ? "NOTE JUMP" : "MOVE", "< >", colors[3]);
    if (notice && (int32_t)(frame_time - notice_until) >= 0) notice = NULL;
    char page_hint[40] = "GLITCH 1/3  ALGO PAGE  ARP BACK";
    page_hint[7] = (char)('0' + song_glitch_layer);
    if (song_glitch_layer == 1) strcpy(page_hint, "GLITCH 1/3  SHIFT+K2 SCALE  ARP BACK");
    const char *hint = notice ? notice : song_glitch_layer ? page_hint : shift ? "SHIFT: F3-E4 LENGTH F#3 TRIP G#3 DOT"
        : (previous_buttons & BUTTON(LFO)) ? "HOLD LFO, PRESS KEYS TO PICK NOTES" : "REC OR PLAY TO LEAVE  PRESETS PITCH";
    uint32_t sig = signature(0, hint, (unsigned)strlen(hint));
    if (sig == cache->hint_cache) return;
    cache->hint_cache = sig;
    paint_fill(6, 231, 228, 7, BACK);
    text(6, 231, hint, 1, DIM, BACK);
}

/* Step entry input. Keys F3 to E5 enter notes, and a chord goes in when
 * every key is up. F5, F#5 and G5 rest, repeat and tie. SHIFT plus the lower
 * keys picks the length, and holding LFO picks notes in a chord. */
static uint32_t step_held; /* entry keys down for the chord being built */
static unsigned song_key_note(unsigned note);

static void song_step_enter(void)
{
    struct song_step *st = song_port_step();
    step_begin(st, track, song_port_player()->playhead);
    step_held = 0;
    page = music_page = SONG_STEP;
    cache->shown_page = ~0U;
    step_land(st);
}
/* The playhead follows the pointer out. PLAY plays from the pointer's bar. */
static void song_step_leave(unsigned play, uint32_t now)
{
    struct song_step *st = song_port_step();
    uint32_t start;
    step_preview_stop(st);
    song_bar_at(st->song, st->pointer, &start, NULL);
    song_port_jump(play ? start : st->pointer);
    page = music_page = SONG;
    cache->shown_page = ~0U;
    if (play) song_start(now);
}
static int song_step_input(const struct fm1_controls *c, uint32_t *pressed, uint32_t key_down, uint32_t now)
{
    struct song_step *st = song_port_step();
    unsigned shift = (c->buttons & BUTTON(FX)) != 0, picking = (c->buttons & BUTTON(LFO)) != 0;
    if (*pressed & (BUTTON(REC) | BUTTON(PLAY))) {
        song_step_leave((*pressed & BUTTON(PLAY)) != 0, now);
        *pressed &= ~(BUTTON(REC) | BUTTON(PLAY));
        return 1;
    }
    if (*pressed & BUTTON(SCL)) step_delete(st);
    if (*pressed & BUTTON(ENV)) step_back_delete(st);
    if (*pressed & BUTTON(ARP)) song_glitch_layer = song_glitch_layer ? 0 : 1;
    *pressed &= ~(BUTTON(FX) | BUTTON(SCL) | BUTTON(ENV) | BUTTON(LFO) | BUTTON(ARP));
    if (*pressed & (BUTTON(HOME) | BUTTON(EDIT) | BUTTON(GLO) | BUTTON(SAVE))) { step_preview_stop(st); return 0; }
    octave = clamp(octave + !!(*pressed & BUTTON(OCT_UP)) - !!(*pressed & BUTTON(OCT_DOWN)), -2, 2);
    if (c->encoders[0]) { select_track(c->encoders[0]); step_begin(st, track, st->pointer); step_held = 0; }
    if (song_glitch_layer) {
        song_glitch_layer = (uint8_t)clamp(song_glitch_layer + c->encoders[1], 1, 3);
        for (unsigned k = 0; k < 3; ++k) {
            unsigned field = song_glitch_pages[song_glitch_layer - 1][k];
            if (field == GLITCH_DRIFT && shift) field = GLITCH_SCALE;
            if (c->encoders[2 + k] && step_set_glitch(st, field, c->encoders[2 + k])) {
                notice = "GLITCH TABLE FULL";
                notice_until = now + 1500U;
            }
        }
    } else {
        if (c->encoders[2]) step_set_length(st, c->encoders[2]);
        if (c->encoders[3]) step_set_velocity(st, c->encoders[3]);
        if (c->encoders[4]) step_set_gate(st, c->encoders[4]);
    }
    if (c->encoders[5]) {
        if (shift) step_jump(st, c->encoders[5] > 0 ? 1 : -1);
        else step_move(st, c->encoders[5]);
    }
    if (c->encoders[6]) { /* the pitch knob: scale steps, octaves with SHIFT, pads on drums */
        unsigned pads = groove_pad_count(track);
        uint8_t notes[GROOVE_PADS];
        for (unsigned i = 0; i < pads && i < GROOVE_PADS; ++i) notes[i] = (uint8_t)groove_pad_note(track, i);
        if (pads) step_transpose_pads(st, notes, pads < GROOVE_PADS ? pads : GROOVE_PADS, c->encoders[6]);
        else step_transpose(st, c->encoders[6], shift);
    }
    uint32_t down = key_down & ~blocked_keys;
    if (shift) {
        for (unsigned i = 0; i < STEP_LENGTHS; ++i)
            if (down & (1U << white_keys[i])) step_pick_length(st, i);
        if (down & (1U << 1)) step_toggle_triplet(st);
        if (down & (1U << 3)) step_cycle_dots(st);
        blocked_keys |= key_down;
    } else {
        if (down & (1U << 24)) step_rest(st);
        if ((down & (1U << 25)) && step_repeat(st)) { notice = "NOTHING TO REPEAT OR SONG FULL"; notice_until = now + 1500U; }
        if ((down & (1U << 26)) && step_tie(st)) { notice = "TIE ONLY RIGHT AFTER A NOTE"; notice_until = now + 1500U; }
        blocked_keys |= key_down & (7U << 24);
        for (unsigned key = 0; key < 24; ++key)
            if (down & (1U << key)) {
                unsigned pitch = song_key_note(groove_key_note(track, key, octave));
                if (pitch > 127) continue;
                if (picking) step_pick(st, pitch);
                else { step_key(st, pitch); step_held |= 1U << key; }
            }
    }
    step_held &= c->keys;
    if (!step_held && st->chord_count && step_commit(st)) {
        notice = "SONG FULL - NOTE NOT ENTERED";
        notice_until = now + 2000U;
    }
    return 1;
}
