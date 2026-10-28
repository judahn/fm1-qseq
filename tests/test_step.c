/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Step entry: lengths, entering, chords on release, rests, ties, repeats,
 * deleting, editing the selection, the pitch knob and the full pool. */
#include "harness.h"
#include "step.h"

static struct song song;
static struct song_step st;

static const struct song_event *at(unsigned track, unsigned i)
{
    unsigned n;
    const struct song_event *e = song_track(&song, track, &n);
    return i < n ? &e[i] : NULL;
}
static unsigned count(unsigned track) { unsigned n; song_track(&song, track, &n); return n; }
static void fresh(unsigned length)
{
    song_clear(&song, 4, 12000);
    step_init(&st, &song, &listener);
    step_begin(&st, 0, 0);
    step_pick_length(&st, length);
    reset_capture();
}
static void play(unsigned pitch) { step_key(&st, pitch); step_commit(&st); }

static void test_lengths(void)
{
    static const uint16_t plain[7] = {30, 60, 120, 240, 480, 960, 1920};
    fresh(STEP_QUARTER);
    for (unsigned l = 0; l < STEP_LENGTHS; ++l)
        for (unsigned t = 0; t < 2; ++t)
            for (unsigned d = 0; d < 3; ++d) {
                st.length = (uint8_t)l; st.triplet = (uint8_t)t; st.dots = (uint8_t)d;
                if (l == STEP_64TH && !t && d == 2) continue;
                uint32_t want = plain[l] * (t ? 2U : 3U) * (d == 0 ? 4U : d == 1 ? 6U : 7U);
                CHECK(want % 12U == 0);
                CHECK_EQ(step_clocks(&st), want / 12U);
            }
    st.length = STEP_QUARTER; st.triplet = 0; st.dots = 1;
    CHECK_EQ(step_clocks(&st), 720);
    st.length = STEP_64TH; st.triplet = 1; st.dots = 0;
    CHECK_EQ(step_clocks(&st), 20);
    /* A plain 64th skips the double dot, which would be 52.5 clocks. */
    st.triplet = 0;
    step_cycle_dots(&st); CHECK_EQ(st.dots, 1);
    step_cycle_dots(&st); CHECK_EQ(st.dots, 0);
    st.length = STEP_8TH; st.dots = 2;
    step_pick_length(&st, STEP_64TH); CHECK_EQ(st.dots, 1);
    /* Gates: 50, 90, 99 and 100%. A normal quarter is 432. */
    st.length = STEP_QUARTER; st.dots = 0;
    static const uint16_t gates[4] = {240, 432, 475, 480};
    for (unsigned g = 0; g < 4; ++g) { st.gate = (uint8_t)g; CHECK_EQ(step_gate_clocks(&st, 480), gates[g]); }
    CHECK(!strcmp(step_velocity_name(100), "FF"));
    CHECK(!strcmp(step_velocity_name(64), "NORM"));
    CHECK(!strcmp(step_velocity_name(1), "PPP"));
    char name[8];
    st.length = STEP_8TH; st.triplet = 1; st.dots = 1;
    step_length_name(&st, name);
    CHECK(!strcmp(name, "1/8T."));
}

static void test_entering(void)
{
    fresh(STEP_QUARTER);
    play(60);
    CHECK_EQ(count(0), 1);
    CHECK_EQ(song_event_start(at(0, 0)), 0);
    CHECK_EQ(at(0, 0)->length, 432);
    CHECK_EQ(at(0, 0)->b, 100);
    CHECK_EQ(st.pointer, 480);
    /* Keys held together become one chord when released. */
    step_key(&st, 64); step_key(&st, 67); step_key(&st, 60); step_key(&st, 64);
    CHECK_EQ(count(0), 1); /* nothing until release */
    CHECK_EQ(step_commit(&st), 0);
    CHECK_EQ(count(0), 4);
    CHECK_EQ(song_event_start(at(0, 1)), 480);
    CHECK_EQ(song_event_start(at(0, 3)), 480);
    /* Released one at a time, they follow each other. */
    play(62); play(65);
    CHECK_EQ(song_event_start(at(0, 4)), 960);
    CHECK_EQ(song_event_start(at(0, 5)), 1440);
    CHECK_EQ(st.pointer, 1920);
    CHECK_EQ(step_commit(&st), -1); /* nothing held */
    /* Rest, then repeat the last chord. */
    step_rest(&st);
    CHECK_EQ(st.pointer, 2400);
    step_key(&st, 48); step_key(&st, 55); step_commit(&st);
    CHECK_EQ(step_repeat(&st), 0);
    CHECK_EQ(count(0), 10);
    CHECK_EQ(song_event_start(at(0, 8)), 2880);
    CHECK_EQ(at(0, 9)->a, 55);
    /* Entering past the end makes the song longer. */
    st.pointer = song_end(&song) - 480;
    play(72);
    CHECK_EQ(song.bars, 5);
    CHECK_EQ(st.pointer, 4U * 1920U);
}

