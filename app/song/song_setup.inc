/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* song-mode: the song settings page (design section 8a). Starting tempo and
 * time signature, swing, key, the metronome and the clips' launch quantize
 * (8f). ALGO turns between the knob pages, and
 * the first eight white keys switch swing per track. The third page picks
 * the parameter each of K1 to K4 records on a track, the fourth sets the
 * track's humanize, and the fifth its chord mode (8g), SELECT for the track
 * on all three. */

static unsigned song_setup_page, song_setup_back = SONG, song_glo_from;
static const char *const song_launch_names[5] = {"OFF", "BEAT", "BAR", "2 BARS", "4 BARS"};
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
/* Page 4: humanize for the current track, up to 30 clocks and 30 steps. */
static void song_humanize_draw(void)
{
    const struct song *s = song_port_song();
    char time[12], velocity[12];
    strcpy(time, "+-"); decimal(time + 2, s->humanize_time[track], 1);
    strcpy(velocity, "+-"); decimal(velocity + 2, s->humanize_velocity[track], 1);
    uint32_t sig = 0x4a5eU ^ track << 4 ^ (uint32_t)s->humanize_time[track] << 8 ^ (uint32_t)s->humanize_velocity[track] << 16;
    if ((!paint_rendering || (paint_top < 192 && paint_top + PAINT_ROWS > 30)) && sig != cache->middle_cache) {
        cache->middle_cache = sig;
        paint_fill(0, 30, 240, 162, BACK);
        if (paint_rendering) {
            text(8, 40, "HUMANIZE", 1, DIM, BACK);
            text(92, 37, "T", 2, colors[track], BACK);
            number(106, 37, (int)track + 1, 1, 2, colors[track], BACK);
            song_setup_row(76, "TIMING", time, 1);
            text(84, 96, "CLOCKS EARLY OR LATE", 1, DIM, BACK);
            song_setup_row(128, "VELOCITY", velocity, 1);
            text(84, 148, "EACH NOTE, AT RANDOM", 1, DIM, BACK);
        }
    }
    if (paint_rendering && paint_top + PAINT_ROWS <= 195) return;
    knob(0, "TIMING", time, colors[0]);
    knob(1, "VELOCITY", velocity, colors[1]);
    knob(2, "-", "--", colors[2]);
    knob(3, "-", "--", colors[3]);
    song_hint("SELECT TRACK  ALGO PAGE  GLO BACK");
}
/* Page 5: chord mode for the current track (design 8g). The type is picked
 * on the keys, F4 to E5. */
