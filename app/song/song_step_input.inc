/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* song-mode: step entry input, included by song_step.inc. Every key, F3 to
 * G5, enters notes, and a chord goes in when every key is up. With SHIFT
 * held a key works by its note name in any octave: the white keys pick the
 * length, F longest to E shortest, as K1 does turning right (0.5.1), and
 * the black ones are triplet, dots, rest, tie and repeat. PRESETS steers
 * the pointer, SHIFT+PRESETS note to note, and K4 is the pitch knob (0.5.2,
 * the other way round before). Holding SEL picks notes in a chord, and
 * turning PRESETS meanwhile sweeps a selection, where keys toggle pitches
 * (0.5.0, LFO until 0.5.6). ENV deletes the selected notes and LFO
 * back-deletes (0.5.6, SEL and ENV before). In chord mode the roots enter their chords, F4 to E5 pick the
 * type, and F5 to G5 enter the last chord again, an octave down and an
 * octave up (song_chord_keys). In chromatic mode every key enters the
 * chosen pad with the key's pitch as its TUNE, and on a sampler track
 * SHIFT+K4 turns the selected notes' TUNE by semitones (0.6.6). SHIFT+K1
 * moves the selected notes by the length and SHIFT+K2 nudges them by 5
 * clocks (0.6.7, step_move.c), playing or not, a run of those turns with
 * nothing else between being one undo step, and the notes they replace
 * along the way coming back as the run moves on (0.6.8). PLAY plays
 * from the pointer's bar and stays here, and while it plays the keys only
 * sound. REC records a take here as in the other views, a hold opens the
 * record settings, and SHIFT+REC leaves, as SEQ does (0.5.0). ALGO zooms
 * the roll and ARP steps the glitch pages. */
static uint32_t step_held; /* entry keys down for the chord being built */
static uint8_t step_from; /* the page step entry was opened from (0.4.1) */
static uint8_t step_moving; /* the last control was MOVE or NUDGE, so the next turn joins its undo step (0.6.7) */
static unsigned song_key_note(unsigned note);
static unsigned song_chromatic(void); /* song_lights.inc */
static int song_key_semitones(unsigned key);
static void song_rec_down(uint32_t now); /* song_rec.inc */

static void song_step_enter(void)
{
    struct song_step *st = song_port_step();
    step_begin(st, track, song_port_player()->playhead);
    step_held = step_moving = 0;
    step_from = (uint8_t)page;
    page = music_page = SONG_STEP;
    cache->shown_page = ~0U;
    step_land(st);
}
/* Back to the view it was opened from (0.4.1). Stopped, the playhead
 * follows the pointer out and starts there next (0.5.0). Playing goes on,
 * and from the grid the clip stays launched as it closes. */
static void song_step_leave(void)
{
    struct song_step *st = song_port_step();
    step_preview_stop(st);
    st->ranged = st->picked = step_moving = 0;
    step_forget(st);
    if (step_from == SONG_GRID && st->song->open) {
        song_port_clip_launched();
        song_port_clip_close();
        page = music_page = SONG_GRID;
    } else {
        if (!song_port_player()->playing) song_port_jump(st->pointer);
        page = music_page = step_from == SONG_AUTO ? SONG_AUTO : SONG;
    }
    cache->shown_page = ~0U;
}
/* PLAY and a take start at the pointer's bar, the start point from then. */
static void step_to_bar(void)
{
    const struct song_step *st = song_port_step();
    uint32_t start;
    song_bar_at(st->song, st->pointer, &start, NULL);
    song_port_jump(start);
}
/* REC tapped here (song_rec_go): a take on the track, from the pointer's
 * bar with its count-in, or playing in a clip from its next pass. With
 * STEP picked in the record settings, this is step entry already. */
static void song_step_rec(unsigned step, uint32_t now)
{
    struct song *s = song_port_song();
    struct song_step *st = song_port_step();
    if (st->track != track) step_begin(st, track, st->pointer); /* SELECT turned in the record settings */
    notice_until = now + 2000U;
    if (step) return;
    if (song_port_player()->playing) notice = s->open && !song_port_record_clip(0) ? "REC FROM THE CLIP'S NEXT PASS" : "STOP FIRST TO RECORD";
    else {
        step_to_bar();
        if (song_port_record(s->rec_mode, track)) notice = "STOP FIRST";
    }
}
static void step_refused(int result, uint32_t now)
{
    if (result != STEP_TOO_BIG) return;
    notice = "SELECTION TOO BIG TO UNDO";
    notice_until = now + 2000U;
}
/* SHIFT plus a key, by its name (key % 12, from F): F# triplet, G# dots,
 * A# repeat, C# rest, D# tie, and the white keys F to E 1/1 to 1/64.
 * Playing, the lengths still change, and nothing is entered. */