static void test_tie(void)
{
    fresh(STEP_QUARTER);
    play(60);
    /* A quarter tied to an 8th: sounds through and ends with the 8th's gate. */
    step_pick_length(&st, STEP_8TH);
    CHECK_EQ(step_tie(&st), 0);
    CHECK_EQ(at(0, 0)->length, 480 + 216);
    CHECK_EQ(st.pointer, 720);
    step_pick_length(&st, STEP_16TH);
    CHECK_EQ(step_tie(&st), 0);
    CHECK_EQ(at(0, 0)->length, 720 + 108);
    CHECK_EQ(st.pointer, 840);
    /* Ties a whole chord, and only straight after entering. */
    step_key(&st, 64); step_key(&st, 67); step_commit(&st);
    step_tie(&st);
    CHECK_EQ(at(0, 1)->length, 240 - 12);
    CHECK_EQ(at(0, 2)->length, 240 - 12);
    step_rest(&st);
    CHECK_EQ(step_tie(&st), -1);
    play(70);
    step_move(&st, 0);
    CHECK_EQ(step_tie(&st), -1);
}

static void test_deleting(void)
{
    fresh(STEP_16TH);
    for (unsigned i = 0; i < 4; ++i) play(60 + i);
    CHECK_EQ(step_back_delete(&st), 0);
    CHECK_EQ(count(0), 3);
    CHECK_EQ(st.pointer, 360);
    /* Back-deleting a quarter takes everything that starts in it. */
    step_pick_length(&st, STEP_QUARTER);
    st.pointer = 480;
    step_back_delete(&st);
    CHECK_EQ(count(0), 0);
    CHECK_EQ(st.pointer, 0);
    /* Delete takes the selection: the whole chord, or the picked notes. */
    step_key(&st, 60); step_key(&st, 64); step_key(&st, 67); step_commit(&st);
    step_move(&st, -1);
    step_pick(&st, 64);
    step_delete(&st);
    CHECK_EQ(count(0), 2);
    CHECK(at(0, 0)->a != 64 && at(0, 1)->a != 64);
    step_delete(&st);
    CHECK_EQ(count(0), 0);
}

static void test_selection(void)
{
    fresh(STEP_8TH);
    st.gate = 0; /* staccato */
    step_key(&st, 60); step_key(&st, 64); step_key(&st, 67); step_commit(&st);
    st.gate = 1; st.length = STEP_QUARTER; st.velocity = 80;
    play(72);
    /* Landing on the chord previews it and loads its length and gate. */
    reset_capture();
    st.pointer = 0;
    step_land(&st);
    CHECK_EQ(heard_count, 3);
    CHECK_EQ(st.length, STEP_8TH);
    CHECK_EQ(st.gate, 0);
    CHECK_EQ(st.velocity, 100);
    /* Knobs edit the selected notes in place. */
    song.event[song.first[0] + 1].b = 80; /* a chord with its own balance */
    step_set_velocity(&st, -10);
    CHECK_EQ(at(0, 0)->b, 90); CHECK_EQ(at(0, 1)->b, 70); CHECK_EQ(at(0, 2)->b, 90);
    song.event[song.first[0] + 1].b = 90;
    step_set_gate(&st, 1);
    for (unsigned i = 0; i < 3; ++i) CHECK_EQ(at(0, i)->length, 216);
    step_set_length(&st, 1);
    for (unsigned i = 0; i < 3; ++i) CHECK_EQ(at(0, i)->length, 432);
    step_toggle_triplet(&st);
    CHECK_EQ(at(0, 0)->length, 288);
    step_toggle_triplet(&st);
    /* Picking narrows the selection to the notes chosen. */
    step_pick(&st, 67);
    step_set_velocity(&st, 20);
    CHECK_EQ(at(0, 0)->b, 90);
    CHECK_EQ(at(0, 2)->b, 110);
    CHECK_EQ(at(0, 3)->b, 80); /* the next note is untouched */
    /* On an empty position the knobs only set the next entry. */
    st.pointer = 1920;
    step_land(&st);
    CHECK_EQ(st.velocity, 110); /* still the last note's */
    step_set_velocity(&st, 17);
    CHECK_EQ(st.velocity, 127);
    step_set_velocity(&st, 2);
    CHECK_EQ(st.random_level, 2);
    for (unsigned i = 0; i < 8; ++i) play(40 + i);
    for (unsigned i = 0; i < 8; ++i) {
        const struct song_event *e = at(0, 4 + i);
        CHECK(e->b >= 80 && e->b <= 112);
    }
    step_set_velocity(&st, -3);
    CHECK_EQ(st.random_level, 0);
    CHECK_EQ(st.velocity, 126);
    /* Jumping note to note. */
    st.pointer = 100;
    step_jump(&st, 1);
    CHECK_EQ(st.pointer, 240);
    step_jump(&st, -1);
    CHECK_EQ(st.pointer, 0);
}