static void song_chords_draw(void)
{
    const struct song *s = song_port_song();
    static const char *const voicings[CHORD_VOICINGS] = {"CLOSE", "SPREAD", "SMOOTH"};
    static const char *const strums[CHORD_STRUMS] = {"OFF", "1/64", "1/32", "1/16", "1/8"};
    static const char *const directions[CHORD_DIRECTIONS] = {"UP", "DOWN", "ALTERNATE", "RANDOM"};
    unsigned drums = groove_pad_count(track) != 0;
    const char *v[4] = {drums ? "DRUMS" : song_chord_on(s, track) ? "ON" : "OFF", voicings[song_chord_voicing(s, track) % 3U],
                        strums[song_chord_strum_of(s, track) % 5U], directions[song_chord_direction(s, track)]};
    uint32_t sig = 0xc402U ^ track << 4 ^ (uint32_t)s->chord[track] << 8 ^ (uint32_t)s->chord_type[track] << 16 ^ drums << 24;
    if ((!paint_rendering || (paint_top < 192 && paint_top + PAINT_ROWS > 30)) && sig != cache->middle_cache) {
        cache->middle_cache = sig;
        paint_fill(0, 30, 240, 162, BACK);
        if (paint_rendering) {
            static const char *const labels[4] = {"MODE", "VOICING", "STRUM", "DIRECTION"};
            text(8, 40, "CHORDS", 1, DIM, BACK);
            text(92, 37, "T", 2, colors[track], BACK);
            number(106, 37, (int)track + 1, 1, 2, colors[track], BACK);
            for (unsigned k = 0; k < 4; ++k) song_setup_row(68 + k * 24, labels[k], v[k], !drums);
            text(8, 170, "TYPE", 1, DIM, BACK);
            text(84, 170, song_chord_names[s->chord_type[track] % CHORD_TYPES], 1, TEXT, BACK);
        }
    }
    if (paint_rendering && paint_top + PAINT_ROWS <= 195) return;
    knob(0, "MODE", v[0], colors[0]);
    knob(1, "VOICING", v[1], colors[1]);
    knob(2, "STRUM", v[2], colors[2]);
    knob(3, "DIRECTION", v[3], colors[3]);
    song_hint("SELECT TRACK  F4-E5 PICK THE TYPE");
}
/* Page 3: the recording knobs of the current track. */
static void song_knobs_draw(void)
{
    unsigned engine = song_port_engine(track);
    uint32_t sig = 0x6b0bU ^ track;
    for (unsigned k = 0; k < 4; ++k) sig = sig * 131U + song_knob_lane(track, k);
    if ((!paint_rendering || (paint_top < 192 && paint_top + PAINT_ROWS > 30)) && sig != cache->middle_cache) {
        cache->middle_cache = sig;
        paint_fill(0, 30, 240, 162, BACK);
        if (paint_rendering) {
            static const char *const engines[SONG_ENGINES] = {"DX7", "303", "808", "PSG", "PLUCK", "SAMPLER"};
            text(8, 40, "RECORD KNOBS", 1, DIM, BACK);
            text(92, 37, "T", 2, colors[track], BACK);
            number(106, 37, (int)track + 1, 1, 2, colors[track], BACK);
            text(130, 40, engines[engine < SONG_ENGINES ? engine : SONG_SAMPLER], 1, TEXT, BACK);
            for (unsigned k = 0; k < 4; ++k) {
                const struct song_param *p = song_param_for(engine, song_knob_lane(track, k));
                text(8, 68 + k * 26, "K", 1, colors[k], BACK);
                number(14, 68 + k * 26, (int)k + 1, 1, 1, colors[k], BACK);
                text(40, 65 + k * 26, p ? p->name : "-", 2, TEXT, BACK);
            }
        }
    }
    if (paint_rendering && paint_top + PAINT_ROWS <= 195) return;
    for (unsigned k = 0; k < 4; ++k) {
        const struct song_param *p = song_param_for(engine, song_knob_lane(track, k));
        char name[12] = "K";
        decimal(name + 1, (int)k + 1, 1);
        knob(k, name, p ? p->name : "-", colors[k]);
    }
    song_hint("SELECT TRACK  K1-K4 PARAMETER");
}
static void song_setup_draw(void)
{
    const struct song *s = song_port_song();
    if ((!paint_rendering || paint_top < 30) && cache->header_cache != 0x5e7U) {
        cache->header_cache = 0x5e7U;
        paint_fill(0, 0, 240, 27, BACK);
        text(8, 7, "SONG SETTINGS", 2, ACCENT, BACK);
    }
    if (song_setup_page == 2) { song_knobs_draw(); return; }
    if (song_setup_page == 3) { song_humanize_draw(); return; }
    if (song_setup_page == 4) { song_chords_draw(); return; }
    unsigned page2 = song_setup_page != 0;
    uint32_t sig = signature(page2 | (uint32_t)s->metronome << 4, &s->swing, 5) ^ s->tempo[0].bpm100 ^ ((uint32_t)s->meter[0].beats << 16) ^
                   ((uint32_t)s->meter[0].unit << 24);
    if (!paint_rendering || (paint_top < 192 && paint_top + PAINT_ROWS > 30)) {
        if (sig != cache->middle_cache) {
            cache->middle_cache = sig;
            paint_fill(0, 30, 240, 162, BACK);
            if (paint_rendering) {
                char value[24];
                song_bpm_text(s->tempo[0].bpm100, 0, value);
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
    static const char *const labels[2][4] = {{"TEMPO", "TIME SIG", "SWING", "GRID"}, {"KEY", "SCALE", "METRONOME", "LAUNCH Q"}};
    char values[4][24];
    if (!page2) {
        song_bpm_text(s->tempo[0].bpm100, (previous_buttons & BUTTON(FX)) != 0, values[0]); /* SHIFT: half BPM (0.5.2) */
        song_meter_text(s, values[1]);
        decimal(values[2], s->swing, 1); strcat(values[2], "%");
        strcpy(values[3], s->swing_grid ? "16TH" : "8TH");
    } else {
        strcpy(values[0], song_key_names[s->key_root]);
        strcpy(values[1], song_scale_names[s->key_scale]);
        static const char *const clicks[3] = {"OFF", "ON", "REC"};
        strcpy(values[2], clicks[s->metronome % 3U]); strcpy(values[3], song_launch_names[s->launch_q % 5U]);
    }
    for (unsigned col = 0; col < 4; ++col) knob(col, labels[page2][col], values[col], colors[col]);
    const char *hint = page2 ? "ALGO PAGE  GLO BACK" : "KEYS 1-8 SWING TRACKS  ALGO PAGE";
    song_hint(hint);
}

static int song_setup_input(const struct fm1_controls *c, uint32_t *pressed, uint32_t key_down, uint32_t now)
{
    struct song *s = song_port_song();
    struct song_player *p = song_port_player();
    unsigned shift = (c->buttons & BUTTON(FX)) != 0;
    if (*pressed & BUTTON(PLAY)) { if (p->playing) song_port_stop(); else song_start(now); }
    *pressed &= ~(BUTTON(PLAY) | BUTTON(FX) | BUTTON(REC) | BUTTON(ARP) | BUTTON(ENV) | BUTTON(SCL) | BUTTON(LFO));
    if (*pressed & (BUTTON(HOME) | BUTTON(EDIT) | BUTTON(GLO) | BUTTON(SAVE))) return 0;
    song_setup_page = (unsigned)clamp((int)song_setup_page + c->encoders[1], 0, 4);
    const int16_t *k = c->encoders + 2;
    if (song_setup_page == 4) {
        select_track(c->encoders[0]);
        s->chord[track] = song_chord_setting((unsigned)clamp((int)song_chord_on(s, track) + k[0], 0, 1),
                                             (unsigned)clamp((int)song_chord_voicing(s, track) + k[1], 0, CHORD_VOICINGS - 1),
                                             (unsigned)clamp((int)song_chord_strum_of(s, track) + k[2], 0, CHORD_STRUMS - 1),
                                             (unsigned)clamp((int)song_chord_direction(s, track) + k[3], 0, CHORD_DIRECTIONS - 1));
    } else if (song_setup_page == 3) {
        select_track(c->encoders[0]);
        s->humanize_time[track] = (uint8_t)clamp(s->humanize_time[track] + k[0], 0, 30);
        s->humanize_velocity[track] = (uint8_t)clamp(s->humanize_velocity[track] + k[1], 0, 30);
    } else if (song_setup_page == 2) {
        select_track(c->encoders[0]);
        const uint8_t *index;
        unsigned n = song_params(song_port_engine(track), &index);
        for (unsigned i = 0; i < 4; ++i) {
            if (!k[i]) continue;
            unsigned at = 0, cc = song_knob_lane(track, i);
            while (at < n && song_param(index[at])->cc != cc) ++at;
            s->knob_cc[track][i] = song_param(index[clamp((int)at + k[i], 0, (int)n - 1)])->cc;
        }
    } else if (!song_setup_page) {
        if (k[0]) /* whole BPM, or half with SHIFT (0.5.2, hundredths before) */
            (void)song_port_set_tempo(0, song_tempo_step(s->tempo[0].bpm100, k[0], shift));
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
        s->launch_q = (uint8_t)clamp(s->launch_q + k[3], 0, 4);
    }
    for (unsigned t = 0; t < SONG_TRACKS && song_setup_page < 2; ++t)
        if ((key_down & ~blocked_keys) & (1U << white_keys[t])) s->swing_tracks ^= (uint8_t)(1U << t);
    blocked_keys |= key_down; /* these keys are switches here */
    return 1;
}
