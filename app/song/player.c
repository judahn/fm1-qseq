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
#include "lanes.h"
#include "glitch.h"
#define QUIET(p, t, e) ((t) == (p)->mute_track && !((e)->at & SONG_FRESH) && \
    song_event_start(e) >= (p)->mute_from && song_event_start(e) < (p)->mute_to)
#define PLAY(p, t, e, at) play(p, t, e, at)
/* A track plays the song unless it is on a clip or stopped. The clip open
 * in the editor plays on its track, in its part, while the song plays on. */
SONG_SMALL static int on_song(const struct song_player *p, unsigned t)
{
    return t != song_editing(p->song) && p->from[t] == SONG_FROM_SONG;
}
/* The song's clock, playing or stopped. */
SONG_SMALL static uint32_t timeline(const struct song_player *p)
{
    const struct song *s = p->song;
    if (!p->playing) return s->open ? p->song_at : p->playhead;
    unsigned k = p->segment;
    return s->tempo[k].clock + (uint32_t)((p->pos - s->tempo_units[k]) / SONG_UNITS_PER_CLOCK);
}
/* A song clock as the editor sees it: inside the open clip, its pass. */
SONG_SMALL static uint32_t view(const struct song_player *p, uint32_t clock)
{
    const struct song *s = p->song;
    if (!s->open) return clock;
    uint32_t length = s->clip_length[s->open - SONG_TRACKS], at = clock + p->shift - p->edit_base;
    return length && (int32_t)at > 0 ? at % length : 0;
}
/* The song's end and loop, whatever the editor shows (the open clip's loop
 * waits in loop_*, the song's in kept_*). A performance take runs on past
 * both. */
SONG_SMALL static uint32_t timeline_end(const struct song_player *p)
{
    return p->perf == 2 ? NEVER : song_bar_clock(p->song, p->song->bars);
}
SONG_SMALL static unsigned song_loop(const struct song_player *p, uint32_t *start, uint32_t *end)
{
    const struct song *s = p->song;
    *start = s->open ? s->kept_start : s->loop_start;
    *end = s->open ? s->kept_end : s->loop_end;
    return p->perf != 2 && (s->open ? s->kept_on : s->loop_on) && *start < *end;
}
#else
#define PLAY(p, t, e, at) do { if (song_event_kind(e) == SONG_NOTE) note_on(p, t, e, at); } while (0)
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
#if SONG_UI
#include "player_glitch.inc" /* chance, repeats, flams and the rest, per note */
SONG_SMALL static void play(struct song_player *p, unsigned t, const struct song_event *e, uint32_t at)
{
    if (song_is_lane(e)) lane_out(p, t, song_event_lane(e), song_event_value(e), 0);
    else if (song_is_note(e) && !QUIET(p, t, e)) glitch_note(p, t, e, at); /* the FX lane plays in emit */
}
#endif
SONG_SMALL static uint32_t onset(const struct song_player *p, unsigned t, const struct song_event *e)
{
#if SONG_UI
    uint32_t at = song_event_start(e) + song_swing(p->song, t, song_event_start(e));
    return song_is_note(e) ? glitch_onset(p, t, e, at) : at;
#else
    return song_event_start(e) + song_swing(p->song, t, song_event_start(e));
#endif
}
/* Everything that starts before clock `end` once swing is applied, in clock
 * order per track. A note-off goes before a note-on on the same clock. A
 * swung off-beat due after `end` waits as a group (all at one clock), so the
 * notes between it and its swung time are not held back. With glitch, the
 * repeats still due come in time order too, and a note can come early (a
 * flam's grace hit, humanize). */
