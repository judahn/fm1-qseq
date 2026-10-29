/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* song-mode: the edit job page's screen, input and running, included by
 * song_jobs.inc. */

static void song_job_open(void)
{
    if (!song_job_ready) {
        for (unsigned j = 0; j < SONG_JOBS; ++j)
            for (unsigned p = 0; p < song_jobs[j].count; ++p) song_job_value[j][p] = song_job_param(j, p)->init;
        song_job_value[JOB_COPY][0] = (int16_t)track;
        song_job_ready = 1;
    }
    page = music_page = SONG_JOB;
    song_ask_close();
    cache->shown_page = ~0U;
}

static void song_job_draw(void)
{
    const struct song_job_def *d = &song_jobs[song_job];
    char where[40], value[16];
    song_job_where(where);
    uint32_t sig = signature(song_job | song_job_page << 8 | (uint32_t)track << 12, song_job_value[song_job], 12);
    sig = signature(sig, where, (unsigned)strlen(where)) ^ (uint32_t)song_port_engine(track) << 20 ^ song_ask_signature();
    if ((!paint_rendering || paint_top < 192) && sig != cache->middle_cache) {
        cache->middle_cache = sig;
        paint_fill(0, 0, 240, 192, BACK);
        if (paint_rendering) {
            text(8, 4, "JOB", 1, DIM, BACK);
            number(28, 4, (int)song_job + 1, 2, 1, DIM, BACK);
            text(8, 14, d->name, 2, ACCENT, BACK);
            text(8, 34, where, 1, TEXT, BACK);
            for (unsigned p = 0; p < d->count; ++p) {
                unsigned y = 52 + p * 21, live = p / 3U == song_job_page;
                char label[12] = "K";
                decimal(label + 1, (int)(p % 3U) + 2, 1);
                if (live) text(8, y + 3, label, 1, colors[p % 3U + 1U], BACK);
                text(24, y + 3, song_job_param(song_job, p)->name, 1, live ? TEXT : DIM, BACK);
                song_job_text(p, value);
                text(96, y, value, 2, live ? TEXT : DIM, BACK);
            }
            if (!d->count) text(24, 55, "NOTHING TO SET", 1, DIM, BACK);
            text(8, 182, d->about, 1, DIM, BACK);
            song_ask_draw();
        }
    }
    if (paint_rendering && paint_top + PAINT_ROWS <= 195) return;
    knob(0, "JOB", d->name, colors[0]);
    for (unsigned k = 0; k < 3; ++k) {
        unsigned p = song_job_page * 3U + k;
        if (p < d->count) { song_job_text(p, value); knob(k + 1U, song_job_param(song_job, p)->name, value, colors[k + 1U]); }
        else knob(k + 1U, "-", "", colors[k + 1U]);
    }
    song_notice_expire();
    const char *hint = notice ? notice : (previous_buttons & BUTTON(LFO)) ? "LFO + F3-F4 TRACKS  G4 ALL"
        : song_asking() ? "OCT+ RUN IT   OCT- CANCEL" : d->count > 3 ? "OCT+ RUN  ALGO MORE  HOME BACK" : "OCT+ RUN  HOME BACK  LFO TRACKS";
    song_hint(hint);
}

