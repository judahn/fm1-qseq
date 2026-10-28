/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* song-mode: the song view's header. Position, transport or REC, the loop,
 * the marker, the time signature and the tempo. Included by song_view.inc. */

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
          ((uint32_t)bpm << 8) | ((uint32_t)song_now.recording << 31);
    sig = signature(sig, m, sizeof *m);
    if (sig == cache->header_cache) return;
    cache->header_cache = sig;
    paint_fill(122, 0, 118, 27, BACK);
    if (song_now.recording) { paint_fill(122, 3, 22, 11, 0xf800); text(124, 5, "REC", 1, TEXT, 0xf800); }
    else text(124, 5, song_now.playing ? "PLAY" : "STOP", 1, song_now.playing ? GREEN : DIM, BACK);
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
