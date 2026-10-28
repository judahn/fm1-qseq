/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Song player. `pos` is song time at audio frame `frame`, in the units of
 * the current tempo segment (see song_time.c). Each advance moves it by
 * 480 * bpm100 units per frame, so no remainder is ever dropped. Tempo
 * changes, the loop end, a pending jump and the song end are edges: the
 * player stops at the first frame past an edge, emits what was due before
 * it, and carries the overshoot into the new position. */
#include "player.h"
#include <string.h>

enum { EDGE_TEMPO, EDGE_END, EDGE_LOOP, EDGE_JUMP };
#define NEVER 0xffffffffU

static void send(struct song_player *p, unsigned track, unsigned pitch, unsigned velocity)
{
    p->out.note(p->out.context, track, pitch, velocity);
}
static void release(struct song_player *p, unsigned track, unsigned i)
{
    send(p, track, p->held[track][i].pitch, 0);
    p->held[track][i] = p->held[track][--p->held_count[track]];
}
static void all_off(struct song_player *p)
{
    for (unsigned t = 0; t < SONG_TRACKS; ++t)
        while (p->held_count[t]) release(p, t, p->held_count[t] - 1U);
}
static void note_on(struct song_player *p, unsigned t, const struct song_event *e)
{
    /* A pitch that is still sounding ends first, so ons and offs pair up. */
    for (unsigned i = 0; i < p->held_count[t]; ++i)
        if (p->held[t][i].pitch == e->a) { release(p, t, i); break; }
    if (p->held_count[t] == SONG_HELD) {
        unsigned first = 0;
        for (unsigned i = 1; i < SONG_HELD; ++i)
            if (p->held[t][i].end < p->held[t][first].end) first = i;
        release(p, t, first);
    }
    p->held[t][p->held_count[t]++] = (struct song_held){song_event_start(e) + e->length, e->a};
    send(p, t, e->a, e->b);
}
/* Everything before clock `end`, in clock order per track. A note-off goes
 * before a note-on on the same clock. */
static void emit(struct song_player *p, uint32_t end)
{
    for (unsigned t = 0; t < SONG_TRACKS; ++t) {
        unsigned count;
        const struct song_event *e = song_track(p->song, t, &count);
        for (;;) {
            uint32_t on = p->cursor[t] < count ? song_event_start(&e[p->cursor[t]]) : NEVER, off = NEVER;
            unsigned first = 0;
            for (unsigned i = 0; i < p->held_count[t]; ++i)
                if (p->held[t][i].end < off) { off = p->held[t][i].end; first = i; }
            if (off <= on && off < end) release(p, t, first);
            else if (on < end) {
                if (song_event_kind(&e[p->cursor[t]]) == SONG_NOTE) note_on(p, t, &e[p->cursor[t]]);
                ++p->cursor[t];
            } else break;
        }
    }
    if (end > p->next) p->next = end;
}
/* Continue from `clock`, `over` units (at `rate`) after it. */
static void relocate(struct song_player *p, uint32_t clock, uint64_t over, uint32_t rate)
{
    const struct song *s = p->song;
    unsigned k = song_tempo_at(s, clock);
    for (unsigned t = 0; t < SONG_TRACKS; ++t) p->cursor[t] = (uint16_t)song_find(s, t, clock);
    p->next = clock;
    p->segment = (uint8_t)k;
    p->pos = song_units(s, k, clock) + (over ? song_rescale(over, song_rate(s->tempo[k].bpm100), rate) : 0);
}
static uint32_t bar_after(const struct song *s, uint32_t clock)
{
    uint32_t start, length;
    song_bar_at(s, clock, &start, &length);
    return start == clock ? clock : start + length;
}

void song_player_init(struct song_player *p, struct song *s, const struct song_output *out)
{
    memset(p, 0, sizeof *p);
    p->song = s;
    p->out = *out;
}
void song_player_start(struct song_player *p, uint64_t frame)
{
    if (p->playing) return;
    if (p->playhead >= song_end(p->song)) p->playhead = 0;
    relocate(p, p->playhead, 0, 0);
    p->frame = frame;
    p->playing = 1;
    p->jumping = 0;
}
void song_player_stop(struct song_player *p)
{
    if (!p->playing) return;
    p->playhead = song_player_clock(p);
    all_off(p);
    p->playing = p->jumping = 0;
}
void song_player_jump(struct song_player *p, uint32_t clock)
{
    uint32_t end = song_end(p->song);
    if (clock > end) clock = end;
    if (!p->playing) { p->playhead = clock; return; }
    p->jump = clock;
    p->jumping = 1;
    p->jump_at = bar_after(p->song, p->next);
}
uint32_t song_player_clock(const struct song_player *p)
{
    if (!p->playing) return p->playhead;
    const struct song *s = p->song;
    unsigned k = p->segment;
    return s->tempo[k].clock + (uint32_t)((p->pos - s->tempo_units[k]) / SONG_UNITS_PER_CLOCK);
}
uint32_t song_player_target(const struct song_player *p)
{
    if (!p->playing) return p->playhead;
    return p->jumping ? p->jump : bar_after(p->song, p->next);
}

void song_player_advance(struct song_player *p, uint64_t frame)
{
    /* Runs once more at frame == p->frame: an edge in the last frame leaves
     * events already due at the new position, and they belong to this block. */
    while (p->playing && p->frame <= frame) {
        const struct song *s = p->song;
        unsigned k = p->segment, kind = EDGE_END;
        uint32_t rate = song_rate(s->tempo[k].bpm100), edge = NEVER, end = song_end(s);
        uint64_t target = p->pos + (frame - p->frame) * rate;
        /* The nearest edge ahead. On a tie the later kind wins. */
        if (k + 1U < s->tempos) { edge = s->tempo[k + 1].clock; kind = EDGE_TEMPO; }
        if (end <= edge) { edge = end; kind = EDGE_END; }
        if (s->loop_on && s->loop_start < s->loop_end && p->next <= s->loop_end && s->loop_end <= edge) {
            edge = s->loop_end;
            kind = EDGE_LOOP;
        }
        if (p->jumping && p->jump_at <= edge) { edge = p->jump_at; kind = EDGE_JUMP; }
        uint64_t at = edge > s->tempo[k].clock ? song_units(s, k, edge) : s->tempo_units[k];
        if (target <= at) {
            /* No edge yet: emit every clock that starts before target. */
            uint64_t span = target - s->tempo_units[k];
            emit(p, s->tempo[k].clock + (uint32_t)((span + SONG_UNITS_PER_CLOCK - 1U) / SONG_UNITS_PER_CLOCK));
            p->pos = target;
            p->frame = frame;
            return;
        }
        emit(p, edge);
        /* Stop at the first frame past the edge and keep the overshoot. */
        uint64_t steps = p->pos > at ? 0 : (at - p->pos) / rate + 1, now = p->pos + steps * rate;
        p->frame += steps;
        if (kind == EDGE_TEMPO) {
            p->segment = (uint8_t)(k + 1U);
            p->pos = song_rescale(now, song_rate(s->tempo[k + 1].bpm100), rate);
            continue;
        }
        all_off(p);
        if (kind == EDGE_END) {
            p->playing = p->jumping = 0;
            p->playhead = 0;
            return;
        }
        relocate(p, kind == EDGE_JUMP ? p->jump : s->loop_start, now - at, rate);
        if (kind == EDGE_JUMP) p->jumping = 0;
        else if (p->jumping) p->jump_at = bar_after(s, p->next);
    }
}