#if SONG_UI
#include "player_clips.inc"   /* each track's source, launches and clips */
#include "player_stutter.inc" /* the emitter with glitch, the stutter and the FX lane */
#else
SONG_SMALL static void emit(struct song_player *p, uint32_t end)
{
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
                PLAY(p, t, &e[p->late_first[t]], late);
                ++p->late_first[t];
                --p->late_count[t];
            } else if (on < end) {
                PLAY(p, t, &e[p->cursor[t]], on);
                ++p->cursor[t];
            } else break;
        }
    }
    if (end > p->next) p->next = end;
}
#endif
/* Continue from `clock`, `over` units (at `rate`) after it. */
SONG_SMALL static void relocate(struct song_player *p, uint32_t clock, uint64_t over, uint32_t rate)
{
    const struct song *s = p->song;
    unsigned k = song_tempo_at(s, clock);
#if SONG_UI
    for (unsigned t = 0; t < SONG_TRACKS; ++t) /* swung past an edge: dropped. Clips play on */
        if (on_song(p, t)) { song_place(p, t, clock); p->hits[t] = 0; }
#else
    for (unsigned t = 0; t < SONG_TRACKS; ++t) p->cursor[t] = (uint16_t)song_find(s, t, clock);
    memset(p->late_count, 0, sizeof p->late_count); /* swung past an edge: dropped */
#endif
#if SONG_UI
    p->shift += p->next - clock; /* the transport runs on */
    ++p->pass; /* each pass rolls its own chances */
    if (p->stut_at != NEVER) { /* a loop or jump: a stutter goes on in step */
        uint32_t delta = clock - p->next;
        p->stut_at += delta;
        if (p->stut_to != NEVER) p->stut_to += delta;
    }
#endif
    p->next = clock;
    p->revision = s->revision;
    p->segment = (uint8_t)k;
    p->pos = song_units(s, k, clock) + (over ? song_rescale(over, song_rate(s->tempo[k].bpm100), rate) : 0);
#if SONG_UI
    click_off(p);
    p->click_next = next_beat(s, clock);
    for (unsigned t = 0; t < SONG_TRACKS; ++t) chase_track(p, t, clock);
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
    p->touch_track = 0xff;
    memset(p->program, 0xff, sizeof p->program);
    p->stut_at = NEVER;
    memset(p->from, SONG_FROM_SONG, sizeof p->from);
    memset(p->waiting, SONG_FROM_NONE, sizeof p->waiting);
    memset(p->blk, SONG_FROM_NONE, sizeof p->blk);
#endif
}
#if SONG_UI
/* From the song's place. With a clip open the playhead is in the clip, and
 * its pass starts so the clip is there as the song starts. */
