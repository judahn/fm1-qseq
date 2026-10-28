/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* song-mode: the selection and arranging in the song view (design 6a). The
 * loop region is the selection. It covers the current track and follows it,
 * until tracks are picked by hand: hold LFO and tap F3 to F4 for tracks 1
 * to 8, G4 for all (again for just the current one). SHIFT plus the lower
 * white keys copy, cut, paste (inserting or over), duplicate, insert bars
 * and delete bars. */
#include "arrange.h"
#include "blocks.h"
#include "record.h"
#include "undo.h"

static const char *const song_rec_modes[SONG_REC_MODES] = {"STEP", "REPLACE", "OVERDUB", "PUNCH", "LOOP"};
static uint8_t song_sel, song_sel_hand;
static unsigned song_selected(void) { return song_sel_hand && !song_port_song()->open ? song_sel : 1U << track; }

static void song_pick_tracks(uint32_t down)
{
    for (unsigned t = 0; t < SONG_TRACKS; ++t)
        if (down & (1U << white_keys[t])) {
            if (!song_sel_hand) song_sel = (uint8_t)(1U << track);
            song_sel ^= (uint8_t)(1U << t);
            song_sel_hand = song_sel != 0;
        }
    if (down & (1U << white_keys[8])) {
        if (song_sel_hand && song_sel == 0xff) song_sel_hand = 0;
        else { song_sel = 0xff; song_sel_hand = 1; }
    }
}
static void song_action(unsigned which, uint32_t now)
{
    static const char *const done[7] = {"COPIED", "CUT", "PASTED, INSERTING", "PASTED OVER",
                                        "DUPLICATED", "BARS INSERTED", "BARS DELETED"};
    struct song *s = song_port_song();
    const struct song_player *p = song_port_player();
    struct song_clip *c = song_port_clip();
    uint32_t at = p->playing ? song_player_clock(p) : p->playhead;
    if (s->open && (which == 2 || which > 3)) { /* a clip keeps its length: copy, cut and paste over only */
        notice = "NOT INSIDE A CLIP";
        notice_until = now + 1500U;
        return;
    }
    struct song_place place;
    song_player_hold(song_port_player(), &place); /* playing goes on in time */
    int r;
    if (which == 0) r = song_copy(s, c, song_selected(), s->loop_start, s->loop_end);
    else if (which == 1) r = song_cut(s, c, song_selected(), s->loop_start, s->loop_end);
    else if (which <= 3 && c->cell && !s->open) { /* a clip from the grid: a block, selected so Duplicate extends it */
        r = which == 2 && song_insert_time(s, at, c->length) ? -1 : song_block_paste(s, c, at);
        if (!r) { s->loop_start = at; s->loop_end = at + c->length; }
    } else if (which <= 3) r = c->length ? song_paste(s, c, at, track, which == 2) : -1;
    else if (which == 4) {
        r = song_duplicate(s, song_selected(), &s->loop_start, &s->loop_end);
        song_blocks_join(s, song_selected());
    }
    else if (which == 5) r = song_insert_time(s, s->loop_start, s->loop_end - s->loop_start);
    else r = song_delete_time(s, s->loop_start, s->loop_end);
    song_player_resume(song_port_player(), &place);
    notice = !r ? done[which] : which >= 2 && which <= 3 && !c->length ? "COPY SOMETHING FIRST"
        : r == -2 ? "THAT CLIP IS IN ANOTHER SONG" : "DOESN'T FIT";
    notice_until = now + 1500U;
}
/* SHIFT+K2: the marker section around the playhead, then the next ones. */
static void song_sections(int delta)
{
    struct song *s = song_port_song();
    uint32_t at = song_port_player()->playhead, end = song_end(s);
    int before = song_marker_step(s, at + 1U, -1), after = song_marker_step(s, at, 1);
    uint32_t start = before >= 0 ? s->marker[before].clock : 0;
    if (delta < 0) {
        int back = song_marker_step(s, s->loop_end, -1);
        if (back >= 0 && s->marker[back].clock > s->loop_start) s->loop_end = s->marker[back].clock;
    } else if (s->loop_start == start && s->loop_end > at) {
        int next = song_marker_step(s, s->loop_end, 1);
        s->loop_end = next >= 0 ? s->marker[next].clock : end;
    } else {
        s->loop_start = start;
        s->loop_end = after >= 0 ? s->marker[after].clock : end;
    }
}
/* HOME+SAVE undoes, with SHIFT it redoes. The two never go down in the
 * same poll, and the first one already leaves the page, so the song page
 * they started from is remembered here. */
static unsigned song_combo_from;
static void song_undo_key(uint32_t buttons, uint32_t now)
{
    unsigned redo = (buttons & BUTTON(FX)) != 0;
    struct song_place place;
    song_player_hold(song_port_player(), &place);
    int r = song_port_recorder()->active ? 1 : redo ? song_redo(song_port_undo(), song_port_song())
                                                    : song_undo(song_port_undo(), song_port_song());
    song_player_resume(song_port_player(), &place);
    if (r > 0) notice = "STOP RECORDING FIRST";
    else if (r) notice = redo ? "NOTHING TO REDO" : "NOTHING TO UNDO";
    else { notice = redo ? "REDONE" : "UNDONE"; song_port_recorder()->taken = 0; } /* the take can't be requantized now */
    notice_until = now + 1500U;
}
/* SCL drops a marker at the playhead. */
static void song_add_marker_here(uint32_t now)
{
    struct song *s = song_port_song();
    char name[4] = {'M', (char)('1' + s->markers % 9U), 0, 0};
    if (s->markers >= 9) { name[1] = '1'; name[2] = (char)('0' + s->markers - 9U); }
    notice = song_add_marker(s, song_port_player()->playhead, name) ? "MARKERS FULL" : "MARKER ADDED";
    notice_until = now + 1500U;
}
