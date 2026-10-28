/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Song player. `pos` is song time at audio frame `frame`, in the units of
 * the current tempo segment (see song_time.c). Each advance moves it by
 * 480 * bpm100 units per frame, so no remainder is ever dropped. Tempo
 * changes, the loop end, a pending jump and the song end are edges: the
 * player stops at the first frame past an edge, emits what was due before
 * it, and carries the overshoot into the new position. */
#include "player.h"
#include "song_config.h"
#include <string.h>

enum { EDGE_TEMPO, EDGE_END, EDGE_LOOP, EDGE_JUMP, CLICK = 37 /* rim */ };
#define NEVER 0xffffffffU
#if SONG_UI
#define QUIET(p, t, e) ((t) == (p)->mute_track && !((e)->at & SONG_FRESH) && \
    song_event_start(e) >= (p)->mute_from && song_event_start(e) < (p)->mute_to)
#else
#define QUIET(p, t, e) 0
#endif

SONG_SMALL static void send(struct song_player *p, unsigned track, unsigned pitch, unsigned velocity)
{
    p->out.note(p->out.context, track, pitch, velocity);
}
SONG_SMALL static void release(struct song_player *p, unsigned track, unsigned i)
{
    send(p, track, p->held[track][i].pitch, 0);
    p->held[track][i] = p->held[track][--p->held_count[track]];
}
#include "player_take.inc" /* metronome, count-in, live clock */
SONG_SMALL static void all_off(struct song_player *p)
{
    for (unsigned t = 0; t < SONG_TRACKS; ++t)
        while (p->held_count[t]) release(p, t, p->held_count[t] - 1U);
#if SONG_UI
    click_off(p);
#endif
}
SONG_SMALL static void note_on(struct song_player *p, unsigned t, const struct song_event *e, uint32_t at)
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
    p->held[t][p->held_count[t]++] = (struct song_held){at + e->length, e->a};
    send(p, t, e->a, e->b);
}
SONG_SMALL static uint32_t onset(const struct song_player *p, unsigned t, const struct song_event *e)
{
    return song_event_start(e) + song_swing(p->song, t, song_event_start(e));
}
/* Everything that starts before clock `end` once swing is applied, in clock
 * order per track. A note-off goes before a note-on on the same clock. A
 * swung off-beat due after `end` waits as a group (all at one clock), so the
 * notes between it and its swung time are not held back. */
SONG_SMALL static void emit(struct song_player *p, uint32_t end)
{
#if SONG_UI
    clicks(p, end);
#endif
    for (unsigned t = 0; t < SONG_TRACKS; ++t) {
        unsigned count;
        const struct song_event *e = song_track(p->song, t, &count);
        for (;;) {
            uint32_t late = p->late_count[t] ? onset(p, t, &e[p->late_first[t]]) : NEVER;
            while (late >= end && p->cursor[t] < count && song_event_start(&e[p->cursor[t]]) < end &&
                   onset(p, t, &e[p->cursor[t]]) >= end &&
                   (!p->late_count[t] || p->late_first[t] + p->late_count[t] == p->cursor[t])) {
                if (!p->late_count[t]) p->late_first[t] = p->cursor[t];
                ++p->late_count[t];
                ++p->cursor[t];
                late = onset(p, t, &e[p->late_first[t]]);
            }
            uint32_t on = p->cursor[t] < count ? song_event_start(&e[p->cursor[t]]) : NEVER, off = NEVER;
            if (on < end) on = onset(p, t, &e[p->cursor[t]]);
            unsigned first = 0;
            for (unsigned i = 0; i < p->held_count[t]; ++i)
                if (p->held[t][i].end < off) { off = p->held[t][i].end; first = i; }
            if (off <= on && off <= late && off < end) release(p, t, first);
            else if (late <= on && late < end) {
                if (song_event_kind(&e[p->late_first[t]]) == SONG_NOTE && !QUIET(p, t, &e[p->late_first[t]]))
                    note_on(p, t, &e[p->late_first[t]], late);
                ++p->late_first[t];
                --p->late_count[t];
            } else if (on < end) {
                if (song_event_kind(&e[p->cursor[t]]) == SONG_NOTE && !QUIET(p, t, &e[p->cursor[t]]))
                    note_on(p, t, &e[p->cursor[t]], on);
                ++p->cursor[t];
            } else break;
        }
    }
    if (end > p->next) p->next = end;
}
/* Continue from `clock`, `over` units (at `rate`) after it. */
SONG_SMALL static void relocate(struct song_player *p, uint32_t clock, uint64_t over, uint32_t rate)
{
    const struct song *s = p->song;
    unsigned k = song_tempo_at(s, clock);
    for (unsigned t = 0; t < SONG_TRACKS; ++t) p->cursor[t] = (uint16_t)song_find(s, t, clock);
    memset(p->late_count, 0, sizeof p->late_count); /* swung past an edge: dropped */
    p->next = clock;
    p->revision = s->revision;
    p->segment = (uint8_t)k;
    p->pos = song_units(s, k, clock) + (over ? song_rescale(over, song_rate(s->tempo[k].bpm100), rate) : 0);
#if SONG_UI
    click_off(p);
    p->click_next = next_beat(s, clock);
#endif
}
SONG_SMALL static uint32_t bar_after(const struct song *s, uint32_t clock)
{
    uint32_t start, length;
    song_bar_at(s, clock, &start, &length);
    return start == clock ? clock : start + length;
}

