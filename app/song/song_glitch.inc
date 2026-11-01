/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* song-mode: glitch on the panel (design section 8e). In the song view,
 * SHIFT plus F#3, G#3 or A#3 mangles the selection: scramble the 16ths,
 * reverse, or shuffle notes between the selected tracks. Step entry's
 * glitch layer (song_step.inc) uses the names and values here. */
#include "glitch.h"
#include "mangle.h"

static const char *const song_glitch_names[GLITCH_FIELDS] = {
    "REPEATS", "DRIFT", "SCALE", "FADE", "CHANCE", "OCTAVE", "RANGE", "ACCENT", "FLAM", "PAD SWAP", "TUNE"};

static void song_glitch_value(uint32_t w, unsigned field, char out[12])
{
    unsigned v = song_glitch_field(w, field);
    if (field == GLITCH_REPEATS) { if (v) { out[0] = 'X'; decimal(out + 1, (int)song_glitch_repeats(w), 1); } else strcpy(out, "OFF"); }
    else if (field == GLITCH_DRIFT || field == GLITCH_TUNE) { /* semitones, or for drift scale steps */
        int d = field == GLITCH_TUNE ? song_glitch_tune(w) : song_glitch_drift(w);
        out[0] = d < 0 ? '-' : '+';
        decimal(out + 1, d < 0 ? -d : d, 1);
        if (field == GLITCH_DRIFT) strcat(out, song_glitch_field(w, GLITCH_SCALE) ? " SC" : " ST");
    } else if (field == GLITCH_RANGE) { decimal(out, (int)v + 1, 1); strcat(out, " OCT"); }
    else if (field == GLITCH_ACCENT || field == GLITCH_FLAM) strcpy(out, v ? "ON" : "OFF");
    else if (field != GLITCH_CHANCE && !v) strcpy(out, "OFF");
    else { decimal(out, (int)song_glitch_percent(w, field), 1); strcat(out, "%"); }
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
