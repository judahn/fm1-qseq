/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* song-mode: chord mode on the keys (design 8g), the port's side. On a
 * pitched track with it on, F3 to E4 are roots and F4 to E5 pick the type
 * (chords.h), and F5, F#5 and G5 play the last chord again, an octave down
 * and an octave up. One chord sounds at a time, while its key is held. Its
 * notes go out through the strum queue, which the delivery empties block by
 * block before each renders (song_deliver.c), so a strum is exact to the
 * frame, and into a take as played (song_port.c). See song_port.h. */
#include "song_port.h"
#include "chords.h"
#include "instrument.h"
#include <string.h>
#if SONG_UI

static struct song_strum strum;
static struct song_chord_play play[SONG_TRACKS];
static uint8_t notes[CHORD_NOTES], count, key = 0xff, held_track, ready;
static uint32_t previous;

#ifdef FM1_OPEN_WEB
void song_probe_heard(unsigned track, unsigned pitch, unsigned velocity); /* the preview check listens */
#endif
SONG_SMALL static void send(unsigned track, unsigned pitch, unsigned velocity)
{
    unsigned channel = groove_midi_input_channel(track);
    const uint8_t m[3] = {(uint8_t)((velocity ? 0x90U : 0x80U) | channel), (uint8_t)pitch, (uint8_t)velocity};
    if (channel < 16) song_deliver_note(m);
#ifdef FM1_OPEN_WEB
    song_probe_heard(track, pitch, velocity);
#endif
}
void song_chord_service(uint32_t block) { song_strum_due(&strum, (uint64_t)(block + 1U) * FM1_BLOCK_FRAMES, send); }
void song_chord_forget(void) { ready = 0; } /* a song loaded: smooth and the random strum start again from it */
/* The chord sounding lets go: what went out ends, what was still to come never plays. */
SONG_SMALL static void let_go(void)
{
    uint32_t state = song_port_lock();
    for (unsigned i = 0; i < count; ++i)
        if (song_strum_drop(&strum, held_track, notes[i])) song_port_chord_drop(held_track, notes[i]);
        else send(held_track, notes[i], 0);
    song_port_unlock(state);
    count = 0;
    key = 0xff;
}
SONG_SMALL static void strike(unsigned t, unsigned k, const uint8_t *pitch, unsigned n, struct song_step *entry)
{
    struct song *s = song_port_song();
    const struct song_player *p = song_port_player();
    uint8_t offset[CHORD_NOTES];
    if (key != 0xff) let_go();
    song_chord_strum(s, t, n, &play[t], offset);
    uint32_t clock = song_player_clock(p), state = song_port_lock();
    song_strum_add(&strum, (uint64_t)song_port_blocks() * FM1_BLOCK_FRAMES, s->tempo[song_tempo_at(s, clock)].bpm100, t, pitch,
                   offset, n, 100);
    song_port_unlock(state);
    song_port_chord_take(t, pitch, offset, n);
    if (entry) step_chord(entry, pitch, offset, n);
    memcpy(notes, pitch, n);
    count = (uint8_t)n;
    key = (uint8_t)k;
    held_track = (uint8_t)t;
}
SONG_SMALL int song_chord_keys(unsigned t, uint32_t keys, int octave, unsigned mode, struct song_step *entry)
{
    struct song *s = song_port_song();
    unsigned on = song_chord_on(s, t) && !groove_pad_count(t); /* drum tracks play their pads */
    uint8_t pitch[CHORD_NOTES];
    if (!ready) { for (unsigned i = 0; i < SONG_TRACKS; ++i) song_chord_reset(&play[i], s, i); ready = 1; }
    if (key != 0xff && (!on || mode || t != held_track || !(keys >> key & 1U))) let_go();
    uint32_t down = on && !mode ? keys & ~previous : 0;
    previous = keys;
    if (!on) return 0;
    for (unsigned k = 12; k < 24; ++k) /* a type, and a root held plays it now */
        if (down >> k & 1U) {
            s->chord_type[t] = song_chord_keys_layout[k - 12U];
            if (key < 12) down |= 1U << key;
        }
    for (unsigned k = 0; k < 12; ++k)
        if (down >> k & 1U) {
            unsigned root = groove_key_note(t, k, octave);
            if (root < 128) strike(t, k, pitch, song_chord_voice(s, t, root, s->chord_type[t], &play[t], pitch), entry);
        }
    for (unsigned k = 24; k < 27; ++k) /* the last chord again, an octave down, an octave up */
        if (down >> k & 1U) {
            unsigned n = 0;
            for (unsigned i = 0; i < play[t].last_count; ++i) {
                int p = play[t].last[i] + (k == 24 ? 0 : k == 25 ? -12 : 12);
                if (p >= 0 && p < 128) pitch[n++] = (uint8_t)p;
            }
            if (n) strike(t, k, pitch, n, entry);
        }
    return 1;
}
#endif
