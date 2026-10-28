/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Key and scale. A scale is a 12-bit set of pitch classes above its root,
 * bit 0 being the root. Notes outside it snap up to the next scale note, and
 * the pitch knob moves by scale steps. Chromatic has all 12, so nothing moves. */
#include "song.h"

const char *const song_scale_names[SONG_SCALES] = {
    "CHROMATIC", "MAJOR", "MINOR", "DORIAN", "PHRYGIAN", "LYDIAN", "MIXOLYDIAN",
    "LOCRIAN", "HARM MINOR", "MEL MINOR", "MAJOR PENTA", "MINOR PENTA", "BLUES"};
const char *const song_key_names[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
static const uint16_t scales[SONG_SCALES] = {
    0xfff,
    0xab5, /* major: 0 2 4 5 7 9 11 */
    0x5ad, /* natural minor: 0 2 3 5 7 8 10 */
    0x6ad, /* Dorian: 0 2 3 5 7 9 10 */
    0x5ab, /* Phrygian: 0 1 3 5 7 8 10 */
    0xad5, /* Lydian: 0 2 4 6 7 9 11 */
    0x6b5, /* Mixolydian: 0 2 4 5 7 9 10 */
    0x56b, /* Locrian: 0 1 3 5 6 8 10 */
    0x9ad, /* harmonic minor: 0 2 3 5 7 8 11 */
    0xaad, /* melodic minor, ascending: 0 2 3 5 7 9 11 */
    0x295, /* major pentatonic: 0 2 4 7 9 */
    0x4a9, /* minor pentatonic: 0 3 5 7 10 */
    0x4e9  /* blues: 0 3 5 6 7 10 */
};

unsigned song_scale_mask(const struct song *s)
{
    unsigned base = scales[s->key_scale < SONG_SCALES ? s->key_scale : 0], root = s->key_root % 12U;
    return ((base << root) | (base >> (12U - root))) & 0xfffU;
}
static unsigned in_scale(unsigned mask, unsigned pitch) { return mask >> (pitch % 12U) & 1U; }

unsigned song_scale_snap(const struct song *s, unsigned pitch)
{
    unsigned mask = song_scale_mask(s);
    if (pitch > 127) pitch = 127;
    for (unsigned p = pitch; p < 128; ++p)
        if (in_scale(mask, p)) return p;
    for (unsigned p = pitch; p-- > 0;) /* nothing above 127: the next one down */
        if (in_scale(mask, p)) return p;
    return pitch;
}

unsigned song_scale_step(const struct song *s, unsigned pitch, int steps)
{
    unsigned mask = song_scale_mask(s);
    for (; steps > 0; --steps) {
        unsigned p = pitch + 1;
        while (p < 128 && !in_scale(mask, p)) ++p;
        if (p > 127) break;
        pitch = p;
    }
    for (; steps < 0; ++steps) {
        unsigned p = pitch;
        while (p > 0 && !in_scale(mask, p - 1)) --p;
        if (p == 0) break;
        pitch = p - 1;
    }
    return pitch;
}
