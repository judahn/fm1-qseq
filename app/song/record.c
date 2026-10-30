/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Real-time recording. See record.h. */
#include "record.h"
#include "song_config.h"
#include <string.h>
#if SONG_UI

SONG_SMALL uint32_t song_quantize_grid(const struct song *s)
{
    static const uint16_t base[7] = {30, 60, 120, 240, 480, 960, 1920};
    if (!s->quantize || s->quantize > 14) return 0;
    unsigned q = s->quantize - 1U;
    return base[q / 2U] * (q & 1U ? 2U : 3U) / 3U;
}
/* The nearest grid line in the note's bar, then `strength` percent of the
 * way there, rounded to the nearest clock. */
SONG_SMALL uint32_t song_quantize(const struct song *s, uint32_t clock)
{
    uint32_t grid = song_quantize_grid(s), start;
    if (!grid) return clock;
    song_bar_at(s, clock, &start, NULL);
    int64_t target = start + (clock - start + grid / 2U) / grid * grid, move = (target - clock) * s->strength;
    return (uint32_t)((int64_t)clock + (move >= 0 ? move + 50 : move - 50) / 100);
}
/* The catch: 100 ms at the tempo at `clock` (480 clocks a beat), but less
 * than a 16th, so a 16th pickup stays a pickup at any tempo, or with input
 * quantize on half its grid when that is wider. */
SONG_SMALL uint32_t song_rec_window(const struct song *s, uint32_t clock)
{
    uint32_t w = s->tempo[song_tempo_at(s, clock)].bpm100 / 125U, half = song_quantize_grid(s) / 2U;
    if (w > 90U) w = 90U;
    return half > w ? half : w;
}
/* A loop take's key this close before the seam is the next pass's. At
 * most half the loop is caught. */
SONG_SMALL static int at_seam(const struct song *s, uint32_t first)
{
    if (s->loop_end <= s->loop_start || first < s->loop_start || first >= s->loop_end) return 0;
    uint32_t w = song_rec_window(s, s->loop_end - 1U), half = (s->loop_end - s->loop_start) / 2U;
    return first + (w < half ? w : half) >= s->loop_end;
}

SONG_SMALL void song_rec_start(struct song_recorder *r, struct song *s, unsigned track, unsigned mode, uint32_t from)
{
    r->song = s;
    r->track = (uint8_t)(track < SONG_TRACKS ? track : 0);
    r->mode = (uint8_t)mode;
    r->from = r->last = r->reach = from;
    r->taken = r->held_count = r->passes = 0;
    r->active = 1;
}
SONG_SMALL int song_rec_quiet(const struct song_recorder *r, uint32_t *from, uint32_t *to)
{
    if (!r->active || (r->mode != SONG_REC_REPLACE && r->mode != SONG_REC_PUNCH)) return 0;
    *from = r->mode == SONG_REC_PUNCH ? r->song->loop_start : r->from;
    *to = r->mode == SONG_REC_PUNCH ? r->song->loop_end : 0xffffffffU;
    return 1;
}
SONG_SMALL static int at_pitch(const struct song *s, unsigned track, uint32_t clock, unsigned pitch)
{
    unsigned count, i = song_find(s, track, clock);
    const struct song_event *e = song_track(s, track, &count);
    for (; i < count && song_event_start(&e[i]) == clock; ++i)
        if (song_is_note(&e[i]) && e[i].a == pitch) return (int)i;
    return -1;
}
/* A held note becomes a song note, ending at `off`. One caught before the
 * seam goes on the loop's first beat, as long as it was held. A note that
 * quantizes onto the seam goes round to the loop's start too. */
