/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* TUNE (0.6.6): a sampler note's semitones in its glitch word. Packing and
 * clamping, the table, the player telling the port before each sampler
 * hit and only then, step entry's SHIFT+K4 and its previews, Normalize and
 * a take keeping it, and song files: version 5 holds chromatic mode, a
 * tuned note round trips, and older files load as before. */
#include "harness.h"
#include "glitch.h"
#include "jobs.h"
#include "record.h"
#include "step.h"
#include "song_file.h"
#include "pool.h"

enum { SAMPLER = 4, DRUMS = 7, SYNTH = 1 };
static struct song song, other;
static struct song_player player, live;
static struct song_recorder recorder;
static struct song_step st;

/* What the player told the port, and how many notes it had sent by then:
 * the next one is the hit it tuned for. */
struct told { uint32_t block; uint8_t track, pitch; int8_t semitones; unsigned before; };
static struct told told[256];
static unsigned told_count;
static void tell(void *context, unsigned track, unsigned pitch, int semitones)
{
    (void)context;
    if (told_count < 256) told[told_count++] = (struct told){block_now, (uint8_t)track, (uint8_t)pitch, (int8_t)semitones, heard_count};
}
static const struct song_output tuner = {.note = hear, .tune = tell};

static uint32_t tune(int semitones) { return song_glitch_turn(0, GLITCH_TUNE, semitones); }
static void note(unsigned track, uint32_t at, unsigned pitch, uint32_t word)
{
    int g = song_glitch_intern(&song, word);
    const struct song_event e = {at | (uint32_t)(g > 0 ? g : 0) << SONG_GLITCH_SHIFT, (uint8_t)pitch, 100, 120};
    CHECK_EQ(song_add_event(&song, track, &e), 0);
}
static uint32_t word_at(const struct song *s, unsigned track, unsigned i)
{
    unsigned n;
    const struct song_event *e = song_track(s, track, &n);
    return i < n ? song_glitch_word(s, &e[i]) : 0xffffffffU;
}
static void pads(struct song_player *p)
{
    for (unsigned i = 0; i < 8; ++i) p->pads[SAMPLER][i] = p->pads[DRUMS][i] = (uint8_t)(36 + i);
    p->pad_count[SAMPLER] = p->pad_count[DRUMS] = 8;
    p->samplers = 1U << SAMPLER;
}

static void test_words(void)
{
    CHECK_EQ(tune(3), 3U << 20);
    CHECK_EQ(tune(-1), 63U << 20);                       /* two's complement in bits 20 to 25 */
    CHECK_EQ(song_glitch_tune(tune(-1)), -1);
    CHECK_EQ(song_glitch_tune(tune(24)), 24);
    CHECK_EQ(song_glitch_tune(tune(30)), 24);            /* clamped */
    CHECK_EQ(song_glitch_tune(tune(-30)), -24);
    CHECK_EQ(song_glitch_tune(song_glitch_turn(tune(24), GLITCH_TUNE, 1)), 24);
    CHECK_EQ(song_glitch_tune(song_glitch_turn(tune(-24), GLITCH_TUNE, -1)), -24);
    CHECK_EQ(song_glitch_tune(song_glitch_turn(tune(5), GLITCH_TUNE, -7)), -2);
    CHECK_EQ(song_glitch_tune(30U << 20), 24);           /* a word from outside, past the range */
    CHECK_EQ(song_glitch_tune(33U << 20), -24);
    CHECK_EQ(song_glitch_tune(0), 0);
    /* The other fields stay as they were, and so do bits 26 to 31. */
    uint32_t w = song_glitch_turn(song_glitch_turn(song_glitch_turn(0, GLITCH_REPEATS, 3), GLITCH_SWAP, 3), GLITCH_DRIFT, -8);
    uint32_t t = song_glitch_turn(w | 0xfc000000U, GLITCH_TUNE, -7);
    CHECK_EQ(song_glitch_tune(t), -7);
    CHECK_EQ(song_glitch_repeats(t), 4);
    CHECK_EQ(song_glitch_percent(t, GLITCH_SWAP), 100);
    CHECK_EQ(song_glitch_drift(t), -8);
    CHECK_EQ(t & 0xfc000000U, 0xfc000000U);
    CHECK_EQ(song_glitch_turn(t, GLITCH_TUNE, 7), w | 0xfc000000U);
    for (unsigned f = 0; f < GLITCH_TUNE; ++f) CHECK_EQ(song_glitch_field(tune(-24), f), 0); /* TUNE alone sets no other field */
    /* The table: TUNE 0 alone needs no entry, a tune its own, the same tune the same. */
    song_clear(&song, 8, 12000);
    CHECK_EQ(song_glitch_intern(&song, tune(0)), 0);
    int a = song_glitch_intern(&song, tune(2)), b = song_glitch_intern(&song, tune(-2)), c = song_glitch_intern(&song, tune(2));
    CHECK(a > 0 && b > 0 && a != b && a == c);
    CHECK(song_glitch_intern(&song, tune(2) | song_glitch_turn(0, GLITCH_FLAM, 1)) != a);
    /* 31 tunes in use fill it, and the next is refused, as any glitch is. */
    song_clear(&song, 8, 12000);
    for (int i = 1; i <= 31; ++i) note(SAMPLER, (uint32_t)i * 120U, 36, tune(i <= 24 ? i : 24 - i));
    CHECK_EQ(song_glitch_intern(&song, tune(-8)), -1);
    CHECK_EQ(song_glitch_intern(&song, tune(-7)), 31);
}

