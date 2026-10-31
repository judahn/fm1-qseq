/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Undo and arranging: diffs that restore exactly, copy, cut, paste,
 * duplicate, inserting and deleting bars, and the full pool. */
#include "harness.h"
#include "arrange.h"
#include "undo.h"
#include "step.h"

static struct song song;
static struct song_undo undo;
static struct song_clip clip;

/* A fingerprint of everything an edit can change. The loop alone is the
 * selection, which undo leaves where it is. */
static uint64_t fingerprint(const struct song *s)
{
    struct song_maps m;
    song_maps_read(s, &m);
    m.loop_start = m.loop_end = 0;
    m.loop_on = 0;
    uint64_t h = 1469598103934665603ULL;
    const uint8_t *p = (const uint8_t *)s->event;
    for (unsigned i = 0; i < s->events * sizeof s->event[0]; ++i) h = (h ^ p[i]) * 1099511628211ULL;
    p = (const uint8_t *)s->count;
    for (unsigned i = 0; i < sizeof s->count; ++i) h = (h ^ p[i]) * 1099511628211ULL;
    p = (const uint8_t *)&m;
    for (unsigned i = 0; i < sizeof m; ++i) h = (h ^ p[i]) * 1099511628211ULL;
    return h;
}
static unsigned count(unsigned track) { unsigned n; song_track(&song, track, &n); return n; }
static const struct song_event *at(unsigned track, unsigned i) { unsigned n; return song_track(&song, track, &n) + i; }
static void fresh(void)
{
    song_clear(&song, 16, 12000);
    song_undo_init(&undo);
    song.undo = &undo;
}

static void test_undo_basics(void)
{
    fresh();
    for (unsigned i = 0; i < 8; ++i) song_add_note(&song, i % 3, i * 240U, 60 + i, 100, 120);
    song_undo_init(&undo); /* the setup is not an edit */
    uint64_t before = fingerprint(&song);
    song_undo_begin(&undo, &song);
    song_add_note(&song, 1, 100, 70, 90, 30);
    song_remove(&song, 0, 1);
    struct song_event e = *at(2, 0);
    e.b = 33; e.at = (e.at & 0xff000000U) | 999U;
    song_set_event(&song, 2, 0, &e);
    song_add_marker(&song, 480, "HERE");
    song.swing = 20;
    CHECK_EQ(song_undo_end(&undo, &song), 0);
    uint64_t after = fingerprint(&song);
    CHECK(after != before);
    CHECK_EQ(song_undo_count(&undo, 0), 1);
    CHECK_EQ(song_undo(&undo, &song), 0);
    CHECK(fingerprint(&song) == before);
    CHECK_EQ(song_undo_count(&undo, 1), 1);
    CHECK_EQ(song_redo(&undo, &song), 0);
    CHECK(fingerprint(&song) == after);
    CHECK_EQ(song_redo(&undo, &song), -1);
    /* An empty transaction leaves no step and keeps redo. A real edit drops it. */
    song_undo_begin(&undo, &song);
    CHECK_EQ(song_undo_end(&undo, &song), 0);
    CHECK_EQ(song_undo_count(&undo, 0), 1);
    song_undo(&undo, &song);
    song_undo_begin(&undo, &song);
    song_undo_end(&undo, &song);
    CHECK_EQ(song_undo_count(&undo, 1), 1);
    song_undo_begin(&undo, &song); song_add_note(&song, 5, 0, 40, 100, 10); song_undo_end(&undo, &song);
    CHECK_EQ(song_undo_count(&undo, 1), 0);
    CHECK_EQ(song_redo(&undo, &song), -1);
    /* Nested begins close as one step. */
    song_undo_begin(&undo, &song); song_undo_begin(&undo, &song);
    song_add_note(&song, 6, 0, 41, 100, 10);
    song_undo_end(&undo, &song);
    song_add_note(&song, 6, 10, 42, 100, 10);
    song_undo_end(&undo, &song);
    song_undo(&undo, &song);
    CHECK_EQ(count(6), 0);
}

static void step(void (*edit)(void)) { song_undo_begin(&undo, &song); edit(); song_undo_end(&undo, &song); }
static void loop_later(void) { song.loop_start += 1920U; song.loop_end += 1920U; }
static void swing_up(void) { song.swing += 5; }
static void key_up(void) { ++song.key_root; }
static void insert_bar(void) { song_insert_time(&song, 0, 1920U); }

/* Moving the loop alone is no step. A knob turned over many polls is one
 * step. A step puts back only what it changed. */
