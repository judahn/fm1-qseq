/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* song-mode: step entry input, included by song_step.inc. Every key, F3 to
 * G5, enters notes, and a chord goes in when every key is up. With SHIFT
 * held a key works by its note name in any octave: the white keys pick the
 * length, F longest to E shortest, and the black ones are triplet, dots,
 * rest, tie and repeat. Holding LFO picks notes in a chord. In chord mode
 * the roots enter their chords, F4 to E5 pick the type, and F5 to G5 enter
 * the last chord again, an octave down and an octave up (song_chord_keys). */
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
/* SHIFT plus a key, by its name (key % 12, from F): F# triplet, G# dots,
 * A# repeat, C# rest, D# tie, and the white keys F to E 1/1 to 1/64. */
static void step_shift_key(struct song_step *st, unsigned name, uint32_t now)
{
    static const uint8_t lengths[12] = {STEP_WHOLE, 0, STEP_HALF, 0, STEP_QUARTER, 0, STEP_8TH, STEP_16TH, 0, STEP_32ND, 0, STEP_64TH};
    const char *wrong = NULL;
    switch (name) {
    case 1: step_toggle_triplet(st); break;
    case 3: step_cycle_dots(st); break;
    case 5: if (step_repeat(st)) wrong = "NOTHING TO REPEAT OR SONG FULL"; break;
    case 8: step_rest(st); break;
    case 10: if (step_tie(st)) wrong = "TIE ONLY RIGHT AFTER A NOTE"; break;
    default: step_pick_length(st, lengths[name]);
    }
    if (wrong) { notice = wrong; notice_until = now + 1500U; }
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
        for (unsigned key = 0; key < 27; ++key)
            if (down >> key & 1U) step_shift_key(st, key % 12U, now);
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