SONG_SMALL static void commit(struct song_recorder *r, unsigned held, uint32_t off)
{
    struct song *s = r->song;
    uint32_t on = r->held[held].at, length, lead = r->held[held].lead, at;
    if (r->held[held].caught) {
        length = off >= on ? off - on : off > s->loop_start ? off - s->loop_start : 1;
        at = on = s->loop_start + lead;
    } else {
        if (off >= on) length = off - on;
        else length = s->loop_end > on ? s->loop_end - on : 1; /* past a loop seam */
        at = song_quantize(s, on - lead) + lead; /* a chord moves as a whole, by its first note */
        if (r->mode == SONG_REC_LOOP && on - lead < s->loop_end && at - lead >= s->loop_end) at = s->loop_start + lead;
    }
    if (r->mode == SONG_REC_PUNCH && at + length > s->loop_end) length = s->loop_end > at ? s->loop_end - at : 1;
    length = length < 1 ? 1 : length > 0xffff ? 0xffff : length;
    int same = at_pitch(s, r->track, at, r->held[held].pitch); /* the same note again replaces it */
    if (same >= 0) (void)song_remove(s, r->track, (unsigned)same);
    const struct song_event e = {at | SONG_FRESH, r->held[held].pitch, r->held[held].velocity, (uint16_t)length};
    if (!song_add_event(s, r->track, &e) && r->taken < SONG_TAKE)
        r->take[r->taken++] = (struct song_take_note){on | lead << 24, at, (uint16_t)length, e.a, e.b};
    r->held[held] = r->held[--r->held_count];
}
SONG_SMALL static void reach(struct song_recorder *r, uint32_t clock)
{
    r->last = clock;
    if (clock > r->reach) r->reach = clock;
}
SONG_SMALL void song_rec_note(struct song_recorder *r, unsigned pitch, unsigned velocity, uint32_t clock)
{
    song_rec_lead(r, pitch, velocity, clock, 0);
}
SONG_SMALL void song_rec_drop(struct song_recorder *r, unsigned pitch)
{
    for (unsigned i = 0; i < r->held_count; ++i)
        if (r->held[i].pitch == pitch) { r->held[i] = r->held[--r->held_count]; return; }
}
SONG_SMALL void song_rec_lead(struct song_recorder *r, unsigned pitch, unsigned velocity, uint32_t clock, unsigned lead)
{
    if (!r->active || pitch > 127) return;
    const struct song *s = r->song;
    unsigned held = 0;
    while (held < r->held_count && r->held[held].pitch != pitch) ++held;
    if (!velocity) {
        if (held < r->held_count) commit(r, held, clock);
    } else if (held == r->held_count && r->held_count < SONG_REC_HELD &&
               (r->mode != SONG_REC_REPLACE || clock >= r->from) &&
               (r->mode != SONG_REC_PUNCH || (clock >= s->loop_start && clock < s->loop_end))) {
        r->held[r->held_count].at = clock;
        r->held[r->held_count].lead = (uint8_t)(lead < clock ? lead : clock);
        r->held[r->held_count].caught = (uint8_t)(r->mode == SONG_REC_LOOP && at_seam(s, clock - r->held[r->held_count].lead));
        r->held[r->held_count].pitch = (uint8_t)pitch;
        r->held[r->held_count++].velocity = (uint8_t)velocity;
    }
    reach(r, clock);
}
SONG_SMALL void song_rec_let_go(struct song_recorder *r, unsigned pitch, uint32_t length)
{
    for (unsigned i = 0; r->active && i < r->held_count; ++i)
        if (r->held[i].pitch == pitch) { commit(r, i, r->held[i].at + length); return; }
}
SONG_SMALL void song_rec_early(struct song_recorder *r, unsigned pitch, unsigned velocity, unsigned lead, uint32_t length)
{
    uint32_t last = r->last, furthest = r->reach; /* not a clock the take has reached */
    song_rec_lead(r, pitch, velocity, r->from + lead, lead);
    r->last = last; r->reach = furthest;
    if (length) song_rec_let_go(r, pitch, length);
}
/* A loop pass ends when the clock goes back. Keys still down end at the
 * seam, but caught ones carry on from the new pass's first beat, and the
 * new pass keeps its own raw timing. */
SONG_SMALL void song_rec_follow(struct song_recorder *r, uint32_t clock)
{
    if (!r->active) return;
    if (r->mode == SONG_REC_LOOP && clock < r->last) {
        for (unsigned i = r->held_count; i-- > 0;) {
            if (!r->held[i].caught) { commit(r, i, r->song->loop_end); continue; }
            r->held[i].caught = 0;
            r->held[i].at = r->song->loop_start + r->held[i].lead;
        }
        r->taken = 0;
        ++r->passes;
    }
    reach(r, clock);
}
SONG_SMALL void song_rec_stop(struct song_recorder *r, uint32_t clock)
{
    if (!r->active) return;
    struct song *s = r->song;
    reach(r, clock);
    while (r->held_count) commit(r, r->held_count - 1U, clock);
    uint32_t from, to;
    if (song_rec_quiet(r, &from, &to)) {
        if (to > r->reach) to = r->reach; /* only what playback passed */
        unsigned count, first = song_find(s, r->track, from);
        const struct song_event *e = song_track(s, r->track, &count);
        for (unsigned i = song_find(s, r->track, to); i-- > first;)
            if (!(e[i].at & SONG_FRESH) && song_is_note(&e[i])) (void)song_remove(s, r->track, i); /* automation stays */
    }
    unsigned count;
    song_track(s, r->track, &count);
    for (unsigned i = 0; i < count; ++i) s->event[s->first[r->track] + i].at &= ~SONG_FRESH;
    r->seam_start = s->loop_start; r->seam_end = s->loop_end;
    r->active = 0;
}
SONG_SMALL int song_rec_requantize(struct song_recorder *r)
{
    struct song *s = r->song;
    int moved = 0;
    for (unsigned k = 0; s && !r->active && k < r->taken; ++k) {
        struct song_take_note *n = &r->take[k];
        uint32_t raw = n->raw & 0xffffffU, lead = n->raw >> 24, at = song_quantize(s, raw - lead) + lead;
        if (r->mode == SONG_REC_LOOP && raw - lead < r->seam_end && at - lead >= r->seam_end) at = r->seam_start + lead; /* round the loop */
        int i = at_pitch(s, r->track, n->at, n->pitch);
        if (at == n->at || i < 0 || at_pitch(s, r->track, at, n->pitch) >= 0) continue;
        struct song_event e = s->event[s->first[r->track] + (unsigned)i];
        e.at = (e.at & 0xff000000U) | at;
        if (!song_set_event(s, r->track, (unsigned)i, &e)) { n->at = at; ++moved; }
    }
    return moved;
}
#endif
