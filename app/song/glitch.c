/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Glitch attribute words and the song's glitch table. See glitch.h. The
 * player plays them (player_glitch.inc). */
#include "glitch.h"
#include "song_config.h"
#include <string.h>
#if SONG_UI

static const struct { uint8_t shift, max; } fields[GLITCH_FIELDS] = {
    {0, 5}, {3, 15}, {7, 1}, {8, 3}, {10, 7}, {13, 3}, {15, 1}, {16, 1}, {17, 1}, {18, 3}};

SONG_SMALL unsigned song_glitch_field(uint32_t word, unsigned field)
{
    unsigned bits = fields[field].max > 7 ? 15U : fields[field].max > 3 ? 7U : fields[field].max > 1 ? 3U : 1U;
    return word >> fields[field].shift & bits;
}
SONG_SMALL int song_glitch_drift(uint32_t word)
{
    int raw = (int)song_glitch_field(word, GLITCH_DRIFT);
    return raw > 7 ? raw - 16 : raw; /* 4-bit two's complement, so 0 is no drift */
}
SONG_SMALL uint32_t song_glitch_turn(uint32_t word, unsigned field, int delta)
{
    unsigned bits = fields[field].max > 7 ? 15U : fields[field].max > 3 ? 7U : fields[field].max > 1 ? 3U : 1U;
    int value = field == GLITCH_DRIFT ? song_glitch_drift(word) : (int)song_glitch_field(word, field);
    int low = field == GLITCH_DRIFT ? -8 : 0, high = field == GLITCH_DRIFT ? 7 : fields[field].max;
    value += delta;
    value = value < low ? low : value > high ? high : value;
    word &= ~(bits << fields[field].shift);
    return word | ((unsigned)value & bits) << fields[field].shift;
}
SONG_SMALL unsigned song_glitch_repeats(uint32_t word)
{
    static const uint8_t repeats[6] = {1, 2, 3, 4, 6, 8};
    return repeats[song_glitch_field(word, GLITCH_REPEATS)];
}
SONG_SMALL unsigned song_glitch_percent(uint32_t word, unsigned field)
{
    static const uint8_t fade[4] = {100, 70, 40, 15}, chance[8] = {100, 87, 75, 62, 50, 37, 25, 12},
                         odds[4] = {0, 25, 50, 100};
    unsigned v = song_glitch_field(word, field);
    return field == GLITCH_FADE ? fade[v] : field == GLITCH_CHANCE ? chance[v] : odds[v];
}
SONG_SMALL uint32_t song_glitch_word(const struct song *s, const struct song_event *e)
{
    return song_is_note(e) ? s->glitch[song_event_glitch(e)] : 0U;
}

/* Entries no note points at go back to the pool. */
SONG_SMALL static void reclaim(struct song *s)
{
    uint32_t used = 1;
    for (unsigned i = 0; i < s->events; ++i)
        if (song_is_note(&s->event[i])) used |= 1U << song_event_glitch(&s->event[i]);
    for (unsigned i = 1; i < GLITCH_ENTRIES; ++i)
        if (!(used >> i & 1U)) s->glitch[i] = 0;
}
SONG_SMALL int song_glitch_intern(struct song *s, uint32_t word)
{
    if (!word) return 0;
    for (unsigned pass = 0; pass < 2; ++pass) {
        for (unsigned i = 1; i < GLITCH_ENTRIES; ++i)
            if (s->glitch[i] == word) return (int)i;
        for (unsigned i = 1; i < GLITCH_ENTRIES; ++i)
            if (!s->glitch[i]) { s->glitch[i] = word; return (int)i; }
        reclaim(s);
    }
    return -1;
}
SONG_SMALL int song_glitch_apply(struct song *s, unsigned track, unsigned index, uint32_t word)
{
    unsigned count;
    const struct song_event *t = song_track(s, track, &count);
    int at = song_glitch_intern(s, word);
    if (index >= count || at < 0) return -1;
    struct song_event e = t[index];
    e.at = (e.at & ~SONG_GLITCH_BITS) | (uint32_t)at << SONG_GLITCH_SHIFT;
    return e.at == t[index].at ? 0 : song_set_event(s, track, index, &e);
}
#endif
