/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Clips: the grid's cells, opening one in the editor, and the cell copy,
 * cut, paste and delete. See clips.h. The player plays them
 * (player_clips.inc). */
#include "clips.h"
#include "undo.h"
#include <string.h>
#if SONG_UI

SONG_SMALL uint32_t song_clip_max(const struct song *s) { return song_bar_clock(s, SONG_CLIP_BARS); }

SONG_SMALL static void reverse(struct song_event *e, unsigned n)
{
    for (; n > 1; ++e, n -= 2U) {
        struct song_event x = e[0];
        e[0] = e[n - 1U];
        e[n - 1U] = x;
    }
}
/* Parts a < b trade events in place: [a][between][b] becomes [b][between][a]. */
SONG_SMALL static void trade(struct song *s, unsigned a, unsigned b)
{
    struct song_event *e = s->event + s->first[a];
    unsigned na = s->count[a], nb = s->count[b], span = s->first[b] + nb - s->first[a];
    reverse(e, span);
    reverse(e, nb);
    reverse(e + nb, span - na - nb);
    reverse(e + span - na, na);
    for (unsigned p = a + 1U; p <= b; ++p) s->first[p] = (uint16_t)(s->first[p] + nb - na);
    s->count[a] = (uint16_t)nb;
    s->count[b] = (uint16_t)na;
    ++s->revision;
}

SONG_SMALL int song_clip_open(struct song *s, unsigned cell)
{
    if (s->open || cell >= SONG_CELLS) return -1;
    if (!s->clip_length[cell]) s->clip_length[cell] = (uint16_t)song_bar_clock(s, 1);
    s->kept_start = s->loop_start;
    s->kept_end = s->loop_end;
    s->kept_on = s->loop_on;
    s->loop_start = 0;
    s->loop_end = s->clip_length[cell];
    s->loop_on = 1;
    trade(s, cell / SONG_GRID_ROWS, song_cell_part(cell));
    s->open = (uint8_t)song_cell_part(cell);
    return 0;
}
SONG_SMALL void song_clip_follow(struct song *s)
{
    if (!s->open) return;
    uint32_t length = s->loop_end, beat = SONG_WHOLE / s->meter[0].unit, most = song_clip_max(s);
    s->clip_length[s->open - SONG_TRACKS] = (uint16_t)(length < beat ? beat : length > most ? most : length);
}
SONG_SMALL void song_clip_close(struct song *s)
{
    if (!s->open) return;
    unsigned cell = s->open - SONG_TRACKS;
    song_clip_follow(s);
    s->open = 0;
    trade(s, cell / SONG_GRID_ROWS, song_cell_part(cell));
    s->loop_start = s->kept_start;
    s->loop_end = s->kept_end;
    s->loop_on = s->kept_on;
}

SONG_SMALL int song_clip_copy(const struct song *s, struct song_clip *board, unsigned cell)
{
    unsigned part = song_owner(s, song_cell_part(cell)), count, track = cell / SONG_GRID_ROWS;
    uint32_t length = cell < SONG_CELLS ? s->clip_length[cell] : 0;
    const struct song_event *e = song_track(s, part, &count);
    unsigned n = song_find(s, part, length); /* past its end, a clip keeps what it hides */
    if (!length || n > SONG_CLIP_EVENTS) return -1;
    board->length = length;
    board->count = (uint16_t)n;
    board->tracks = (uint8_t)(1U << track);
    board->cell = (uint8_t)(cell + 1U);
    memcpy(board->glitch, s->glitch, sizeof board->glitch);
    for (unsigned i = 0; i < n; ++i) {
        board->event[i] = e[i];
        board->event[i].at &= ~SONG_FRESH;
        board->track[i] = (uint8_t)track;
    }
    return 0;
}
/* A cell's length, journaled. */
SONG_SMALL void song_clip_resize(struct song *s, unsigned cell, unsigned length)
{
    if (s->undo) {
        const struct song_event e = {s->clip_length[cell], 0, 0, (uint16_t)length};
        song_undo_record(s->undo, SONG_UNDO_LENGTH, cell, &e, 0);
    }
    s->clip_length[cell] = (uint16_t)length;
    ++s->revision;
}
SONG_SMALL static void empty(struct song *s, unsigned cell)
{
    unsigned part = song_owner(s, song_cell_part(cell));
    while (s->count[part]) (void)song_remove(s, part, s->count[part] - 1U);
}
SONG_SMALL int song_clip_empty(struct song *s, unsigned cell)
{
    if (cell >= SONG_CELLS || !s->clip_length[cell]) return -1;
    empty(s, cell);
    return 0;
}
SONG_SMALL int song_clip_delete(struct song *s, unsigned cell)
{
    if (song_clip_empty(s, cell)) return -1;
    song_clip_resize(s, cell, 0);
    return 0;
}
SONG_SMALL int song_clip_cut(struct song *s, struct song_clip *board, unsigned cell)
{
    return song_clip_copy(s, board, cell) ? -1 : song_clip_delete(s, cell);
}
SONG_SMALL int song_clip_paste(struct song *s, const struct song_clip *board, unsigned cell)
{
    unsigned row = cell % SONG_GRID_ROWS, single = board->tracks && !(board->tracks & (board->tracks - 1U));
    unsigned targets = single ? 1U << cell / SONG_GRID_ROWS : board->tracks, freed = 0, blocks = 0;
    if (cell >= SONG_CELLS || !board->length || board->length > song_clip_max(s)) return -1;
    for (unsigned t = 0; t < SONG_TRACKS; ++t)
        if (targets >> t & 1U) freed += s->count[song_owner(s, song_cell_part(song_cell(t, row)))];
    for (unsigned i = 0; i < board->count; ++i) blocks += song_is_block(&board->event[i]);
    if (s->events - freed + board->count - blocks > SONG_EVENTS) return -1;
    for (unsigned t = 0; t < SONG_TRACKS; ++t)
        if (targets >> t & 1U) {
            empty(s, song_cell(t, row));
            song_clip_resize(s, song_cell(t, row), board->length);
        }
    for (unsigned i = 0; i < board->count; ++i) {
        struct song_event e = song_board_event(s, board, i, 0);
        unsigned t = single ? cell / SONG_GRID_ROWS : board->track[i];
        if (!song_is_block(&e)) /* blocks stay with the song */
            (void)song_add_event(s, song_owner(s, song_cell_part(song_cell(t, row))), &e);
    }
    return 0;
}
#endif