static void test_undo_steps(void)
{
    fresh();
    song.loop_start = 1920U; song.loop_end = 3840U;
    song_undo_init(&undo);
    step(loop_later);
    CHECK_EQ(song_undo_count(&undo, 0), 0);
    step(swing_up); step(swing_up); step(swing_up);
    CHECK_EQ(song_undo_count(&undo, 0), 1);
    step(key_up); step(swing_up);
    CHECK_EQ(song_undo_count(&undo, 0), 3);
    step(loop_later);
    CHECK_EQ(song_undo(&undo, &song), 0);  /* the last swing turn */
    CHECK_EQ(song.swing, 15);
    CHECK_EQ(song.loop_start, 5760U);       /* the loop stays */
    CHECK_EQ(song_undo(&undo, &song), 0);  /* the key */
    CHECK_EQ(song_undo(&undo, &song), 0);  /* all three swing turns */
    CHECK_EQ(song.swing, 0);
    CHECK_EQ(song.key_root, 0);
    /* An insert moves the loop with the music, and its undo moves it back. */
    step(insert_bar);
    CHECK_EQ(song.loop_start, 7680U);
    song_undo(&undo, &song);
    CHECK_EQ(song.loop_start, 5760U);
    /* With redo waiting, a settings change starts a new step. */
    step(swing_up);
    song_undo(&undo, &song);
    step(swing_up);
    CHECK_EQ(song_undo_count(&undo, 1), 0);
    CHECK_EQ(song_undo_count(&undo, 0), 1);
}

static void test_arrange(void)
{
    fresh();
    song_set_tempo(&song, 8U * 1920U, 9000);
    song_add_marker(&song, 4U * 1920U, "B");
    song_add_marker(&song, 8U * 1920U, "C");
    song_set_meter(&song, 10, 3, 4);
    for (unsigned bar = 0; bar < 8; ++bar) {
        song_add_note(&song, 0, bar * 1920U, 60 + bar, 100, 480);
        song_add_note(&song, 1, bar * 1920U + 960U, 40 + bar, 100, 1200); /* crosses the bar */
    }
    /* Copy bars 3 and 4 of track 2, paste onto track 6 at beat 3 of bar 13. */
    CHECK_EQ(song_copy(&song, &clip, 1U << 1, 2U * 1920U, 4U * 1920U), 0);
    CHECK_EQ(clip.count, 2);
    CHECK_EQ(clip.event[1].length, 960); /* cut off at the selection end */
    uint32_t to = 12U * 1920U + 960U;
    CHECK_EQ(song_paste(&song, &clip, to, 5, 0), 0);
    CHECK_EQ(count(5), 2);
    CHECK_EQ(song_event_start(at(5, 0)), to + 960U);
    CHECK_EQ(at(5, 1)->a, 43);
    /* Several tracks paste back onto their own, replacing what's there. */
    song_copy(&song, &clip, 3U, 0, 1920U);
    CHECK_EQ(song_paste(&song, &clip, 1920U, 7, 0), 0);
    CHECK_EQ(count(0), 8);
    CHECK_EQ(at(0, 1)->a, 60);
    CHECK_EQ(at(1, 1)->a, 40);
    /* Inserting two bars at bar 3 moves notes, markers, tempo and meters. */
    unsigned bars = song.bars;
    CHECK_EQ(song_insert_time(&song, 2U * 1920U, 2U * 1920U), 0);
    CHECK_EQ(song.bars, bars + 2);
    CHECK_EQ(song.marker[0].clock, 6U * 1920U);
    CHECK_EQ(song.tempo[1].clock, 10U * 1920U);
    CHECK_EQ(song.meter[1].bar, 12);
    CHECK_EQ(song_find(&song, 0, 2U * 1920U), 2); /* bars 3 and 4 are empty */
    CHECK_EQ(song_event_start(at(0, 2)), 4U * 1920U);
    /* Deleting them again puts everything back. */
    CHECK_EQ(song_delete_time(&song, 2U * 1920U, 4U * 1920U), 0);
    CHECK_EQ(song.bars, bars);
    CHECK_EQ(song.marker[0].clock, 4U * 1920U);
    CHECK_EQ(song.tempo[1].clock, 8U * 1920U);
    CHECK_EQ(song.meter[1].bar, 10);
    /* Deleting bars 8 and 9: the tempo change inside moves to the cut, the
     * marker inside goes, and a note running into the gap ends at it. */
    CHECK_EQ(song_delete_time(&song, 7U * 1920U + 0U, 9U * 1920U), 0);
    CHECK_EQ(song.tempos, 2);
    CHECK_EQ(song.tempo[1].clock, 7U * 1920U);
    CHECK_EQ(song.markers, 1);
    CHECK_EQ(at(1, 6)->length, 960); /* bar 7's note stops at bar 8 */
    /* Duplicate goes over what follows, which stays where it is (0.5.5): 2
     * bars become 4 with the note after them still at bar 5, then 6 over
     * it, and the selection follows. Another track is left alone. */
    fresh();
    for (unsigned i = 0; i < 4; ++i) song_add_note(&song, 3, i * 960U, 50 + i, 100, 240);
    song_add_note(&song, 3, 4U * 1920U, 70, 100, 240); /* after the selection */
    song_add_note(&song, 0, 5U * 1920U, 80, 100, 240);
    uint32_t from = 0, end = 2U * 1920U;
    CHECK_EQ(song_duplicate(&song, 1U << 3, &from, &end), 0);
    CHECK_EQ(from, 2U * 1920U);
    CHECK_EQ(end, 4U * 1920U);
    CHECK_EQ(count(3), 9);
    CHECK(at(3, 8)->a == 70 && song_event_start(at(3, 8)) == 4U * 1920U);
    CHECK_EQ(song_duplicate(&song, 1U << 3, &from, &end), 0);
    CHECK_EQ(count(3), 12);
    CHECK(at(3, 8)->a == 50 && song_event_start(at(3, 8)) == 4U * 1920U); /* bar 5's note went under the copy */
    CHECK(count(0) == 1 && song_event_start(at(0, 0)) == 5U * 1920U);
    CHECK_EQ(song.bars, 16);
    /* Cut leaves the bars in place, empty. */
    CHECK_EQ(song_cut(&song, &clip, 1U << 3, 0, 1920U), 0);
    CHECK_EQ(count(3), 10);
    CHECK_EQ(song_event_start(at(3, 0)), 1920U);
    /* Erase (delete, 0.5.5) empties the selected tracks there, copying
     * nothing and moving nothing, and says when there was nothing. */
    CHECK_EQ(song_erase(&song, 1U << 3 | 1U, 1920U, 2U * 1920U), 0);
    CHECK(count(3) == 8 && song_event_start(at(3, 0)) == 2U * 1920U);
    CHECK(count(0) == 1 && song_event_start(at(0, 0)) == 5U * 1920U);
    CHECK_EQ(clip.length, 1920);
    CHECK_EQ(song_erase(&song, 1U << 3, 1920U, 2U * 1920U), -1);
    CHECK_EQ(song_erase(&song, 1U << 3, 1920U, 1920U), -1);
    /* Near the end the song grows to hold a duplicate or a paste. */
    from = 14U * 1920U, end = 16U * 1920U;
    song_add_note(&song, 3, from, 60, 100, 240);
    CHECK_EQ(song_duplicate(&song, 1U << 3, &from, &end), 0);
    CHECK_EQ(song.bars, 18);
    CHECK_EQ(song_paste(&song, &clip, 18U * 1920U + 960U, 3, 0), 0);
    CHECK_EQ(song.bars, 20);
    /* A paste that doesn't fit changes nothing. */
    while (song.events < SONG_EVENTS) song_add_note(&song, 0, song.events * 30U, 60, 100, 30);
    uint64_t full = fingerprint(&song);
    CHECK_EQ(song_paste(&song, &clip, 16U * 1920U, 3, 1), -1);
    CHECK(fingerprint(&song) == full);
}