/* Each sampler hit is told its tune just before it goes, repeats and a
 * flam's grace hit too, untuned ones as 0. Other tracks never are. */
static void test_player(void)
{
    song_clear(&song, 8, 12000);
    note(SAMPLER, 0, 36, 0);
    note(SAMPLER, 480, 36, tune(5));
    note(SAMPLER, 960, 38, song_glitch_turn(tune(-3), GLITCH_REPEATS, 1));
    note(SAMPLER, 1440, 40, song_glitch_turn(tune(2), GLITCH_FLAM, 1));
    note(SAMPLER, 1920, 36, tune(24) | song_glitch_turn(0, GLITCH_SWAP, 3));
    note(SYNTH, 480, 60, tune(5));
    note(DRUMS, 480, 36, tune(5));
    song_player_init(&player, &song, &tuner);
    pads(&player);
    reset_capture();
    told_count = 0;
    block_now = 0;
    song_player_start(&player, 0);
    run_blocks(&player, 0, 1200);
    song_player_stop(&player);
    static const int8_t want[] = {0, 5, -3, -3, 2, 2, 24};
    unsigned bad = 0;
    CHECK_EQ(told_count, sizeof want);
    for (unsigned i = 0; i < told_count && i < sizeof want; ++i) {
        const struct heard *h = told[i].before < heard_count ? &heard[told[i].before] : NULL;
        bad += told[i].track != SAMPLER || told[i].semitones != want[i] || !h || !h->velocity || h->track != SAMPLER ||
               h->pitch != told[i].pitch || h->block != told[i].block;
    }
    CHECK_EQ(bad, 0);
    CHECK(told_count > 6 && told[6].pitch >= 36 && told[6].pitch < 44); /* a swapped pad plays the note's tune */
    CHECK(balanced());
    /* No hook, no harm: the plain listener plays the same notes. */
    unsigned tuned_notes = heard_count;
    song_player_init(&player, &song, &listener);
    pads(&player);
    reset_capture();
    song_player_start(&player, 0);
    run_blocks(&player, 0, 1200);
    song_player_stop(&player);
    CHECK_EQ(heard_count, tuned_notes);
}

/* Step entry: SHIFT+K4's edit turns the selected notes' TUNE, clamped, and
 * a preview tells the port the tune first. */
static void test_step(void)
{
    song_clear(&song, 8, 12000);
    note(SAMPLER, 480, 36, 0);
    note(SAMPLER, 480, 38, tune(1));
    step_init(&st, &song, &tuner);
    step_begin(&st, SAMPLER, 480);
    reset_capture();
    told_count = 0;
    CHECK_EQ(step_set_glitch(&st, GLITCH_TUNE, 3), 0);
    CHECK_EQ(song_glitch_tune(word_at(&song, SAMPLER, 0)), 3);
    CHECK_EQ(song_glitch_tune(word_at(&song, SAMPLER, 1)), 4);
    CHECK_EQ(song_glitch_tune(st.glitch), 3);           /* the entry shows the first note's */
    step_preview(&st);
    CHECK(told_count == 2 && told[0].semitones == 3 && told[1].semitones == 4 && heard_count == 2 && told[1].before == 1);
    CHECK_EQ(step_set_glitch(&st, GLITCH_TUNE, -40), 0);
    CHECK(song_glitch_tune(word_at(&song, SAMPLER, 0)) == -24 && song_glitch_tune(word_at(&song, SAMPLER, 1)) == -24);
    /* On an empty spot it sets the entry, and the next note goes in with it. */
    step_begin(&st, SAMPLER, 960);
    st.glitch = 0;
    CHECK_EQ(step_set_glitch(&st, GLITCH_TUNE, -5), 0);
    step_key(&st, 40);
    CHECK_EQ(step_commit(&st), 0);
    CHECK_EQ(song_glitch_tune(word_at(&song, SAMPLER, 2)), -5);
    step_preview_stop(&st);
}

