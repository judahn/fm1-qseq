/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* song-mode: what the song view shares with the automation view: reading
 * the transport, the header (position, transport or REC, the loop, the
 * marker, the time signature and the tempo), the playhead and the footer
 * knobs while recording. Included by song_view.inc. */

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
    int marker = s->open ? -1 : song_marker_step(s, song_now.clock + 1U, -1);
    sig = song_now.playing | (s->loop_on << 1) | (song_now.jumping << 2) | ((uint32_t)(marker + 1) << 3) |
          ((uint32_t)bpm << 8) | ((uint32_t)s->open << 23) | ((uint32_t)song_now.recording << 31);
    sig = signature(sig, m, sizeof *m);
    if (sig == cache->header_cache) return;
    cache->header_cache = sig;
    paint_fill(122, 0, 118, 27, BACK);
    if (song_now.recording) { paint_fill(122, 3, 22, 11, 0xf800); text(124, 5, "REC", 1, TEXT, 0xf800); }
    else text(124, 5, song_now.playing ? "PLAY" : "STOP", 1, song_now.playing ? GREEN : DIM, BACK);
    text(124, 16, "LOOP", 1, s->loop_on ? ACCENT : PANEL, BACK);
    char name[SONG_MARKER_NAME + 1] = {0};
    if (s->open) { strcpy(name, "CLIP "); decimal(name + 5, (s->open - SONG_TRACKS) % SONG_GRID_ROWS + 1, 1); } /* its row */
    else if (marker >= 0) memcpy(name, s->marker[marker].name, SONG_MARKER_NAME);
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
/* The marking pass reads the transport once for every band. */
static void song_now_read(struct song *s)
{
    const struct song_player *p = song_port_player();
    song_now.clock = song_player_clock(p);
    song_now.target = song_player_target(p);
    song_now.playing = p->playing;
    song_now.jumping = p->jumping;
    song_now.shift = (previous_buttons & BUTTON(FX)) != 0;
    song_now.recording = song_port_recorder()->active;
    song_now.selected = (uint8_t)song_selected();
    const struct song_clip *c = song_port_clip();
    unsigned single = c->tracks && !(c->tracks & (c->tracks - 1U));
    song_now.ghost_tracks = (uint8_t)(c->length && !p->playing ? single ? 1U << track : c->tracks : 0);
    song_now.ghost_from = p->playhead;
    song_now.ghost_to = p->playhead + c->length;
    song_now.start = p->start_at < song_end(s) ? p->start_at : song_end(s);
    song_follow(s, song_now.clock);
    song_ranges(s);
}
/* The playhead: the marking pass damages its old and new columns. */
static void song_playhead_line(void)
{
    unsigned x = song_visible(song_now.clock) ? song_x(song_now.clock) : ~0U;
    if (!paint_rendering && x != song_drawn_x) {
        if (song_drawn_x != ~0U) paint_fill(song_drawn_x, SONG_RULER, 1, SONG_BOTTOM - SONG_RULER, BACK);
        if (x != ~0U) paint_fill(x, SONG_RULER, 1, SONG_BOTTOM - SONG_RULER, GREEN);
        song_drawn_x = x;
    } else if (paint_rendering && song_drawn_x != ~0U)
        paint_fill(song_drawn_x, SONG_RULER, 1, SONG_BOTTOM - SONG_RULER, GREEN);
}
/* The four recording knobs, by parameter name, on the take track. */
static void song_knob_footer(unsigned t)
{
    unsigned engine = song_port_engine(t);
    for (unsigned k = 0; k < 4; ++k) {
        char value[12];
        const struct song_param *p = song_param_for(engine, song_knob_lane(t, k));
        decimal(value, (int)song_knob_value(t, k), 1);
        knob(k, p ? p->name : "-", value, song_knob_held(k) ? AMBER : colors[k]);
    }
}
/* The footer: snap, loop and zoom, or the recording knobs during a take. */
static void song_footer(const struct song *s, const char *shift_k1)
{
    if (song_now.recording) { song_knob_footer(song_port_recorder()->track); return; }
    char start[24], end[24];
    song_position(s, s->loop_start, start, 0);
    song_position(s, s->loop_end, end, 0);
    knob(0, song_now.shift ? shift_k1 : "SNAP", song_now.shift ? "< >" : song_snaps[song_zoom], colors[0]);
    knob(1, "LOOP IN", start, colors[1]);
    knob(2, s->open ? "CLIP END" : "LOOP OUT", end, colors[2]);
    knob(3, "ZOOM", song_spans[song_zoom], colors[3]);
}
/* Memory, compact: flash (songs, then kits) and the song's RAM pool. */
static void song_memory_bars(void)
{
    if (paint_rendering && paint_top >= 30) return;
    struct song_gauge g;
    song_store_gauge(&g);
    uint32_t sig = (uint32_t)((uint64_t)g.songs * 44U / g.pool) | (uint32_t)((uint64_t)g.samples * 44U / g.pool) << 8 |
                   (uint32_t)(g.events * 34U / SONG_EVENTS) << 16 | 0x80000000U;
    if (sig == cache->seq_cache) return;
    cache->seq_cache = sig;
    unsigned songs = sig & 255U, samples = sig >> 8 & 255U, events = sig >> 16 & 255U;
    paint_fill(124, 24, 44, 2, PANEL);
    paint_fill(124, 24, songs, 2, GREEN);
    paint_fill(124 + songs, 24, samples, 2, AMBER);
    paint_fill(172, 24, 34, 2, PANEL);
    paint_fill(172, 24, events, 2, ACCENT);
}