/* Hundreds of random edits, each its own step. Undo walks back through every
 * state exactly, and redo walks forward again. */
static void test_undo_walk(void)
{
    static uint64_t states[200];
    static struct song_step st;
    fresh();
    for (unsigned bar = 0; bar < 16; ++bar)
        for (unsigned t = 0; t < 4; ++t) song_add_note(&song, t, bar * 1920U + t * 240U, 48 + bar + t, 100, 400);
    song_undo_init(&undo);
    step_init(&st, &song, &listener);
    uint32_t seed = 99;
    unsigned steps = 0, failed_steps = 0;
    states[0] = fingerprint(&song);
    for (unsigned n = 1; n < 200; ++n) {
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        uint32_t a = (seed >> 4) % 12U * 960U, b = a + 960U * (1U + seed % 4U);
        song_undo_begin(&undo, &song);
        switch (seed % 8) {
        case 0: song_cut(&song, &clip, seed >> 20 & 0xffU, a, b); break;
        case 1: song_copy(&song, &clip, seed >> 20 & 0xffU, a, b); song_paste(&song, &clip, b, seed % 8, seed >> 9 & 1U); break;
        case 2: song_duplicate(&song, seed >> 20 & 0xffU, &a, &b); break;
        case 3: song_insert_time(&song, a, 1920U); break;
        case 4: song_delete_time(&song, a, b); break;
        case 5: step_begin(&st, seed % 8, a); step_key(&st, 60 + seed % 12); step_commit(&st); step_tie(&st); break;
        case 6: step_begin(&st, seed % 4, a); step_land(&st); step_set_velocity(&st, -7); step_transpose(&st, 1, 0); break;
        default: song.loop_start = a; song.loop_end = b; song_add_marker(&song, b, "M"); break;
        }
        if (song_undo_end(&undo, &song)) ++failed_steps;
        states[n] = fingerprint(&song);
        steps = n;
    }
    CHECK_EQ(failed_steps, 0);
    unsigned levels = song_undo_count(&undo, 0), wrong = 0, undone = 0;
    CHECK(levels > 20);
    for (unsigned now = steps; song_undo(&undo, &song) == 0;) {
        ++undone;
        /* Edits that changed nothing left no step, so look further back. */
        unsigned k = now;
        while (k > 0 && states[k - 1] != fingerprint(&song)) --k;
        if (!k) { ++wrong; break; }
        now = k - 1;
    }
    CHECK_EQ(undone, levels);
    CHECK_EQ(wrong, 0);
    unsigned redone = 0;
    while (song_redo(&undo, &song) == 0) ++redone;
    CHECK_EQ(redone, levels);
    CHECK(fingerprint(&song) == states[steps]);
}

int main(void)
{
    test_undo_basics();
    test_undo_steps();
    test_arrange();
    test_undo_walk();
    return report("test_edit");
}
