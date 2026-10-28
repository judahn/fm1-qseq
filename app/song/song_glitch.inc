/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* song-mode: glitch on the panel (design section 8e). In the song view,
 * hold GLITCH (ARP) while playing to stutter. F3 to C4 pick the slice, 1/4
 * to 1/64, F#3 makes it a triplet, and K1 drifts each repeat's pitch. SHIFT
 * plus F#3, G#3 or A#3 mangles the selection: scramble the 16ths, reverse,
 * or shuffle notes between the selected tracks. Step entry's glitch layer
 * (song_step.inc) uses the names and values here. */
#include "glitch.h"
#include "mangle.h"

static uint8_t song_slice = 1, song_triplet;
static int song_drift;
static const char *const song_slice_names[SONG_SLICES] = {"1/4", "1/8", "1/16", "1/32", "1/64"};
static const char *const song_glitch_names[GLITCH_FIELDS] = {
    "REPEATS", "DRIFT", "SCALE", "FADE", "CHANCE", "OCTAVE", "RANGE", "ACCENT", "FLAM", "PAD SWAP"};

static void song_glitch_value(uint32_t w, unsigned field, char out[12])
{
    unsigned v = song_glitch_field(w, field);
    if (field == GLITCH_REPEATS) { if (v) { out[0] = 'X'; decimal(out + 1, (int)song_glitch_repeats(w), 1); } else strcpy(out, "OFF"); }
    else if (field == GLITCH_DRIFT) {
        int d = song_glitch_drift(w);
        out[0] = d < 0 ? '-' : '+';
        decimal(out + 1, d < 0 ? -d : d, 1);
        strcat(out, song_glitch_field(w, GLITCH_SCALE) ? " SC" : " ST");
    } else if (field == GLITCH_RANGE) { decimal(out, (int)v + 1, 1); strcat(out, " OCT"); }
    else if (field == GLITCH_ACCENT || field == GLITCH_FLAM) strcpy(out, v ? "ON" : "OFF");
    else if (field != GLITCH_CHANCE && !v) strcpy(out, "OFF");
    else { decimal(out, (int)song_glitch_percent(w, field), 1); strcat(out, "%"); }
}

/* While GLITCH is held: slice keys, triplet and drift. Returns 1 then. */
static int song_stutter_input(const struct fm1_controls *c, uint32_t pressed, uint32_t key_down, uint32_t now)
{
    if (!(c->buttons & BUTTON(ARP))) {
        if (song_stuttering()) song_stutter_release();
        return 0;
    }
    uint32_t down = key_down & ~blocked_keys;
    unsigned before = song_slice | (unsigned)song_triplet << 3;
    for (unsigned i = 0; i < SONG_SLICES; ++i)
        if (down & (1U << white_keys[i])) song_slice = (uint8_t)i;
    if (down & (1U << 1)) song_triplet ^= 1U; /* F#3 */
    blocked_keys |= key_down; /* these keys play nothing while GLITCH is down */
    if (c->encoders[2]) {
        song_drift = clamp(song_drift + c->encoders[2], -12, 12);
        song_stutter_drift(song_drift);
    }
    unsigned slice = song_slice | (song_triplet ? (unsigned)SONG_SLICE_TRIPLET : 0U);
    if ((pressed & BUTTON(ARP)) || (song_stuttering() && slice != before)) {
        if (song_port_player()->playing) song_stutter_hold(slice, song_drift);
        else { notice = "PLAY, THEN HOLD GLITCH"; notice_until = now + 1500U; }
    }
    return 1;
}
static void song_stutter_knob(void)
{
    char drift[12];
    drift[0] = song_drift < 0 ? '-' : '+';
    decimal(drift + 1, song_drift < 0 ? -song_drift : song_drift, 1);
    knob(0, "DRIFT", drift, AMBER);
}
static const char *song_stutter_hint(char out[40])
{
    strcpy(out, "GLITCH ");
    strcat(out, song_slice_names[song_slice]);
    if (song_triplet) strcat(out, "T");
    strcat(out, "  F3-C4 SLICE  F#3 TRIPLET");
    return out;
}

static unsigned song_mangle_count;
static void song_mangle(unsigned which, uint32_t now)
{
    static const char *const done[3] = {"16THS SCRAMBLED", "NOTES REVERSED", "NOTES SHUFFLED ACROSS TRACKS"};
    struct song *s = song_port_song();
    uint32_t seed = s->seed ^ ++song_mangle_count * 0x9e3779b9U;
    int r = which == 0 ? song_scramble(s, song_selected(), s->loop_start, s->loop_end, seed)
          : which == 1 ? song_reverse(s, song_selected(), s->loop_start, s->loop_end)
                       : song_shuffle_tracks(s, song_selected(), s->loop_start, s->loop_end, seed);
    notice = r > 0 ? done[which] : r == 0 ? "NO NOTES THERE" : which == 2 ? "PICK TWO TRACKS OR MORE" : "SELECTION TOO LONG";
    notice_until = now + 1500U;
}
