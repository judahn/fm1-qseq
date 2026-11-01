/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* song-mode: per-note pitch on sampler tracks (0.6.6), the port's side. The
 * engine plays a sampler note at its pad's tune, and a sampler voice takes
 * its pitch as it starts, so a note with TUNE (glitch.h) has its pad's tune
 * set just before it goes out: the user's setting plus the note's
 * semitones. A pad is edited only when what it needs differs from what it
 * has, and stopped, every pad goes back to the user's setting two blocks
 * after the last sampler note, once that note has started. A tune the user
 * turns meanwhile is theirs: it becomes the setting, and isn't put back.
 * Hits of one pad that start in the same block share a tune. The engine
 * marks a sound edited on any change, so a track whose sound was clean
 * before our first change is saved clean while nothing else has been
 * edited since (song_store.c). Control task only. See song_port.h. */
#include "song_port.h"
#include "instrument.h"
#include "sampledrum.h"
#if SONG_UI

enum { PADS = SAMPLE_DRUM_PADS, SEMITONES = 2 * 24 };
static uint8_t base[SONG_TRACKS][PADS], set[SONG_TRACKS][PADS], changed[SONG_TRACKS], seen, clean;
static uint32_t noted_at, revision;
static int8_t live;

/* Before a change: each track's edited mark as it was, read the first time,
 * forgotten when anything else was edited since our last change. */
SONG_SMALL static void edit(unsigned t, unsigned pad, int delta)
{
    if (groove_edit_revision() != revision) seen = clean = 0;
    if (!(seen >> t & 1U)) {
        struct groove_view v = {0};
        groove_get_view(t, 0, &v);
        seen |= (uint8_t)(1U << t);
        if (!v.modified) clean |= (uint8_t)(1U << t);
    }
    groove_tone_edit(t, pad, SAMPLE_DRUM_TUNE, delta);
    revision = groove_edit_revision();
}
/* The pad's tune for a note: the user's setting plus its semitones, inside
 * the parameter's range. */
SONG_SMALL static int aim(unsigned t, unsigned pad, int semitones, const fm1_param_t *p)
{
    int want = base[t][pad] + semitones * (p->max >= SEMITONES ? p->max / SEMITONES : 1);
    return want < 0 ? 0 : want > p->max ? p->max : want;
}
SONG_SMALL void song_tune_note(unsigned t, unsigned pitch, int semitones)
{
    int pad = t < SONG_TRACKS ? groove_pad_index(t, pitch) : -1;
    const fm1_param_t *p = pad >= 0 && pad < PADS && groove_source_type(t) == GROOVE_SAMPLE ?
                           groove_tone_parameter(t, (unsigned)pad, SAMPLE_DRUM_TUNE) : NULL;
    if (!p) return;
    noted_at = song_port_blocks();
    unsigned bit = 1U << pad, had = changed[t] & bit;
    uint8_t now[GROOVE_TONE_PARAMS];
    if (had ? aim(t, (unsigned)pad, semitones, p) == set[t][pad] : !semitones) return; /* it has that already */
    if (groove_tone_copy(t, (unsigned)pad, now) < 0) return;
    if (!had || now[SAMPLE_DRUM_TUNE] != set[t][pad]) base[t][pad] = now[SAMPLE_DRUM_TUNE]; /* the user's, or theirs since */
    int want = aim(t, (unsigned)pad, semitones, p);
    if (want != now[SAMPLE_DRUM_TUNE]) edit(t, (unsigned)pad, want - now[SAMPLE_DRUM_TUNE]);
    set[t][pad] = (uint8_t)want;
    changed[t] = (uint8_t)(want != base[t][pad] ? changed[t] | bit : changed[t] & ~bit);
}
SONG_SMALL void song_tune_restore(void)
{
    for (unsigned t = 0; t < SONG_TRACKS; ++t)
        for (unsigned pad = 0; changed[t] && pad < PADS; ++pad) {
            uint8_t now[GROOVE_TONE_PARAMS];
            if (!(changed[t] >> pad & 1U)) continue;
            changed[t] &= (uint8_t)~(1U << pad);
            if (groove_source_type(t) == GROOVE_SAMPLE && groove_tone_copy(t, pad, now) >= 0 && now[SAMPLE_DRUM_TUNE] == set[t][pad])
                edit(t, pad, base[t][pad] - now[SAMPLE_DRUM_TUNE]);
        }
}
SONG_SMALL unsigned song_tune_settle(void)
{
    unsigned any = 0;
    for (unsigned t = 0; t < SONG_TRACKS; ++t) any |= changed[t];
    if (!any || song_port_player()->playing) return 0;
    if (song_port_blocks() - noted_at < 2U) return 1; /* its note may still be on its way */
    song_tune_restore();
    return 0;
}
void song_tune_forget(void) { song_tune_restore(); seen = clean = 0; }
unsigned song_tune_clean(void) { return groove_edit_revision() == revision ? clean : 0U; }
/* A live key: its pad tuned, and its semitones kept for the take. */
SONG_SMALL void song_tune_live(unsigned t, unsigned pitch, int semitones)
{
    song_tune_note(t, pitch, semitones);
    live = (int8_t)semitones;
}
int song_tune_taken(void) { int s = live; live = 0; return s; }
#endif