static void step_shift_key(struct song_step *st, unsigned name, unsigned playing, uint32_t now)
{
    static const uint8_t lengths[12] = {STEP_WHOLE, 0, STEP_HALF, 0, STEP_QUARTER, 0, STEP_8TH, STEP_16TH, 0, STEP_32ND, 0, STEP_64TH};
    const char *wrong = NULL;
    switch (playing && (name == 5 || name == 8 || name == 10) ? 12 : name) {
    case 1: step_refused(step_toggle_triplet(st), now); break;
    case 3: step_refused(step_cycle_dots(st), now); break;
    case 5: if (step_repeat(st)) wrong = "NOTHING TO REPEAT OR SONG FULL"; break;
    case 8: step_rest(st); break;
    case 10: if (step_tie(st)) wrong = "TIE ONLY RIGHT AFTER A NOTE"; break;
    case 12: wrong = "STOP TO ENTER NOTES"; break; /* rest, tie and repeat while playing */
    default: step_refused(step_pick_length(st, lengths[name]), now);
    }
    if (wrong) { notice = wrong; notice_until = now + 1500U; }
}
/* MOVE and NUDGE (0.6.7): detent by detent, `clocks` each, stopping at the
 * first one refused, which says why. Playing, the player hears of each.
 * `more`: the run goes on, as it does from the first detent on. */