static void test_pitch_knob(void)
{
    fresh(STEP_QUARTER);
    song.key_root = 9; song.key_scale = 2; /* A minor */
    step_key(&st, 60); step_key(&st, 64); step_key(&st, 67); step_commit(&st);
    st.pointer = 0;
    CHECK_EQ(step_transpose(&st, 1, 0), 3);
    CHECK_EQ(at(0, 0)->a, 62); CHECK_EQ(at(0, 1)->a, 65); CHECK_EQ(at(0, 2)->a, 69);
    step_transpose(&st, -1, 0);
    CHECK_EQ(at(0, 0)->a, 60); CHECK_EQ(at(0, 1)->a, 64); CHECK_EQ(at(0, 2)->a, 67);
    step_transpose(&st, 1, 1);
    CHECK_EQ(at(0, 0)->a, 72); CHECK_EQ(at(0, 2)->a, 79);
    step_transpose(&st, -1, 1);
    /* Chromatic moves by semitone. A picked note can't land on another. */
    song.key_scale = 0;
    step_pick(&st, 60);
    step_transpose(&st, 1, 0);
    CHECK_EQ(at(0, 0)->a, 61);
    step_transpose(&st, 3, 0);
    CHECK_EQ(at(0, 0)->a, 61); /* 64 is taken */
    /* Drum tracks move along their pads. */
    static const uint8_t pads[4] = {36, 38, 42, 46};
    step_begin(&st, 7, 0);
    play(36);
    st.pointer = 0;
    CHECK_EQ(step_transpose_pads(&st, pads, 4, 2), 1);
    CHECK_EQ(at(7, 0)->a, 42);
    CHECK_EQ(step_transpose_pads(&st, pads, 4, 2), 0);
}

/* The pool stays sorted per track whatever happens, full or not. */
static int pool_sound(void)
{
    unsigned total = 0;
    for (unsigned t = 0; t < SONG_TRACKS; ++t) {
        if (song.first[t] != total) return 0;
        const struct song_event *e = song.event + song.first[t];
        for (unsigned i = 1; i < song.count[t]; ++i) {
            if (song_event_start(&e[i]) < song_event_start(&e[i - 1])) return 0;
            for (unsigned k = i; k-- > 0 && song_event_start(&e[k]) == song_event_start(&e[i]);)
                if (e[k].a == e[i].a) return 0; /* one note per pitch and clock */
        }
        total += song.count[t];
    }
    return total == song.events && total <= SONG_EVENTS;
}
static void test_full_pool(void)
{
    fresh(STEP_16TH);
    for (unsigned i = 0; song.events < SONG_EVENTS - 2; ++i) song_add_note(&song, 1 + i % 7, i * 30U, 50, 90, 30);
    step_key(&st, 60); step_key(&st, 64); step_key(&st, 67);
    CHECK_EQ(step_commit(&st), -1);
    CHECK_EQ(count(0), 0);
    CHECK_EQ(st.pointer, 0);
    play(60); play(62);
    CHECK_EQ(song.events, SONG_EVENTS);
    CHECK_EQ(step_repeat(&st), -1);
    CHECK_EQ(step_tie(&st), 0);   /* ties change lengths, not the count */
    st.pointer = 0;
    step_delete(&st);
    CHECK_EQ(song.events, SONG_EVENTS - 1);
    CHECK(pool_sound());
    /* A long run of random edits on a nearly full pool. */
    uint32_t seed = 12345, bad = 0;
    for (unsigned n = 0; n < 40000; ++n) {
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        unsigned op = seed % 13, arg = seed >> 8;
        if (n % 500 == 0) step_begin(&st, arg % 8, (arg % 64) * 30U);
        switch (op) {
        case 0: case 1: step_key(&st, 40 + arg % 40); if (arg & 1) step_key(&st, 41 + arg % 30); step_commit(&st); break;
        case 2: step_rest(&st); break;
        case 3: step_tie(&st); break;
        case 4: step_repeat(&st); break;
        case 5: step_delete(&st); break;
        case 6: step_back_delete(&st); break;
        case 7: step_move(&st, (int)(arg % 9) - 4); break;
        case 8: step_jump(&st, arg & 1 ? 1 : -1); break;
        case 9: step_pick_length(&st, arg % STEP_LENGTHS); if (arg & 2) step_toggle_triplet(&st); break;
        case 10: step_set_velocity(&st, (int)(arg % 21) - 10); break;
        case 11: step_pick(&st, 40 + arg % 40); step_transpose(&st, (int)(arg % 5) - 2, arg & 8); break;
        default: step_cycle_dots(&st); step_set_gate(&st, (int)(arg % 3) - 1); break;
        }
        if (st.pointer > song_end(&song) - 1) st.pointer = 0;
        bad += !pool_sound();
    }
    CHECK_EQ(bad, 0);
    CHECK(song.events > SONG_EVENTS - 400);
}

int main(void)
{
    test_lengths();
    test_entering();
    test_tie();
    test_deleting();
    test_selection();
    test_pitch_knob();
    test_full_pool();
    return report("test_step");
}
