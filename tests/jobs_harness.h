/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Shared setup for the job tests (phase 6): a song with the journal on,
 * notes by hand, a job run as one undo step, and undo and redo checked
 * against copies of the song taken before and after. */
#ifndef FM1_TEST_JOBS_HARNESS_H
#define FM1_TEST_JOBS_HARNESS_H
#include "harness.h"
#include "jobs.h"
#include "undo.h"
#include "glitch.h"
#include "lanes.h"

static struct song song, before, after;
static struct song_undo undo;
static struct song_player player;

static void fresh(unsigned bars)
{
    song_clear(&song, bars, 12000);
    song_undo_init(&undo);
    song.undo = &undo;
}
static void note(unsigned track, uint32_t at, unsigned pitch, unsigned velocity, unsigned length)
{
    song_undo_begin(&undo, &song);
    CHECK_EQ(song_add_note(&song, track, at, pitch, velocity, length), 0);
    song_undo_end(&undo, &song);
}
static void lane(unsigned track, uint32_t at, unsigned cc, unsigned value)
{
    const struct song_event e = song_lane_event(at, cc, value);
    song_undo_begin(&undo, &song);
    CHECK_EQ(song_add_event(&song, track, &e), 0);
    song_undo_end(&undo, &song);
}
static struct song_range range(uint32_t from, uint32_t to, unsigned tracks)
{
    return (struct song_range){from, to, (uint8_t)tracks, 0};
}
/* The nth event of a track, and the nth note. */
static const struct song_event *at_index(unsigned track, unsigned i) { return &song.event[song.first[track] + i]; }
static const struct song_event *nth_note(unsigned track, unsigned n)
{
    for (unsigned i = 0; i < song.count[track]; ++i)
        if (song_is_note(at_index(track, i)) && !n--) return at_index(track, i);
    return NULL;
}
/* A note of this pitch starts here: undo may reorder notes that share a
 * start, so checks look notes up rather than count on their order. */
static int has_note(unsigned track, uint32_t clock, unsigned pitch)
{
    for (unsigned i = 0; i < song.count[track]; ++i) {
        const struct song_event *e = at_index(track, i);
        if (song_is_note(e) && song_event_start(e) == clock && e->a == pitch) return 1;
    }
    return 0;
}
/* Songs compared as sets of events per track, plus the maps. */
static uint64_t fingerprint(const struct song *s)
{
    uint64_t sum = 0;
    for (unsigned t = 0; t < SONG_TRACKS; ++t) {
        unsigned n;
        const struct song_event *e = song_track(s, t, &n);
        for (unsigned i = 0; i < n; ++i) {
            uint64_t h = ((e[i].at & ~SONG_FRESH) ^ (uint64_t)e[i].a << 32 ^ (uint64_t)e[i].b << 40 ^ (uint64_t)e[i].length << 48) *
                         0x9e3779b97f4a7c15ULL;
            sum += (h ^ h >> 29 ^ t) * 1099511628211ULL;
        }
    }
    return sum;
}
static int same_song(const struct song *a, const struct song *b)
{
    struct song_maps x, y;
    song_maps_read(a, &x);
    song_maps_read(b, &y);
    return a->events == b->events && !memcmp(a->count, b->count, sizeof a->count) && fingerprint(a) == fingerprint(b) &&
           !memcmp(&x, &y, sizeof x);
}
/* Begin, run, end: then undo must give back `before` and redo `after`. */
#define JOB(call) (before = song, song_undo_begin(&undo, &song), song_undo_step(&undo), job_result = (call), \
                   song_undo_end(&undo, &song), after = song, job_result)
static int job_result;
static unsigned undo_line;
#define undo_redo() (undo_line = __LINE__, undo_redo_at())
static void undo_redo_at(void)
{
    CHECK_EQ(song_undo(&undo, &song), 0);
    if (!same_song(&song, &before)) printf("  undo differs, job at line %u\n", undo_line);
    CHECK(same_song(&song, &before));
    CHECK_EQ(song_redo(&undo, &song), 0);
    if (!same_song(&song, &after)) printf("  redo differs, job at line %u\n", undo_line);
    CHECK(same_song(&song, &after));
}
#endif