SONG_SMALL void song_player_start(struct song_player *p, uint64_t frame)
{
    const struct song *s = p->song;
    if (p->playing) return;
    uint32_t from = timeline(p), length = s->open ? s->clip_length[s->open - SONG_TRACKS] : 0;
    if (from >= song_bar_clock(s, s->bars)) from = 0;
    if (p->playhead >= (s->open ? length : song_end(s))) p->playhead = s->open ? 0 : from;
    p->stut_at = NEVER;
    p->next = from; /* the transport starts at the song's place */
    p->shift = s->open ? p->playhead : 0;
    p->edit_base = from;
    p->jumping = 0;
    if (p->perf) p->perf = 2; /* PLAY starts an armed performance take */
    relocate(p, from, 0, 0);
    if (s->open) refind(p, song_editing(s), from + p->shift);
    p->frame = frame;
    p->playing = 1;
    p->pre_left = 0;
}
#else
SONG_SMALL void song_player_start(struct song_player *p, uint64_t frame)
{
    if (p->playing) return;
    if (p->playhead >= song_end(p->song)) p->playhead = 0;
    relocate(p, p->playhead, 0, 0);
    p->frame = frame;
    p->playing = 1;
    p->jumping = 0;
}
#endif
SONG_SMALL void song_player_stop(struct song_player *p)
{
    if (!p->playing) return;
#if SONG_UI
    p->song_at = timeline(p);
    p->playhead = view(p, p->song_at);
#else
    p->playhead = song_player_clock(p);
#endif
    all_off(p);
    p->playing = p->jumping = 0;
#if SONG_UI
    p->stut_at = NEVER;
    memset(p->hits, 0, sizeof p->hits);
    memset(p->from, SONG_FROM_SONG, sizeof p->from); /* every track back to the song */
    memset(p->waiting, SONG_FROM_NONE, sizeof p->waiting);
    memset(p->base, 0, sizeof p->base);
#endif
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
#if SONG_UI
SONG_SMALL uint32_t song_player_clock(const struct song_player *p) { return p->playing ? view(p, timeline(p)) : p->playhead; }
#else
SONG_SMALL uint32_t song_player_clock(const struct song_player *p)
{
    if (!p->playing) return p->playhead;
    const struct song *s = p->song;
    unsigned k = p->segment;
    return s->tempo[k].clock + (uint32_t)((p->pos - s->tempo_units[k]) / SONG_UNITS_PER_CLOCK);
}
#endif
SONG_SMALL int song_player_set_tempo(struct song_player *p, uint32_t clock, unsigned bpm100)
{
    struct song *s = p->song;
    if (!p->playing) return song_set_tempo(s, clock, bpm100);
    /* Keep the current clock and the time already spent inside it. */
#if SONG_UI
    uint32_t at = timeline(p), rate = song_rate(s->tempo[p->segment].bpm100);
#else
    uint32_t at = song_player_clock(p), rate = song_rate(s->tempo[p->segment].bpm100);
#endif
    uint64_t inside = p->pos - song_units(s, p->segment, at);
    if (song_set_tempo(s, clock, bpm100)) return -1;
    p->segment = (uint8_t)song_tempo_at(s, at);
    p->pos = song_units(s, p->segment, at) + song_rescale(inside, song_rate(s->tempo[p->segment].bpm100), rate);
    return 0;
}
SONG_SMALL uint32_t song_player_target(const struct song_player *p)
{
    if (!p->playing) return p->playhead;
#if SONG_UI
    return p->jumping ? p->jump : view(p, bar_after(p->song, p->next));
#else
    return p->jumping ? p->jump : bar_after(p->song, p->next);
#endif
}

SONG_SMALL void song_player_advance(struct song_player *p, uint64_t frame)
{
    /* Runs once more at frame == p->frame: an edge in the last frame leaves
     * events already due at the new position, and they belong to this block. */
    if (p->playing && p->revision != p->song->revision) {
        /* The notes changed under playback: find our place again. */
#if SONG_UI
        for (unsigned t = 0; t < SONG_TRACKS; ++t) refind(p, t, p->next + p->shift);
#else
        for (unsigned t = 0; t < SONG_TRACKS; ++t) p->cursor[t] = (uint16_t)song_find(p->song, t, p->next);
        memset(p->late_count, 0, sizeof p->late_count);
#endif
        p->revision = p->song->revision;
    }
#if SONG_UI
    pre_roll(p, frame);
#endif
    while (p->playing && p->frame <= frame) {
        const struct song *s = p->song;
        unsigned k = p->segment, kind = EDGE_END;
#if SONG_UI
        uint32_t rate = song_rate(s->tempo[k].bpm100), edge = NEVER, end = timeline_end(p), loop_start, loop_end;
        uint64_t target = p->pos + (frame - p->frame) * rate;
        if (k + 1U < s->tempos) { edge = s->tempo[k + 1].clock; kind = EDGE_TEMPO; }
        if (end <= edge) { edge = end; kind = EDGE_END; }
        if (song_loop(p, &loop_start, &loop_end) && p->next <= loop_end && loop_end <= edge) { edge = loop_end; kind = EDGE_LOOP; }
        if (p->jumping && !s->open && p->jump_at <= edge) { edge = p->jump_at; kind = EDGE_JUMP; } /* the open clip's own */
#else
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
#endif
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
#if SONG_UI
        if (kind == EDGE_END && !clips_playing(p)) { /* while clips play, the song goes round from the top */
            song_player_stop(p);
            p->playhead = 0;
            return;
        }
        for (unsigned t = 0; t < SONG_TRACKS; ++t) if (on_song(p, t)) song_player_cut(p, t);
        relocate(p, kind == EDGE_JUMP ? p->jump : kind == EDGE_END ? 0 : loop_start, now - at, rate);
#else
        all_off(p);
        if (kind == EDGE_END) {
            p->playing = p->jumping = 0;
            p->playhead = 0;
            return;
        }
        relocate(p, kind == EDGE_JUMP ? p->jump : s->loop_start, now - at, rate);
#endif
        if (kind == EDGE_JUMP) p->jumping = 0;
        else if (p->jumping) p->jump_at = bar_after(s, p->next);
    }
}
#if SONG_UI
#include "player_bake.inc" /* Normalize's capture */
#endif