/* The job, with its settings, on the range. Below -1: a reason of its own. */
static int song_job_go(void)
{
    struct song *s = song_port_song();
    const int16_t *v = song_job_value[song_job];
    struct song_range r = song_job_range();
    /* In a clip, nothing that reaches other tracks or the song's time. */
    if (s->open && ((1U << JOB_PASTE | 1U << JOB_EXTRACT | 1U << JOB_STRETCH | 1U << JOB_COPY | 1U << JOB_MERGE |
                     1U << JOB_EXPAND) >> song_job & 1U ||
                    (song_job == JOB_RAMP && song_job_lane(v[1]) == SONG_JOB_TEMPO))) return -4;
    switch (song_job) {
    case JOB_QUANTIZE: return song_job_quantize(s, &r, song_job_grid(v[1]), (unsigned)v[2], (unsigned)v[3], (unsigned)v[4]);
    case JOB_VELOCITY: return song_job_velocity(s, &r, (unsigned)v[1], (unsigned)(v[1] ? v[2] : clamp(v[2], 1, 127)), v[3]);
    case JOB_CRESCENDO: return song_job_crescendo(s, &r, v[1]);
    case JOB_GATE: return song_job_gate(s, &r, (unsigned)v[1], (unsigned)(v[1] == SONG_JOB_SET && !v[2] ? 1 : v[2]), v[3]);
    case JOB_TRANSPOSE: return song_job_transpose(s, &r, v[1], (unsigned)v[2], (unsigned)v[3], (unsigned)v[4]);
    case JOB_FIT: return s->key_scale ? song_job_fit_scale(s, &r) : -2;
    case JOB_SHIFT: return song_job_shift(s, &r, v[1]);
    case JOB_SORT: return song_job_chord_sort(s, &r, (unsigned)v[1]);
    case JOB_STRUM: return song_job_strum(s, &r, (unsigned)v[1], (unsigned)v[2]);
    case JOB_PASTE:
        return song_port_clip()->length ? song_job_paste(s, song_port_clip(), song_port_player()->playhead, track,
                                                         (unsigned)v[0], v[1], song_drums()) : -3;
    case JOB_ERASE: return song_job_erase(s, &r, v[1] == 3 ? 7U : 1U << v[1]);
    case JOB_EXTRACT: return song_job_extract(s, &r, 1U << v[1], (unsigned)v[2]);
    case JOB_RAMP: {
        unsigned lane = song_job_lane(v[1]), tempo = lane == SONG_JOB_TEMPO;
        int low = tempo ? 20 : 0, high = tempo ? 300 : 127;
        return song_job_ramp(s, &r, lane, clamp(v[2], low, high), clamp(v[3], low, high), 30U << v[4], v[5]);
    }
    case JOB_THIN: return song_job_thin(s, &r, song_job_lane(v[1]));
    case JOB_STRETCH: return song_job_stretch(s, &r, (unsigned)v[1]);
    case JOB_COPY: {
        int n = v[2] == 1 ? 0 : song_job_copy_track(s, (unsigned)v[0], (unsigned)v[1]);
        uint8_t before[SONG_SOUND_BYTES], sound[SONG_SOUND_BYTES];
        if (n < 0 || !v[2] || v[0] == v[1]) return v[0] == v[1] ? -1 : n;
        if (groove_timbre_export((unsigned)v[1], before, sizeof before) || groove_timbre_export((unsigned)v[0], sound, sizeof sound) ||
            groove_timbre_import((unsigned)v[1], sound, sizeof sound)) return n ? n : -1;
        song_undo_sound(song_port_undo(), (unsigned)v[1], before, sound);
        return n + 1;
    }
    case JOB_MERGE: return song_job_merge(s, (unsigned)v[0], (unsigned)v[1]);
    case JOB_CLEAR: return song_job_clear(s, r.tracks);
    case JOB_EXPAND: return song_job_expand(s, &r);
    default: song_port_ready_pads(); return song_job_normalize(s, r.tracks, song_port_player());
    }
}
/* YES asks first, with what the job takes on and whether undo can hold it.
 * YES again runs it: one undo step, and playing goes on in time. */
