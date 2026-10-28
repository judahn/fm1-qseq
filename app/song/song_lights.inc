/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* song-mode: button and key lights on the song pages, and the scale snap
 * for played keys. Included by song_panel.inc. */

/* Button lights: the page's own buttons, and PLAY while the song plays. */
static uint32_t song_leds(uint32_t bits, unsigned visible)
{
    const struct song_player *p = song_port_player();
    if (p->playing) bits |= BUTTON(PLAY);
    if (visible < SONG) return bits;
    bits &= ~(BUTTON(HOME) | BUTTON(ENV) | BUTTON(FX) | BUTTON(GLO) | BUTTON(REC) | BUTTON(LFO));
    if (previous_buttons & BUTTON(FX)) bits |= BUTTON(FX);
#if SONG_UI
    bits = song_stuttering() ? bits | BUTTON(ARP) : bits & ~BUTTON(ARP);
#endif
    if (visible == SONG) return bits | BUTTON(SEQ) | (p->song->loop_on ? BUTTON(ENV) : 0);
#if SONG_UI
    if (visible == SONG_STEP) return bits | BUTTON(REC) | (previous_buttons & BUTTON(LFO)) | (song_glitch_layer ? BUTTON(ARP) : 0);
#else
    if (visible == SONG_STEP) return bits | BUTTON(REC) | (previous_buttons & BUTTON(LFO));
#endif
    return visible == SONG_SETUP ? bits | BUTTON(GLO) : bits;
}
/* Key lights: the scale on pitched tracks, or the swinging tracks on the
 * settings page. Held keys stay lit. */
static uint32_t song_key_leds(uint32_t bits, unsigned visible)
{
#if !SONG_UI
    (void)visible;
    return bits;
#else
    const struct song *s = song_port_song();
    if (visible == SONG_SETUP) {
        bits = 0;
        for (unsigned t = 0; t < SONG_TRACKS; ++t)
            if (s->swing_tracks >> t & 1U) bits |= 1U << white_keys[t];
        return bits;
    }
    if (visible < SONG || !s->key_scale || groove_pad_count(track)) return bits;
    unsigned mask = song_scale_mask(s);
    for (unsigned key = 0; key < (visible == SONG_STEP ? 24U : 27U); ++key) {
        unsigned note = groove_key_note(track, key, octave);
        if (note < 128 && (mask >> (note % 12U) & 1U)) bits |= 1U << key;
    }
    return bits;
#endif
}
/* Out-of-scale keys snap up to the next scale note on pitched tracks. */
static unsigned song_key_note(unsigned note)
{
#if SONG_UI
    if (note > 127 || page < SONG || groove_pad_count(track)) return note;
    return song_scale_snap(song_port_song(), note);
#else
    return note;
#endif
}