/* Normalize: a sampler note's TUNE stays on each note it makes, alone in
 * its word. An 808 track's goes, as it never played. */
static void test_normalize(void)
{
    song_clear(&song, 2, 12000);
    note(SAMPLER, 0, 36, tune(-4));
    note(SAMPLER, 960, 38, song_glitch_turn(tune(7), GLITCH_REPEATS, 1));
    note(SAMPLER, 1920, 40, 0);
    note(DRUMS, 0, 36, tune(9));
    song_player_init(&live, &song, &listener);
    pads(&live);
    CHECK_EQ(song_job_normalize(&song, 1U << SAMPLER | 1U << DRUMS, &live), 5);
    unsigned n;
    song_track(&song, SAMPLER, &n);
    CHECK_EQ(n, 4);
    CHECK(word_at(&song, SAMPLER, 0) == tune(-4) && word_at(&song, SAMPLER, 1) == tune(7) && word_at(&song, SAMPLER, 2) == tune(7));
    CHECK_EQ(word_at(&song, SAMPLER, 3), 0);
    CHECK_EQ(word_at(&song, DRUMS, 0), 0);
    const struct song_event *e = song_track(&song, SAMPLER, &n);
    CHECK(song_event_start(&e[1]) == 960 && song_event_start(&e[2]) == 1020 && e[1].a == 38 && song_event_kind(&e[1]) == SONG_NOTE);
}

/* A take: the note started with `tune` set lands with it, and the next
 * goes in plain. */
static void test_take(void)
{
    song_clear(&song, 8, 12000);
    song_rec_start(&recorder, &song, SAMPLER, SONG_REC_OVERDUB, 0);
    recorder.tune = 7;
    song_rec_note(&recorder, 36, 100, 480);
    CHECK_EQ(recorder.tune, 0);
    song_rec_note(&recorder, 36, 0, 600);
    song_rec_note(&recorder, 38, 100, 960);
    recorder.tune = -12;
    song_rec_note(&recorder, 38, 0, 1000);              /* a key going up takes it too, and keeps none */
    recorder.tune = 2;
    song_rec_early(&recorder, 40, 90, 0, 60);           /* one caught from before the take, let go */
    song_rec_stop(&recorder, 1920);
    unsigned n;
    const struct song_event *e = song_track(&song, SAMPLER, &n);
    CHECK_EQ(n, 3);
    CHECK(e[0].a == 40 && song_glitch_tune(song_glitch_word(&song, &e[0])) == 2);
    CHECK(e[1].a == 36 && song_event_start(&e[1]) == 480 && song_glitch_word(&song, &e[1]) == tune(7));
    CHECK(e[2].a == 38 && song_glitch_word(&song, &e[2]) == 0);
    CHECK(!(e[1].at & SONG_FRESH));
}

/* Song files: version 5 holds chromatic mode after the chords, TUNE rides
 * in the glitch table, version 4 loads with chromatic mode off and its
 * TUNE words as they are, and a setting past 15 is refused. */
