/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Shared helpers for the native song tests: checks, a capturing output and
 * exact rational timing, independent of the player's own arithmetic. */
#ifndef FM1_TEST_HARNESS_H
#define FM1_TEST_HARNESS_H
#include "player.h"
#include <stdio.h>
#include <string.h>

static unsigned failures, checks;
#define CHECK(cond) do { ++checks; if (!(cond)) { ++failures; \
    printf("%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_EQ(a, b) do { long long x_ = (long long)(a), y_ = (long long)(b); ++checks; \
    if (x_ != y_) { ++failures; printf("%s:%d: %s = %lld, expected %lld\n", \
        __FILE__, __LINE__, #a, x_, y_); } } while (0)

enum { CAPTURE = 16384, BLOCK = 128 };
struct heard { uint32_t block; uint8_t track, pitch, velocity; };
static struct heard heard[CAPTURE];
static unsigned heard_count;
static uint32_t block_now;
static void hear(void *context, unsigned track, unsigned pitch, unsigned velocity)
{
    (void)context;
    if (heard_count < CAPTURE)
        heard[heard_count++] = (struct heard){block_now, (uint8_t)track, (uint8_t)pitch, (uint8_t)velocity};
}
static const struct song_output listener = {.note = hear};
/* Advance block by block, as the simulator does: the player is asked for
 * everything due before the end of the block it is about to render. */
static void run_blocks(struct song_player *p, uint32_t first, uint32_t count)
{
    for (uint32_t b = first; b < first + count; ++b) {
        block_now = b;
        song_player_advance(p, (uint64_t)(b + 1) * BLOCK);
    }
}
static void reset_capture(void) { heard_count = 0; }

/* Every note-on must be closed by exactly one note-off on its track and
 * pitch, in order, and nothing may be left sounding. */
static int balanced(void)
{
    static uint8_t on[SONG_TRACKS][128];
    memset(on, 0, sizeof on);
    for (unsigned i = 0; i < heard_count; ++i) {
        uint8_t *n = &on[heard[i].track][heard[i].pitch];
        if (heard[i].velocity) { if (*n) return 0; *n = 1; }
        else { if (!*n) return 0; *n = 0; }
    }
    for (unsigned t = 0; t < SONG_TRACKS; ++t)
        for (unsigned k = 0; k < 128; ++k) if (on[t][k]) return 0;
    return 1;
}

/* Exact frame of a clock as num / den, straight from the tempo list. */
typedef __int128 wide;
struct exact { wide num, den; }; /* reduced, den > 0 */
static wide gcd_wide(wide a, wide b)
{
    if (a < 0) a = -a;
    while (b) { wide t = a % b; a = b; b = t; }
    return a ? a : 1;
}
static struct exact sum(struct exact a, struct exact b)
{
    wide g = gcd_wide(a.den, b.den), den = a.den / g * b.den;
    struct exact r = {a.num * (den / a.den) + b.num * (den / b.den), den};
    g = gcd_wide(r.num, r.den);
    return (struct exact){r.num / g, r.den / g};
}
static struct exact scaled(struct exact a, wide by) { return sum((struct exact){0, 1}, (struct exact){a.num * by, a.den}); }
static struct exact exact_frame(const struct song *s, uint32_t clock)
{
    struct exact f = {0, 1};
    for (unsigned k = 0; k < s->tempos; ++k) {
        uint32_t from = s->tempo[k].clock, to = k + 1 < s->tempos ? s->tempo[k + 1].clock : clock;
        if (to > clock) to = clock;
        if (to <= from) break;
        f = sum(f, (struct exact){(wide)(to - from) * (wide)SONG_UNITS_PER_CLOCK, song_rate(s->tempo[k].bpm100)});
    }
    return f;
}
/* Audio frame of a clock, for playback that put `origin` at start_frame. */
static struct exact exact_audio(const struct song *s, uint64_t start_frame, uint32_t origin, uint32_t clock)
{
    struct exact back = exact_frame(s, origin);
    back.num = -back.num;
    return sum(sum(exact_frame(s, clock), back), (struct exact){(wide)start_frame, 1});
}
static uint32_t block_of(struct exact f) { return (uint32_t)(f.num / (f.den * BLOCK)); }
static uint32_t exact_block(const struct song *s, uint64_t start_frame, uint32_t origin, uint32_t clock)
{
    return block_of(exact_audio(s, start_frame, origin, clock));
}
/* The player's song time must match `frames` (exact) to a thousandth of a frame. */
static int time_matches(const struct song_player *p, struct exact frames)
{
    wide rate = song_rate(p->song->tempo[p->segment].bpm100);
    wide error = (wide)p->pos * frames.den - frames.num * rate; /* units * den */
    if (error < 0) error = -error;
    return error * 1000 < frames.den * rate;
}
/* Note-ons only: how many, and the nth in emission order. */
static unsigned ons(void)
{
    unsigned n = 0;
    for (unsigned i = 0; i < heard_count; ++i) n += heard[i].velocity != 0;
    return n;
}
static const struct heard *nth_on(unsigned n)
{
    for (unsigned i = 0; i < heard_count; ++i)
        if (heard[i].velocity && !n--) return &heard[i];
    return NULL;
}

static int report(const char *name)
{
    printf("%s: %u checks, %u failed\n", name, checks, failures);
    return failures != 0;
}
#endif
