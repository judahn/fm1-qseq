/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* song-mode: the selection and arranging in the song view (design 6a). The
 * loop region is the selection. It covers the current track and follows it,
 * until tracks are picked by hand: hold LFO and tap F3 to F4 for tracks 1
 * to 8, G4 for all (again for just the current one). SHIFT plus the lower
 * white keys copy, cut, paste (A3 and B3 both, over what's there),
 * duplicate, insert bars and delete the selection, and SHIFT+D#4 clears
 * the clipboard. */
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
    static const char *const done[7] = {"COPIED", "CUT", "PASTED", "PASTED", "DUPLICATED", "BARS INSERTED", "DELETED"};
    struct song *s = song_port_song();
    const struct song_player *p = song_port_player();
    struct song_clip *c = song_port_clip();
    uint32_t at = p->playing ? song_player_clock(p) : p->playhead;
    if (s->open && (which == 4 || which == 5)) { /* a clip keeps its length: no duplicate or inserted bars */
        notice = "NOT INSIDE A CLIP";
        notice_until = now + 1500U;
        return;
    }
    struct song_place place;
    song_player_hold(song_port_player(), &place); /* playing goes on in time */
    int r;
    /* Only inserting bars moves what follows (0.5.5). Paste and duplicate
     * go over it, and delete leaves the selected tracks empty there. */
    if (which == 0) r = song_copy(s, c, song_selected(), s->loop_start, s->loop_end);
    else if (which == 1) r = song_cut(s, c, song_selected(), s->loop_start, s->loop_end);
    else if (which <= 3 && c->cell && !s->open) { /* a clip from the grid: a block, selected so Duplicate extends it */
        r = song_block_paste(s, c, at);
        if (!r) { song_grow(s, at + c->length); s->loop_start = at; s->loop_end = at + c->length; }
    } else if (which <= 3) r = c->length ? song_paste(s, c, at, track, 0) : -1;
    else if (which == 4) {
        r = song_duplicate(s, song_selected(), &s->loop_start, &s->loop_end);
        song_blocks_join(s, song_selected());
    }
    else if (which == 5) r = song_insert_time(s, s->loop_start, s->loop_end - s->loop_start);
    else r = song_erase(s, song_selected(), s->loop_start, s->loop_end);
    song_player_resume(song_port_player(), &place);
    notice = !r ? done[which] : which >= 2 && which <= 3 && !c->length ? "COPY SOMETHING FIRST"
        : r == -2 ? "THAT CLIP IS IN ANOTHER SONG" : which == 6 ? "NOTHING TO DELETE" : "DOESN'T FIT";
    notice_until = now + 1500U;
}
/* SHIFT+D#4, beside delete, in the song view and the grid (0.5.4): the
 * clipboard emptied, and the song view's paste outline with it. */
static void song_clipboard_clear(uint32_t now)
{
    struct song_clip *c = song_port_clip();
    notice = c->length ? "CLIPBOARD CLEARED" : "NOTHING COPIED";
    notice_until = now + 1500U;
    c->length = 0;
    c->count = 0;
    c->tracks = 0;
    c->cell = 0;
}
/* SHIFT+K3: the marker section around the playhead, then the next ones. */
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
    int r = song_port_recording() ? 1 : redo ? song_redo(song_port_undo(), song_port_song())
                                                    : song_undo(song_port_undo(), song_port_song());
    song_player_resume(song_port_player(), &place);
    song_ask_close(); /* what a question was about may have changed */
    if (r > 0) notice = "STOP RECORDING FIRST";
    else if (r) notice = redo ? "NOTHING TO REDO" : "NOTHING TO UNDO";
    else { notice = redo ? "REDONE" : "UNDONE"; song_port_recorder()->taken = 0; } /* the take can't be requantized now */
    notice_until = now + 1500U;
}
/* SCL, stopped: removes the markers at the playhead (SHIFT+PRESETS lands
 * on one exactly), or drops one there. Playing: drops one at the snap step
 * nearest where the song is, and never removes. A new one takes the first
 * free name of M1 to M16. */
static void song_marker_key(uint32_t now)
{
    struct song *s = song_port_song();
    struct song_player *p = song_port_player();
    uint32_t at = p->playhead;
    if (p->playing) {
        uint32_t clock = song_player_clock(p), down = song_snap_move(s, clock + 1U, song_zoom, -1);
        uint32_t up = song_snap_move(s, down, song_zoom, 1);
        at = up > clock && up - clock < clock - down ? up : down;
    }
    unsigned there = 0;
    for (unsigned i = 0; i < s->markers; ++i) there |= s->marker[i].clock == at;
    notice_until = now + 1500U;
    if (there) {
        if (p->playing) notice = "MARKER ALREADY THERE";
        else { (void)song_remove_marker(s, at); notice = "MARKER REMOVED"; }
        return;
    }
    char name[4] = {'M', 0, 0, 0};
    for (unsigned n = 1, i = 0; n <= SONG_MARKERS && i < s->markers; ++n) {
        name[1] = (char)(n < 10 ? '0' + n : '1');
        name[2] = (char)(n < 10 ? 0 : '0' + n - 10);
        i = 0;
        while (i < s->markers && strncmp(s->marker[i].name, name, SONG_MARKER_NAME)) ++i;
    }
    if (!name[1]) name[1] = '1';
    notice = song_add_marker(s, at, name) ? "MARKERS FULL" : "MARKER ADDED";
}