static void song_job_yes(uint32_t now)
{
    if (!song_asking()) {
        const struct song *s = song_port_song();
        const struct song_range r = song_job_range();
        unsigned n = 0;
        char asked[SONG_ASK_CHARS + 1], part[12];
        for (unsigned t = 0; t < SONG_TRACKS; ++t)
            if (r.tracks >> t & 1U) n += song_find(s, t, r.to) - song_find(s, t, r.from);
        strcpy(asked, song_jobs[song_job].name); strcat(asked, " ");
        decimal(part, (int)n, 1); strcat(asked, part); strcat(asked, n == 1 ? " EVENT?" : " EVENTS?");
        unsigned big = n * 24U + 512U > SONG_UNDO_BYTES;
        song_ask(asked, big ? "TOO BIG TO UNDO" : NULL, big);
        return;
    }
    static char done[40];
    struct song_place place;
    song_player_hold(song_port_player(), &place);
    song_undo_step(song_port_undo());
    int r = song_job_go();
    song_player_resume(song_port_player(), &place);
    song_ask_close();
    if (r > 0) {
        char part[12];
        strcpy(done, song_jobs[song_job].name); strcat(done, ": ");
        decimal(part, r, 1); strcat(done, part); strcat(done, r == 1 ? " EVENT" : " EVENTS");
        notice = done;
    } else if ((song_job == JOB_COPY || song_job == JOB_MERGE) && song_job_value[song_job][0] == song_job_value[song_job][1])
        notice = "PICK TWO DIFFERENT TRACKS";
    else notice = !r ? "NOTHING TO CHANGE THERE" : r == -2 ? "PICK A KEY AND SCALE IN SETTINGS" : r == -4 ? "NOT INSIDE A CLIP"
        : r == -3 ? "COPY SOMETHING FIRST" : song_job == JOB_STRETCH ? "DOESN'T FIT, OR TRACKS IN THE WAY"
        : song_job == JOB_EXPAND ? "NOT ENOUGH ROOM IN THE SONG" : "DOESN'T FIT";
    notice_until = now + 2500U;
}

static int song_job_input(const struct fm1_controls *c, uint32_t *pressed, uint32_t key_down, uint32_t now)
{
    unsigned shift = (c->buttons & BUTTON(FX)) != 0;
    int answer = song_yes_no(pressed);
    if ((*pressed & (BUTTON(HOME) | BUTTON(EDIT))) || (answer < 0 && !song_asking())) { /* back to the song */
        *pressed &= ~(BUTTON(HOME) | BUTTON(EDIT));
        page = music_page = SONG;
        cache->shown_page = ~0U;
        return 1;
    }
    if (*pressed & BUTTON(PLAY)) { if (song_port_player()->playing) song_port_stop(); else song_start(now); }
    if ((*pressed & BUTTON(SAVE)) && !song_asking()) { /* the song list as SAVE comes up, as on the other song pages */
        *pressed &= ~BUTTON(SAVE);
        song_save_press();
    }
    *pressed &= ~(BUTTON(PLAY) | BUTTON(SAVE) | BUTTON(FX) | BUTTON(REC) | BUTTON(ARP) | BUTTON(ENV) | BUTTON(SCL) | BUTTON(LFO));
    if (*pressed & BUTTON(GLO)) return 0;
    if (answer < 0) song_ask_close();
    select_track(c->encoders[0]);
    unsigned before = song_job;
    song_job = (unsigned)clamp((int)song_job + c->encoders[2], 0, SONG_JOBS - 1);
    const struct song_job_def *d = &song_jobs[song_job];
    unsigned pages = d->count ? (d->count + 2U) / 3U : 1U;
    if (song_job != before) song_job_page = 0;
    song_job_page = (unsigned)clamp((int)song_job_page + c->encoders[1], 0, (int)pages - 1);
    for (unsigned k = 0; k < 3; ++k) {
        unsigned p = song_job_page * 3U + k;
        if (p >= d->count || !c->encoders[3 + k]) continue;
        const struct song_job_param *q = song_job_param(song_job, p);
        int high = q->high;
        if (q->kind == V_LANE) { const uint8_t *index; high = (int)song_job_lanes(&index) - 1; }
        song_job_value[song_job][p] = (int16_t)clamp(song_job_value[song_job][p] + c->encoders[3 + k] * (shift ? 10 : 1),
                                                     q->low, high);
    }
    unsigned picked = (c->buttons & BUTTON(LFO)) && (key_down & ~blocked_keys);
    if (song_job != before || picked || c->encoders[0] || c->encoders[1] || c->encoders[3] || c->encoders[4] || c->encoders[5])
        song_ask_close(); /* the job, a setting or the range changed */
    if (c->buttons & BUTTON(LFO)) { song_pick_tracks(key_down & ~blocked_keys); blocked_keys |= key_down; }
    if (answer > 0) song_job_yes(now);
    return 1;
}