SONG_SMALL void song_player_init(struct song_player *p, struct song *s, const struct song_output *out)
{
    memset(p, 0, sizeof *p);
    p->song = s;
    p->out = *out;
#if SONG_UI
    p->mute_track = 0xff;
    p->click_track = SONG_TRACKS - 1U; /* the 808 kit in the default project */
    p->click_off = NEVER;
#endif
}
SONG_SMALL void song_player_start(struct song_player *p, uint64_t frame)
{
    if (p->playing) return;
    if (p->playhead >= song_end(p->song)) p->playhead = 0;
    relocate(p, p->playhead, 0, 0);
    p->frame = frame;
    p->playing = 1;
    p->jumping = 0;
#if SONG_UI
    p->pre_left = 0;
#endif
}
SONG_SMALL void song_player_stop(struct song_player *p)
{
    if (!p->playing) return;
    p->playhead = song_player_clock(p);
    all_off(p);
    p->playing = p->jumping = 0;
}
SONG_SMALL void song_player_jump(struct song_player *p, uint32_t clock)
{
    uint32_t end = song_end(p->song);
    if (clock > end) clock = end;
    if (!p->playing) { p->playhead = clock; return; }
    p->jump = clock;
    p->jumping = 1;
    p->jump_at = bar_after(p->song, p->next);
}
SONG_SMALL uint32_t song_player_clock(const struct song_player *p)
{
    if (!p->playing) return p->playhead;
    const struct song *s = p->song;
    unsigned k = p->segment;
    return s->tempo[k].clock + (uint32_t)((p->pos - s->tempo_units[k]) / SONG_UNITS_PER_CLOCK);
}
SONG_SMALL int song_player_set_tempo(struct song_player *p, uint32_t clock, unsigned bpm100)
{
    struct song *s = p->song;
    if (!p->playing) return song_set_tempo(s, clock, bpm100);
    /* Keep the current clock and the time already spent inside it. */
    uint32_t at = song_player_clock(p), rate = song_rate(s->tempo[p->segment].bpm100);
    uint64_t inside = p->pos - song_units(s, p->segment, at);
    if (song_set_tempo(s, clock, bpm100)) return -1;
    p->segment = (uint8_t)song_tempo_at(s, at);
    p->pos = song_units(s, p->segment, at) + song_rescale(inside, song_rate(s->tempo[p->segment].bpm100), rate);
    return 0;
}
SONG_SMALL uint32_t song_player_target(const struct song_player *p)
{
    if (!p->playing) return p->playhead;
    return p->jumping ? p->jump : bar_after(p->song, p->next);
}

SONG_SMALL void song_player_advance(struct song_player *p, uint64_t frame)
{
    /* Runs once more at frame == p->frame: an edge in the last frame leaves
     * events already due at the new position, and they belong to this block. */
    if (p->playing && p->revision != p->song->revision) {
        /* The notes changed under playback: find our place again. */
        for (unsigned t = 0; t < SONG_TRACKS; ++t) p->cursor[t] = (uint16_t)song_find(p->song, t, p->next);
        memset(p->late_count, 0, sizeof p->late_count);
        p->revision = p->song->revision;
    }
#if SONG_UI
    pre_roll(p, frame);
#endif
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
