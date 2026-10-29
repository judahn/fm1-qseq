/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* song-mode: recording into a cell from the grid (design 8f, 0.3.0), the
 * port's side. An empty cell becomes a clip of the REC length, or up to 16
 * bars when FREE, and a clip already there is overdubbed as loop recording,
 * or with the song's mode on REPLACE emptied first (0.4.0).
 * Playing, the cell launches on its track at the launch quantize (the next
 * beat with it off) and the take starts there, or from its next pass when
 * it plays already. From a stop, playing starts with the count-in. While
 * recording the clip is open in the editor behind the grid, so the take is
 * an open clip's (song_takes.c): the keys play its track and go into the
 * clip at the clip's clocks. A new clip's take ends by itself at the clip's
 * end, FREE's where REC put it, at a bar line, and the clip plays on,
 * launched. The take and a new clip's length are one undo step. */
#include "song_port.h"
#include "player.h"
#include "record.h"
#include "clips.h"
#include "undo.h"
#if SONG_UI

enum { IDLE, LAUNCHING, TAKING };
static struct { uint8_t state, cell, fresh, open_end, ending; } grid;

SONG_SMALL int song_grid_rec(unsigned cell, unsigned bars)
{
    struct song *s = song_port_song();
    struct song_player *p = song_port_player();
    unsigned t = cell / SONG_GRID_ROWS, row = cell % SONG_GRID_ROWS;
    if (cell >= SONG_CELLS || s->open || p->perf || song_port_recording()) return -1;
    grid.cell = (uint8_t)cell;
    grid.fresh = !s->clip_length[cell];
    grid.open_end = grid.fresh && !bars;
    grid.ending = 0;
    grid.state = TAKING;
    song_undo_begin(song_port_undo(), s); /* before the clip opens, so the step keeps the song's loop */
    if (grid.fresh) s->clip_length[cell] = (uint16_t)(bars ? song_bar_clock(s, bars) : song_clip_max(s));
    else if (s->rec_mode == SONG_REC_REPLACE) { /* emptied, keeping its length, in the take's step */
        (void)song_clip_empty(s, cell);
        if (p->from[t] == row) song_player_cut(p, t);
    }
    if (!p->playing) { /* the count-in, then the cell from its top */
        if (!song_port_clip_open(cell) && !song_port_record(SONG_REC_LOOP, t)) return 0;
    } else if (p->from[t] == row && p->waiting[t] == SONG_FROM_NONE) { /* playing it: from its next pass */
        if (!song_port_clip_open(cell) && !song_port_record_clip(0)) return 0;
    } else {
        song_player_launch_rec(p, t, row);
        grid.state = LAUNCHING;
        return 0;
    }
    song_grid_take_ended(0, 0); /* it couldn't start */
    return -1;
}
/* REC again: a FREE take ends at the next bar line, and stops there by
 * itself. Any other ends now, or one not started yet is called off. */
SONG_SMALL void song_grid_rec_key(void)
{
    struct song *s = song_port_song();
    int64_t now = song_port_live_clock();
    if (grid.state == TAKING && grid.open_end && !grid.ending && song_port_recorder()->active && now >= 0) {
        uint32_t bar = song_bar_clock(s, 1), end = ((uint32_t)now + bar - 1U) / bar * bar;
        s->clip_length[grid.cell] = (uint16_t)(end ? end : bar);
        s->loop_end = s->clip_length[grid.cell]; /* the open clip's end */
        grid.ending = 1;
        return;
    }
    song_port_record_end(1);
}
/* Each poll, and before a key: a launch that has taken the track over
 * starts the take, and a new clip's take ends as the clip goes round. */
SONG_SMALL void song_grid_take_poll(void)
{
    struct song *s = song_port_song();
    struct song_player *p = song_port_player();
    unsigned t = grid.cell / SONG_GRID_ROWS, row = grid.cell % SONG_GRID_ROWS;
    if (grid.state == LAUNCHING) {
        if (!p->playing) { song_grid_take_ended(0, 0); return; }
        if (p->from[t] != row || p->waiting[t] != SONG_FROM_NONE) return;
        grid.state = TAKING; /* the top of its first pass */
        if (song_port_clip_open(grid.cell) || song_port_record_clip(1)) song_grid_take_ended(0, 0);
        return;
    }
    if (grid.state != TAKING) return;
    const struct song_recorder *r = song_port_recorder();
    if (!r->active && !song_port_clip_armed()) { song_grid_take_ended(0, 0); return; } /* playing stopped first */
    int64_t now = song_port_live_clock();
    if (grid.fresh && r->active && now >= 0 && (uint32_t)now < r->last) song_port_record_end_at(s->clip_length[grid.cell]);
}
/* The take has ended at `clock`, in the clip's time, or never began. The
 * clip closes, and plays on as the launched clip while playing goes on. A
 * new clip keeps its length only when its take began, and FREE's is the
 * bar line at or after where it ended unless REC set it. */
SONG_SMALL void song_grid_take_ended(unsigned began, uint32_t clock)
{
    struct song *s = song_port_song();
    struct song_player *p = song_port_player();
    unsigned t = grid.cell / SONG_GRID_ROWS, row = grid.cell % SONG_GRID_ROWS;
    if (!grid.state) return;
    if (grid.state == LAUNCHING && p->waiting[t] == row) p->waiting[t] = SONG_FROM_NONE; /* called off */
    if (s->open && p->playing) { p->from[t] = (uint8_t)row; p->base[t] = p->edit_base; p->waiting[t] = SONG_FROM_NONE; }
    song_port_clip_close();
    if (grid.fresh) {
        uint32_t bar = song_bar_clock(s, 1), length = s->clip_length[grid.cell];
        if (grid.open_end && !grid.ending) length = clock > bar ? (clock + bar - 1U) / bar * bar : bar;
        s->clip_length[grid.cell] = 0;
        if (began) song_clip_resize(s, grid.cell, length); /* journaled: the new clip comes and goes with its take */
    }
    grid.state = IDLE;
    song_undo_end(song_port_undo(), s);
}
/* 0 none, 1 armed (a launch, the count-in or the next pass to come), 2
 * recording, plus 4 while a FREE take has no end yet and 8 for a new clip. */
unsigned song_grid_rec_state(void)
{
    if (!grid.state) return 0;
    unsigned on = grid.state == TAKING && song_port_recorder()->active && song_port_live_clock() >= 0;
    return (on ? 2U : 1U) | (grid.open_end && !grid.ending ? 4U : 0U) | (grid.fresh ? 8U : 0U);
}
unsigned song_grid_rec_cell(void) { return grid.cell; }
#endif
