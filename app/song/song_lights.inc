/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* song-mode: key lights on the song pages, and the scale snap for played
 * keys. Included by song_panel.inc. The button lights are all on, all the
 * time, on every page (screen.c), so the printed labels can be read, but
 * PLAY's blinks with the beat while the song plays (0.5.1). */

static unsigned song_button_leds(unsigned bits)
{
#if SONG_UI
    if (!song_deliver_beat_lit()) bits &= ~BUTTON(PLAY);
#endif
    return bits;
}

/* Key lights: the scale on pitched tracks, the swinging tracks on the
 * settings page, or in the grid each track key lit while its clip plays and
 * blinking while a launch waits. Held keys stay lit. */
static uint32_t song_key_leds(uint32_t bits, unsigned visible)
{
#if !SONG_UI
    (void)visible;
    return bits;
#else
    const struct song *s = song_port_song();
    if (visible == SONG_GRID) {
        const struct song_player *p = song_port_player();
        bits = 0;
        for (unsigned t = 0; t < SONG_TRACKS; ++t)
            if (p->waiting[t] != SONG_FROM_NONE ? frame_time / 200U & 1U : p->from[t] < SONG_GRID_ROWS && p->playing)
                bits |= 1U << white_keys[t];
        return bits;
    }
    if (visible == SONG_SETUP) {
        bits = 0;
        for (unsigned t = 0; t < SONG_TRACKS; ++t)
            if (s->swing_tracks >> t & 1U) bits |= 1U << white_keys[t];
        return bits;
    }
    if (visible < SONG || !s->key_scale || groove_pad_count(track)) return bits;
    unsigned mask = song_scale_mask(s);
    for (unsigned key = 0; key < 27; ++key) {
        unsigned note = groove_key_note(track, key, octave);
        if (note < 128 && (mask >> (note % 12U) & 1U)) bits |= 1U << key;
    }
    return bits;
#endif
}
#if SONG_UI
/* Chord mode plays the keys on the song pages (song_chord.c): 1 when it
 * took them. In step entry the chord a root or F5 to G5 plays is the
 * entry, unless it plays (0.5.0), and keys pressed with SHIFT never get
 * here (blocked_keys). A type key names the type it picked. The stock
 * pages play the keys as ever. */
static int song_chord_panel(uint32_t keys, unsigned mode)
{
    const struct song *s = song_port_song();
    unsigned before = s->chord_type[track];
    if (page < SONG) { (void)song_chord_keys(track, 0, octave, 1, NULL); return 0; } /* the stock pages play single notes */
    int took = song_chord_keys(track, keys, octave, mode,
                               page == SONG_STEP && !song_port_player()->playing ? song_port_step() : NULL);
    if (took && s->chord_type[track] != before) {
        notice = song_chord_names[s->chord_type[track]];
        notice_until = frame_time + 1500U;
    }
    return took;
}
#endif
#if SONG_UI
/* Chromatic mode (0.6.6): on a sampler track with it on, every key on the
 * song pages plays the chosen pad, at the key's semitones from C4 after
 * the octave, at most 24 either way. */
static unsigned song_chromatic(void)
{
    return page >= SONG && song_chromatic_on(song_port_song(), track) && groove_source_type(track) == GROOVE_SAMPLE;
}
static int song_key_semitones(unsigned key)
{
    int n = (int)key - 7 + 12 * octave; /* F3 is 7 below C4 */
    return n < -GLITCH_TUNE_MAX ? -GLITCH_TUNE_MAX : n > GLITCH_TUNE_MAX ? GLITCH_TUNE_MAX : n;
}
/* A key going down: its pad tuned to the key's semitones, or to none. */
static void song_key_tune(unsigned key, unsigned note)
{
    song_tune_live(track, note, song_chromatic() ? song_key_semitones(key) : 0);
}
#else
#define song_key_tune(key, note) ((void)0)
#endif
/* Out-of-scale keys snap up to the next scale note on pitched tracks, and
 * in chromatic mode every key is the chosen pad. */
static unsigned song_key_note(unsigned note)
{
#if SONG_UI
    if (song_chromatic()) return groove_pad_note(track, song_chromatic_pad(song_port_song(), track));
    if (note > 127 || page < SONG || groove_pad_count(track)) return note;
    return song_scale_snap(song_port_song(), note);
#else
    return note;
#endif
}
