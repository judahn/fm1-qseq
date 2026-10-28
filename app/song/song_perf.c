/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* song-mode: recording a performance from the grid (design 8f), the port's
 * side: the player locked around each step, and the take one undo step.
 * The blocks themselves come from blocks.c. See song_port.h. */
#include "song_port.h"
#include "blocks.h"
#include "player.h"
#include "undo.h"
#if SONG_UI

static struct song_take take;

SONG_SMALL void song_perf_poll(void)
{
    struct song_player *p = song_port_player();
    if (!p->perf) return;
    if (p->perf == 2 && !p->playing) { song_perf_end(); return; } /* it stopped on its own */
    song_take_drain(&take, p);
}
SONG_SMALL int song_perf_arm(void)
{
    struct song_player *p = song_port_player();
    if (p->perf || p->song->open) return -1;
    song_undo_begin(song_port_undo(), p->song);
    song_take_begin(&take, p);
    return 0;
}
SONG_SMALL void song_perf_end(void)
{
    struct song_player *p = song_port_player();
    if (!p->perf) return;
    song_take_end(&take, p);
    song_undo_end(song_port_undo(), p->song);
}
unsigned song_perf_state(void) { return song_port_player()->perf; }
#endif