static void step_nudged(struct song_step *st, int detents, uint32_t clocks, unsigned *more, uint32_t now)
{
    for (int k = detents < 0 ? -detents : detents; k-- > 0; *more = 1) {
        int r = step_nudge(st, detents < 0 ? -(int32_t)clocks : (int32_t)clocks, *more, song_port_player());
        if (!r) continue;
        if (r == STEP_START || r == STEP_END) {
            notice = r == STEP_START ? "CAN'T MOVE BEFORE THE START" : "CAN'T MOVE PAST THE END";
            notice_until = now + 1500U;
        } else step_refused(r, now);
        *more = 1;
        return;
    }
}
static int song_step_input(const struct fm1_controls *c, uint32_t *pressed, uint32_t key_down, uint32_t now)
{
    struct song_step *st = song_port_step();
    unsigned shift = (c->buttons & BUTTON(FX)) != 0, picking = (c->buttons & BUTTON(SCL)) != 0;
    unsigned playing = song_port_player()->playing, taking = song_port_recording() != 0;
    /* MOVE and NUDGE, SHIFT+K1 and SHIFT+K2 with the glitch layer off (0.6.7). Any other control ends their run. */
    unsigned moving = shift && !song_glitch_layer && (c->encoders[2] || c->encoders[3]);
    unsigned other = key_down || (*pressed & ~(BUTTON(FX) | BUTTON(SCL))) || c->encoders[0] || c->encoders[1] ||
                     c->encoders[4] || c->encoders[5] || c->encoders[6] || (!moving && (c->encoders[2] || c->encoders[3]));
    if (other || taking) step_moving = 0;
    if (st->track != track && !taking) { step_begin(st, track, st->pointer); step_held = step_moving = 0; } /* the record settings' SELECT */
    if (*pressed & BUTTON(REC)) { /* a take ends as REC goes down, SHIFT+REC leaves, a tap records (song_rec_held) */
        *pressed &= ~BUTTON(REC);
        if (taking) { song_port_record_end(1); return 1; }
        if (shift) { song_step_leave(); return 1; }
        song_rec_down(now);
    }
    if (*pressed & BUTTON(PLAY)) { /* PLAY during a take is song_play_ends_take's */
        if (playing) song_port_stop();
        else { step_to_bar(); song_start(now); }
        playing = song_port_player()->playing;
    }
    if (taking) { /* the pointer is hidden: K1 to K4 record their lanes, ALGO zooms, PRESETS does nothing */
        step_forget(st); /* a take changes the notes: moved ones are selected no more (0.6.8) */
        *pressed &= ~(BUTTON(PLAY) | BUTTON(FX) | BUTTON(SCL) | BUTTON(ENV) | BUTTON(LFO) | BUTTON(ARP));
        if (*pressed & (BUTTON(HOME) | BUTTON(EDIT) | BUTTON(GLO) | BUTTON(SAVE))) return 0;
        octave = clamp(octave + !!(*pressed & BUTTON(OCT_UP)) - !!(*pressed & BUTTON(OCT_DOWN)), -2, 2);
        step_zoom_turn(c->encoders[1]);
        for (unsigned k = 0; k < 4; ++k)
            if (c->encoders[2 + k]) song_knob_turn(k, fm_encoder_delta(2 + k, c->encoders[2 + k]));
        step_held = 0;
        return 1;
    }
    if (*pressed & BUTTON(ENV)) step_refused(step_delete(st), now);
    if (*pressed & BUTTON(LFO)) step_back_delete(st);
    if (*pressed & BUTTON(ARP)) song_glitch_layer = (uint8_t)((song_glitch_layer + 1U) % 4U); /* pages 1 to 3, then off */
    *pressed &= ~(BUTTON(PLAY) | BUTTON(FX) | BUTTON(SCL) | BUTTON(ENV) | BUTTON(LFO) | BUTTON(ARP));
    if (*pressed & (BUTTON(HOME) | BUTTON(EDIT) | BUTTON(GLO) | BUTTON(SAVE))) { step_preview_stop(st); return 0; }
    octave = clamp(octave + !!(*pressed & BUTTON(OCT_UP)) - !!(*pressed & BUTTON(OCT_DOWN)), -2, 2);
    if (c->encoders[0]) { select_track(c->encoders[0]); step_begin(st, track, st->pointer); step_held = 0; }
    step_zoom_turn(c->encoders[1]);
    if (song_glitch_layer) {
        for (unsigned k = 0; k < 3; ++k) {
            unsigned field = song_glitch_pages[song_glitch_layer - 1][k];
            if (field == GLITCH_DRIFT && shift) field = GLITCH_SCALE;
            int r = c->encoders[2 + k] ? step_set_glitch(st, field, c->encoders[2 + k]) : 0;
            if (r == -1) { notice = "GLITCH TABLE FULL"; notice_until = now + 1500U; }
            step_refused(r, now);
        }
    } else if (moving) { /* the run's undo step goes on, or one starts */
        unsigned more = step_moving;
        song_undo_run(song_port_undo(), more);
        step_moving = !other;
        step_nudged(st, c->encoders[2], step_clocks(st), &more, now);
        step_nudged(st, c->encoders[3], STEP_NUDGE, &more, now);
        if (c->encoders[4]) step_refused(step_set_gate(st, c->encoders[4]), now);
    } else {
        if (c->encoders[2]) step_refused(step_set_length(st, -c->encoders[2]), now); /* right halves it, as F to E (0.5.1) */
        if (c->encoders[3]) step_refused(step_set_velocity(st, c->encoders[3]), now);
        if (c->encoders[4]) step_refused(step_set_gate(st, c->encoders[4]), now);
    }
    if (c->encoders[6]) { /* PRESETS, the pointer (0.5.2): with SEL held, a sweep */
        if (picking) step_sweep(st, c->encoders[6], shift);
        else if (shift) step_jump(st, c->encoders[6] > 0 ? 1 : -1);
        else step_move(st, c->encoders[6]);
    }
    if (c->encoders[5] && shift && groove_source_type(track) == GROOVE_SAMPLE) { /* SHIFT+K4 on a sampler: TUNE (0.6.6) */
        int r = step_set_glitch(st, GLITCH_TUNE, c->encoders[5]);
        if (r == -1) { notice = "GLITCH TABLE FULL"; notice_until = now + 1500U; }
        step_refused(r, now);
        if (!r) step_preview(st);
    } else if (c->encoders[5]) { /* K4, the pitch knob (0.5.2): scale steps, octaves with SHIFT, pads on drums */
        unsigned pads = groove_pad_count(track);
        uint8_t notes[GROOVE_PADS];
        for (unsigned i = 0; i < pads && i < GROOVE_PADS; ++i) notes[i] = (uint8_t)groove_pad_note(track, i);
        step_refused(pads ? step_transpose_pads(st, notes, pads < GROOVE_PADS ? pads : GROOVE_PADS, c->encoders[5])
                          : step_transpose(st, c->encoders[5], shift), now);
    }
    uint32_t down = key_down & ~blocked_keys;
    if (shift) {
        for (unsigned key = 0; key < 27; ++key)
            if (down >> key & 1U) step_shift_key(st, key % 12U, playing, now);
        blocked_keys |= key_down; /* no note, no chord */
    } else {
        /* Chord mode: song_chord_panel plays these keys after this, and a
         * root's chord or F5 to G5's is the entry, in when the key is up. */
        unsigned chords = song_chord_on(st->song, track) && !groove_pad_count(track);
        for (unsigned key = 0; key < 27; ++key)
            if (chords) { if ((down >> key & 1U) && (key < 12 || key >= 24)) step_held |= 1U << key; }
            else if (down & (1U << key)) {
                unsigned pitch = song_key_note(groove_key_note(track, key, octave));
                if (pitch > 127) continue;
                if (picking) step_pick(st, pitch);
                else if (!playing) {
                    if (song_chromatic()) /* the entry is the pad at the key's pitch (0.6.6) */
                        st->glitch = song_glitch_turn(st->glitch, GLITCH_TUNE, song_key_semitones(key) - song_glitch_tune(st->glitch));
                    step_key(st, pitch);
                    step_held |= 1U << key;
                }
            }
    }
    if (playing) { step_held = 0; st->chord_count = 0; } /* playing, the keys only sound */
    step_held &= c->keys;
    if (!step_held && st->chord_count && step_commit(st)) {
        notice = "SONG FULL - NOTE NOT ENTERED";
        notice_until = now + 2000U;
    }
    return 1;
}
