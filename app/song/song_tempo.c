/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* song-mode: the engine's tempo follows the song (0.3.0). The engine keeps
 * a tempo of its own, whole BPM from 30 to 300, for the arpeggiator, the
 * synced delay, the stock pages' tempo and MIDI clock out. It follows the
 * song's tempo where the playhead is, rounded to whole BPM: as playing
 * crosses a tempo change, after a jump, and after the song changes (a
 * setting, a load, an undo). A change made to it on a stock page or by tap
 * tempo goes the other way, into the song's tempo in force at the playhead
 * (the starting tempo, or the conductor's change there), so the two never
 * disagree. With MIDI clock in, neither moves. See song_port.h. */
#include "song_port.h"
#include "instrument.h"
#include "player.h"
#if SONG_UI

static unsigned sent, seen; /* the BPM last sent, and the engine's after it */

/* The tempo change in force at the playhead, in the song's own time. */
SONG_SMALL static unsigned segment(void)
{
    const struct song_player *p = song_port_player();
    const struct song *s = p->song;
    return p->playing ? p->segment : song_tempo_at(s, s->open ? p->song_at : p->playhead);
}
SONG_SMALL void song_tempo_follow(unsigned force)
{
    if (groove_clock_mode() == GROOVE_CLOCK_MIDI) return;
    unsigned bpm = (song_port_song()->tempo[segment()].bpm100 + 50U) / 100U;
    bpm = bpm < 30 ? 30 : bpm > 300 ? 300 : bpm;
    if (bpm == sent && (!force || groove_tempo_saved() == bpm)) return;
    groove_tempo_edit((int)bpm - (int)groove_tempo_saved());
    sent = bpm;
    seen = groove_tempo_saved();
}
SONG_SMALL void song_tempo_adopt(void)
{
    unsigned bpm = groove_tempo_saved();
    if (groove_clock_mode() == GROOVE_CLOCK_MIDI || bpm == seen || !seen) return;
    const struct song *s = song_port_song();
    (void)song_port_set_tempo(s->tempo[segment()].clock, bpm * 100U);
    sent = seen = bpm;
}
#endif
