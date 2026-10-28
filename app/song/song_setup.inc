/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* song-mode: the song settings page (design section 8a). Starting tempo and
 * time signature, swing and key. ALGO turns between the two knob pages, and
 * the first eight white keys switch swing per track. */

static unsigned song_setup_page, song_setup_back = SONG, song_glo_from;
static void song_tempo_text(const struct song *s, char out[24])
{
    char part[12];
    decimal(out, s->tempo[0].bpm100 / 100, 1);
    if (s->tempo[0].bpm100 % 100) { strcat(out, "."); decimal(part, s->tempo[0].bpm100 % 100, 2); strcat(out, part); }
}
static void song_meter_text(const struct song *s, char out[24])
{
    char part[12];
    decimal(out, s->meter[0].beats, 1); strcat(out, "/"); decimal(part, s->meter[0].unit, 1); strcat(out, part);
}

static void song_setup_row(unsigned y, const char *label, const char *value, unsigned live)
{
    text(8, y, label, 1, live ? ACCENT : DIM, BACK);
    text(84, y - 3, value, 2, TEXT, BACK);
}
static void song_setup_draw(void)
{
    const struct song *s = song_port_song();
    if ((!paint_rendering || paint_top < 30) && cache->header_cache != 0x5e7U) {
        cache->header_cache = 0x5e7U;
        paint_fill(0, 0, 240, 27, BACK);
        text(8, 7, "SONG SETTINGS", 2, ACCENT, BACK);
    }
    unsigned page2 = song_setup_page != 0;
    uint32_t sig = signature(page2 | (uint32_t)s->metronome << 4, &s->swing, 5) ^ s->tempo[0].bpm100 ^ ((uint32_t)s->meter[0].beats << 16) ^
                   ((uint32_t)s->meter[0].unit << 24);
    if (!paint_rendering || (paint_top < 192 && paint_top + PAINT_ROWS > 30)) {
        if (sig != cache->middle_cache) {
            cache->middle_cache = sig;
            paint_fill(0, 30, 240, 162, BACK);
            if (paint_rendering) {
                char value[24];
                song_tempo_text(s, value);
                song_setup_row(40, "TEMPO", value, !page2);
                song_meter_text(s, value);
                song_setup_row(66, "TIME SIG", value, !page2);
                decimal(value, s->swing, 1); strcat(value, "% "); strcat(value, s->swing_grid ? "16TH" : "8TH");
                song_setup_row(92, "SWING", s->swing ? value : "OFF", !page2);
                text(8, 118, "SWING ON", 1, DIM, BACK);
                for (unsigned t = 0; t < SONG_TRACKS; ++t) {
                    unsigned on = s->swing_tracks >> t & 1U, x = 84 + t * 19;
                    paint_fill(x, 114, 16, 15, on ? colors[t] : PANEL);
                    number(x + 5, 118, (int)t + 1, 1, 1, on ? BACK : DIM, on ? colors[t] : PANEL);
                }
                strcpy(value, song_key_names[s->key_root]);
                song_setup_row(146, "KEY", s->key_scale ? value : "-", page2);
                text(84, 168, song_scale_names[s->key_scale], 2, s->key_scale ? TEXT : DIM, BACK);
                text(8, 171, "SCALE", 1, page2 ? ACCENT : DIM, BACK);
            }
        }
    }
    if (paint_rendering && paint_top + PAINT_ROWS <= 195) return;
    static const char *const labels[2][4] = {{"TEMPO", "TIME SIG", "SWING", "GRID"}, {"KEY", "SCALE", "METRONOME", "-"}};
    char values[4][24];
    if (!page2) {
        song_tempo_text(s, values[0]);
        song_meter_text(s, values[1]);
        decimal(values[2], s->swing, 1); strcat(values[2], "%");
        strcpy(values[3], s->swing_grid ? "16TH" : "8TH");
    } else {
        strcpy(values[0], song_key_names[s->key_root]);
        strcpy(values[1], song_scale_names[s->key_scale]);
        static const char *const clicks[3] = {"OFF", "ON", "REC"};
        strcpy(values[2], clicks[s->metronome % 3U]); strcpy(values[3], "--");
    }
    for (unsigned col = 0; col < 4; ++col) knob(col, labels[page2][col], values[col], colors[col]);
    const char *hint = page2 ? "ALGO PAGE  GLO BACK" : "KEYS 1-8 SWING TRACKS  ALGO PAGE";
    if (cache->hint_cache != page2 + 1U) {
        cache->hint_cache = page2 + 1U;
        paint_fill(6, 231, 228, 7, BACK);
        text(6, 231, hint, 1, DIM, BACK);
    }
}

static int song_setup_input(const struct fm1_controls *c, uint32_t *pressed, uint32_t key_down, uint32_t now)
{
    struct song *s = song_port_song();
    struct song_player *p = song_port_player();
    unsigned shift = (c->buttons & BUTTON(FX)) != 0;
    if (*pressed & BUTTON(PLAY)) { if (p->playing) song_port_stop(); else song_start(now); }
    *pressed &= ~(BUTTON(PLAY) | BUTTON(FX) | BUTTON(REC) | BUTTON(ARP) | BUTTON(ENV) | BUTTON(SCL) | BUTTON(LFO));
    if (*pressed & (BUTTON(HOME) | BUTTON(EDIT) | BUTTON(GLO) | BUTTON(SAVE))) return 0;
    song_setup_page = (unsigned)clamp((int)song_setup_page + c->encoders[1], 0, 1);
    const int16_t *k = c->encoders + 2;
    if (!song_setup_page) {
        if (k[0]) /* whole BPM, or hundredths with SHIFT */
            (void)song_port_set_tempo(0, (unsigned)clamp(s->tempo[0].bpm100 + k[0] * (shift ? 1 : 100),
                                                               SONG_TEMPO_MIN, SONG_TEMPO_MAX));
        if (k[1]) {
            static const uint8_t units[3] = {4, 8, 16};
            unsigned unit = s->meter[0].unit, beats = s->meter[0].beats, u = unit == 4 ? 0 : unit == 8 ? 1 : 2;
            if (shift) unit = units[clamp((int)u + k[1], 0, 2)];
            else beats = (unsigned)clamp((int)beats + k[1], 1, 16);
            if (unit == 4 && beats > 8) beats = 8;
            (void)song_set_meter(s, 0, beats, unit);
        }
        s->swing = (uint8_t)clamp(s->swing + k[2], 0, 50);
        if (k[3]) s->swing_grid = k[3] > 0;
    } else {
        if (k[0]) s->key_root = (uint8_t)((s->key_root + 12 + k[0] % 12) % 12);
        s->key_scale = (uint8_t)clamp(s->key_scale + k[1], 0, SONG_SCALES - 1);
        s->metronome = (uint8_t)clamp(s->metronome + k[2], 0, 2);
    }
    for (unsigned t = 0; t < SONG_TRACKS; ++t)
        if ((key_down & ~blocked_keys) & (1U << white_keys[t])) s->swing_tracks ^= (uint8_t)(1U << t);
    blocked_keys |= key_down; /* these keys are switches here */
    return 1;
}
