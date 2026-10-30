/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Step entry's swept selection (0.5.0). Holding LFO and turning K4 moves
 * the pointer from an anchor, and the selection becomes every note that
 * starts from the anchor to the pointer, both included (a bar swept ends
 * with the pointer on its last step, in the span shown), at every pitch
 * until keys pick some (step_pick). SEL, the knobs and glitch then change all of it,
 * each change one undo step. A change undo couldn't hold is refused whole:
 * the edit jobs reckon 24 bytes a note and 512 to spare, and a glitch
 * table change adds the maps. See step.h. */
#include "step.h"
#include "glitch.h"
#include "undo.h"
#include "song_config.h"
#if SONG_UI

SONG_SMALL int step_range(const struct song_step *st, uint32_t *from, uint32_t *to)
{
    if (!st->ranged || st->anchor == st->pointer) return 0;
    *from = st->anchor < st->pointer ? st->anchor : st->pointer;
    *to = (st->anchor < st->pointer ? st->pointer : st->anchor) + 1U; /* the notes at both ends are in */
    return 1;
}
SONG_SMALL int step_chosen(const struct song_step *st, const struct song_event *e)
{
    return song_is_note(e) && (!st->picked || (st->pick[e->a / 32U] >> (e->a % 32U) & 1U));
}
/* The swept events, as pool indices [*first, return). */
SONG_SMALL static unsigned swept(const struct song_step *st, unsigned *first)
{
    uint32_t from = 0, to = 0;
    step_range(st, &from, &to);
    *first = st->song->first[st->track] + song_find(st->song, st->track, from);
    return st->song->first[st->track] + song_find(st->song, st->track, to);
}
SONG_SMALL unsigned step_count(const struct song_step *st)
{
    unsigned index[STEP_CHORD], i, end, n = 0;
    uint32_t from, to;
    if (!step_range(st, &from, &to)) return step_selection(st, index);
    for (end = swept(st, &i); i < end; ++i) n += step_chosen(st, &st->song->event[i]) != 0;
    return n;
}
SONG_SMALL int step_too_big(const struct song_step *st)
{
    uint32_t from, to;
    return step_range(st, &from, &to) && step_count(st) * 24U + 512U + sizeof(struct song_maps) > SONG_UNDO_BYTES;
}
SONG_SMALL int step_range_delete(struct song_step *st)
{
    unsigned first, end;
    if (step_too_big(st)) return STEP_TOO_BIG;
    for (end = swept(st, &first); end-- > first;)
        if (step_chosen(st, &st->song->event[end])) (void)song_remove(st->song, st->track, end - st->song->first[st->track]);
    st->tie_ok = 0;
    return 0;
}
/* The knobs show the first note's velocity afterwards, as at the pointer. */
SONG_SMALL void step_range_velocity(struct song_step *st, int delta)
{
    unsigned i, end, shown = 0;
    for (end = swept(st, &i); i < end; ++i) {
        struct song_event e = st->song->event[i];
        if (!step_chosen(st, &e)) continue;
        int v = e.b + delta;
        e.b = (uint8_t)(v < 1 ? 1 : v > 127 ? 127 : v);
        step_change(st, i, &e);
        if (!shown++) { st->velocity = e.b; st->random_level = 0; }
    }
}
SONG_SMALL void step_range_scale(struct song_step *st, uint32_t before, uint32_t after)
{
    unsigned i, end;
    for (end = swept(st, &i); i < end; ++i) {
        struct song_event e = st->song->event[i];
        if (!step_chosen(st, &e) || !before) continue;
        uint32_t length = (e.length * after + before / 2U) / before;
        e.length = (uint16_t)(length < 1 ? 1 : length > 0xffff ? 0xffff : length);
        step_change(st, i, &e);
    }
}
SONG_SMALL int step_range_glitch(struct song_step *st, unsigned field, int delta)
{
    unsigned i, end, shown = 0;
    int result = 0;
    for (end = swept(st, &i); i < end; ++i) {
        if (!step_chosen(st, &st->song->event[i])) continue;
        uint32_t word = song_glitch_turn(song_glitch_word(st->song, &st->song->event[i]), field, delta);
        result |= song_glitch_apply(st->song, st->track, i - st->song->first[st->track], word);
        if (!shown++) st->glitch = song_glitch_word(st->song, &st->song->event[i]);
    }
    return result;
}
#endif