enum { SECTORS = 8, BYTES = SECTORS * POOL_SECTOR };
static uint8_t flash[BYTES], file[BYTES];
static int f_read(uint32_t at, void *data, size_t n) { if (at + n > BYTES) return -1; memcpy(data, flash + at, n); return 0; }
static int f_write(uint32_t at, const void *data, size_t n) { if (at + n > BYTES) return -1; memcpy(flash + at, data, n); return 0; }
static int f_erase(uint32_t at) { if (at >= BYTES) return -1; memset(flash + at, 0xff, POOL_SECTOR); return 0; }
static const uint8_t *f_map(uint32_t at, size_t n) { return at + n <= BYTES ? flash + at : NULL; }
static const struct song_flash ram = {f_read, f_write, f_erase, f_map};
static uint32_t write_song(const struct song *s)
{
    static struct song_pool pool;
    struct pool_writer w;
    uint8_t sounds[4] = {1, 2, 3, 4};
    memset(flash, 0xff, sizeof flash);
    pool_scan(&pool, &ram, 0, SECTORS, SECTORS, SECTORS);
    uint32_t bytes = song_file_bytes(s, sizeof sounds);
    if (pool_begin(&w, &pool, POOL_SONG, 1, "S", bytes)) return 0;
    song_file_write(&w, s, sounds, sizeof sounds);
    if (pool_end(&w)) return 0;
    memcpy(file, f_map(pool_address(pool_find(&pool, POOL_SONG, 1)), bytes), bytes);
    return bytes;
}
static void test_file(void)
{
    const uint8_t *sounds;
    uint32_t n;
    song_clear(&song, 4, 12000);
    note(SAMPLER, 0, 36, tune(-24));
    note(SAMPLER, 480, 38, song_glitch_turn(tune(13), GLITCH_FLAM, 1));
    note(SYNTH, 0, 60, tune(5));
    song.chromatic[SAMPLER] = SONG_CHROMATIC_ON | 3;
    song.chromatic[6] = 5;
    uint32_t bytes = write_song(&song);
    CHECK(bytes > 0 && file[0] == 5 && SONG_FILE_VERSION == 5);
    CHECK(!memcmp(file + bytes - SONG_TRACKS, song.chromatic, SONG_TRACKS)); /* the last 8 bytes */
    CHECK_EQ(song_file_read(&other, file, bytes, &sounds, &n), 0);
    CHECK(!memcmp(other.chromatic, song.chromatic, sizeof song.chromatic));
    CHECK(song_chromatic_on(&other, SAMPLER) && song_chromatic_pad(&other, SAMPLER) == 3 && !song_chromatic_on(&other, 6));
    CHECK(other.events == song.events && !memcmp(other.event, song.event, song.events * sizeof song.event[0]));
    CHECK(word_at(&other, SAMPLER, 0) == tune(-24) && song_glitch_tune(word_at(&other, SAMPLER, 1)) == 13 &&
          word_at(&other, SYNTH, 0) == tune(5));
    CHECK_EQ(write_song(&other), bytes);                 /* back to the same bytes */
    /* Version 4: no chromatic bytes. It loads with chromatic mode off, the
     * TUNE words as they were, and writes back as version 5. */
    file[0] = 4;
    CHECK_EQ(song_file_read(&other, file, bytes - SONG_TRACKS, &sounds, &n), 0);
    unsigned any = 0;
    for (unsigned t = 0; t < SONG_TRACKS; ++t) any |= other.chromatic[t];
    CHECK_EQ(any, 0);
    CHECK(word_at(&other, SAMPLER, 0) == tune(-24) && song_glitch_tune(word_at(&other, SAMPLER, 1)) == 13);
    CHECK_EQ(song_file_check(file, bytes, &sounds, &n), -1); /* version 4 with the chromatic bytes after */
    CHECK_EQ(write_song(&other), bytes);
    CHECK_EQ(file[0], 5);
    /* Each value of track 2's chromatic byte: past 15 is refused. */
    unsigned bad = 0;
    for (unsigned v = 0; v < 256; ++v) {
        file[bytes - SONG_TRACKS + 2U] = (uint8_t)v;
        bad += (song_file_check(file, bytes, &sounds, &n) != 0) != (v > 15);
    }
    CHECK_EQ(bad, 0);
    file[bytes - SONG_TRACKS + 2U] = 0;
    /* A glitch word holding any TUNE bits loads: the loader takes every word. */
    song_clear(&song, 4, 12000);
    note(SAMPLER, 0, 36, 31U << 20);                    /* raw 31, past 24: plays as 24 */
    bytes = write_song(&song);
    CHECK_EQ(song_file_read(&other, file, bytes, &sounds, &n), 0);
    CHECK_EQ(song_glitch_tune(word_at(&other, SAMPLER, 0)), 24);
}

int main(void)
{
    test_words();
    test_player();
    test_step();
    test_normalize();
    test_take();
    test_file();
    return report("test_tune");
}
