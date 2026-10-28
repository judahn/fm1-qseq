/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Knob recording in touch style. See touch.h. */
#include "touch.h"
#include "song_config.h"
#if SONG_UI

static int fresh(const struct song_event *e) { return (e->at & SONG_FRESH) != 0; }
static int on_lane(const struct song_event *e, unsigned lane) { return song_is_lane(e) && song_event_lane(e) == lane; }

/* The lane's value just before `clock` as this touch has it: its own
 * changes, or what was there before it began. */
SONG_SMALL static int before(const struct song *s, unsigned track, const struct song_touch *t, uint32_t clock)
{
    unsigned count, i = song_find(s, track, clock);
    const struct song_event *e = song_track(s, track, &count);
    while (i-- > 0)
        if (on_lane(&e[i], t->lane) && (fresh(&e[i]) || song_event_start(&e[i]) < t->from))
            return (int)song_event_value(&e[i]);
    return -1;
}
SONG_SMALL void song_touch_move(struct song *s, unsigned track, struct song_touch *t, unsigned lane, unsigned value,
                                uint32_t clock)
{
    uint32_t grid = clock - clock % SONG_TOUCH_GRID;
    if (!t->active || t->lane != lane || grid < t->from) { t->active = 1; t->lane = (uint8_t)lane; t->from = grid; }
    t->last = grid;
    t->value = (uint8_t)value;
    unsigned count;
    const struct song_event *e = song_track(s, track, &count);
    int at = song_lane_find(s, track, lane, grid);
    if (before(s, track, t, grid) != (int)value) (void)song_lane_set(s, track, lane, grid, value, 1);
    else if (at >= 0 && fresh(&e[at])) (void)song_remove(s, track, (unsigned)at); /* back where it was */
}
SONG_SMALL int song_touch_release(struct song *s, unsigned track, struct song_touch *t, uint32_t clock)
{
    if (!t->active) return -1;
    t->active = 0;
    uint32_t end = clock - clock % SONG_TOUCH_GRID;
    if (end < t->last) end = t->last;
    unsigned count, stop = song_find(s, track, end + 1U);
    const struct song_event *e = song_track(s, track, &count);
    /* The older curve at the end, before anything goes. */
    int old = -1;
    for (unsigned i = stop; i-- > 0 && old < 0;)
        if (on_lane(&e[i], t->lane) && !fresh(&e[i])) old = (int)song_event_value(&e[i]);
    for (unsigned i = stop; i < count && old < 0; ++i)
        if (on_lane(&e[i], t->lane) && !fresh(&e[i])) old = (int)song_event_value(&e[i]);
    for (unsigned i = stop, first = song_find(s, track, t->from); i-- > first;)
        if (on_lane(&e[i], t->lane) && !fresh(&e[i])) (void)song_remove(s, track, i);
    if (old < 0 || old == t->value) return old;
    uint32_t back = end > t->last ? end : end + SONG_TOUCH_GRID;
    if (song_lane_find(s, track, t->lane, back) < 0) (void)song_lane_set(s, track, t->lane, back, (unsigned)old, 1);
    return old;
}
#endif
