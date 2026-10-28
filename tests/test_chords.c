/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Chord mode (phase 8, design 8g), the model: every type on every root,
 * in key across the scales and the rule for a root outside one, close,
 * spread and smooth voicing (smooth's inversions over a progression), the
 * strum's offsets by direction (alternating flipping each chord, random
 * seeded and repeatable) and in exact audio frames at three tempos through
 * the live queue, a strummed chord quantized as a whole when recorded and
 * requantized, step entry of a chord with and without a strum, and the
 * settings in song files. The keys themselves are in preview_8.mjs. */
#include "harness.h"
#include "chords.h"
#include "record.h"
#include "step.h"
#include "song_file.h"
#include "pool.h"

static struct song song, other;
static struct song_chord_play play;
static uint8_t out[CHORD_NOTES];
static unsigned set(unsigned t, unsigned on, unsigned voicing, unsigned strum, unsigned direction)
{
    song.chord[t] = song_chord_setting(on, voicing, strum, direction);
    return song.chord[t];
}
static int same(const uint8_t *a, unsigned n, const int *want, unsigned m)
{
    if (n != m) return 0;
    for (unsigned i = 0; i < n; ++i) if (a[i] != want[i]) return 0;
    return 1;
}

/* Every type on every root, close: the shape above the root, notes past 127 left out. */
static void test_types(void)
{
    static const int shape[CHORD_TYPES][5] = {{3, 0, 4, 7}, {3, 0, 4, 7}, {3, 0, 3, 7}, {4, 0, 4, 7, 10}, {4, 0, 3, 7, 10},
        {4, 0, 4, 7, 11}, {4, 0, 4, 7, 9}, {3, 0, 2, 7}, {3, 0, 5, 7}, {3, 0, 3, 6}, {3, 0, 4, 8}, {4, 0, 4, 7, 14}};
    song_clear(&song, 4, 12000);                            /* no key: in key is major */
    set(0, 1, CHORD_CLOSE, 0, CHORD_UP);
    unsigned wrong = 0, chords = 0;
    for (unsigned type = 0; type < CHORD_TYPES; ++type)
        for (unsigned root = 0; root < 128; ++root, ++chords) {
            int want[4];
            unsigned m = 0;
            for (int i = 0; i < shape[type][0]; ++i) if (root + shape[type][1 + i] < 128) want[m++] = (int)root + shape[type][1 + i];
            song_chord_reset(&play, &song, 0);
            wrong += !same(out, song_chord_voice(&song, 0, root, type, &play, out), want, m);
        }
    CHECK_EQ(chords, 12 * 128);
    CHECK_EQ(wrong, 0);
    /* The layout: the common types on the white keys, the color ones on the black. */
    static const uint8_t white[7] = {0, 2, 4, 6, 7, 9, 11};
    unsigned seen = 0;
    for (unsigned k = 0; k < 12; ++k) seen |= 1U << song_chord_keys_layout[k];
    CHECK_EQ(seen, (1U << CHORD_TYPES) - 1U);
    static const uint8_t whites[7] = {CHORD_IN_KEY, CHORD_MAJOR, CHORD_MINOR, CHORD_SEVEN, CHORD_MINOR7, CHORD_MAJOR7, CHORD_SIX};
    for (unsigned i = 0; i < 7; ++i) CHECK_EQ(song_chord_keys_layout[white[i]], whites[i]);
}

/* In key: the root and the second and fourth notes of the song's scale
 * above it, on every root of every key of every scale. In C major the D key
 * plays D minor, and C#, outside the scale, C# diminished. */
static const uint16_t scales[SONG_SCALES] = {0xfff, 0xab5, 0x5ad, 0x6ad, 0x5ab, 0xad5, 0x6b5, 0x56b, 0x9ad, 0xaad, 0x295, 0x4a9, 0x4e9};
static void test_in_key(void)
{
    song_clear(&song, 4, 12000);
    set(0, 1, CHORD_CLOSE, 0, CHORD_UP);
    unsigned wrong = 0, chords = 0;
    for (unsigned scale = 1; scale < SONG_SCALES; ++scale)
        for (unsigned key = 0; key < 12; ++key) {
            song.key_root = (uint8_t)key;
            song.key_scale = (uint8_t)scale;
            for (unsigned root = 48; root < 72; ++root, ++chords) {
                int want[3] = {(int)root, 0, 0};
                unsigned found = 0;
                for (unsigned p = root + 1; found < 4; ++p)
                    if (scales[scale] >> ((p + 12U - key) % 12U) & 1U) { if (++found == 2) want[1] = (int)p; else if (found == 4) want[2] = (int)p; }
                song_chord_reset(&play, &song, 0);
                wrong += !same(out, song_chord_voice(&song, 0, root, CHORD_IN_KEY, &play, out), want, 3);
            }
        }
    CHECK_EQ(chords, 12 * 12 * 24);
    CHECK_EQ(wrong, 0);
    song.key_root = 0;
    song.key_scale = 1;                                      /* C major */
    static const int d_minor[3] = {62, 65, 69}, c_sharp_dim[3] = {61, 64, 67}, b_dim[3] = {59, 62, 65};
    CHECK(same(out, song_chord_voice(&song, 0, 62, CHORD_IN_KEY, &play, out), d_minor, 3));
    CHECK(same(out, song_chord_voice(&song, 0, 61, CHORD_IN_KEY, &play, out), c_sharp_dim, 3));
    CHECK(same(out, song_chord_voice(&song, 0, 59, CHORD_IN_KEY, &play, out), b_dim, 3));
    song.key_scale = 10;                                     /* C major pentatonic: C D E G A */
    static const int c_penta[3] = {60, 64, 69}, f_penta[3] = {65, 69, 74};
    CHECK(same(out, song_chord_voice(&song, 0, 60, CHORD_IN_KEY, &play, out), c_penta, 3));
    CHECK(same(out, song_chord_voice(&song, 0, 65, CHORD_IN_KEY, &play, out), f_penta, 3)); /* F, outside: A and D above */
}

/* Spread lifts the second note an octave, on every type. Smooth takes the
 * inversion nearest the last chord: C, F, G, C in C major moves by steps,
 * and a chord climbing too far comes back down an octave. */
static void test_voicings(void)
{
    song_clear(&song, 4, 12000);
    set(1, 1, CHORD_SPREAD, 0, CHORD_UP);
    static const int c_spread[3] = {60, 67, 76}, c7_spread[4] = {60, 67, 70, 76}, sus2_spread[3] = {60, 67, 74}, add9_spread[4] = {60, 67, 74, 76};
    CHECK(same(out, song_chord_voice(&song, 1, 60, CHORD_MAJOR, &play, out), c_spread, 3));
    CHECK(same(out, song_chord_voice(&song, 1, 60, CHORD_SEVEN, &play, out), c7_spread, 4));
    CHECK(same(out, song_chord_voice(&song, 1, 60, CHORD_SUS2, &play, out), sus2_spread, 3));
    CHECK(same(out, song_chord_voice(&song, 1, 60, CHORD_ADD9, &play, out), add9_spread, 4));
    unsigned wrong = 0;
    uint8_t close[CHORD_NOTES];
    for (unsigned type = 1; type < CHORD_TYPES; ++type) { /* the rule on every type */
        set(1, 1, CHORD_CLOSE, 0, CHORD_UP);
        unsigned n = song_chord_voice(&song, 1, 48, type, &play, close);
        set(1, 1, CHORD_SPREAD, 0, CHORD_UP);
        unsigned m = song_chord_voice(&song, 1, 48, type, &play, out);
        int want[4] = {close[0], 0, 0, 0}, k = 1;
        for (unsigned i = 2; i < n; ++i) if (close[i] < close[1] + 12) want[k++] = close[i];
        want[k++] = close[1] + 12;
        for (unsigned i = 2; i < n; ++i) if (close[i] >= close[1] + 12) want[k++] = close[i];
        wrong += !same(out, m, want, (unsigned)k);
    }
    CHECK_EQ(wrong, 0);
    set(2, 1, CHORD_SMOOTH, 0, CHORD_UP);
    song_chord_reset(&play, &song, 2);
    static const int progression[4][3] = {{60, 64, 67}, {60, 65, 69}, {59, 62, 67}, {60, 64, 67}};
    static const uint8_t roots[4] = {60, 65, 67, 60};
    for (unsigned i = 0; i < 4; ++i) CHECK(same(out, song_chord_voice(&song, 2, roots[i], CHORD_MAJOR, &play, out), progression[i], 3));
    song_chord_reset(&play, &song, 2);   /* round the circle of fifths: each change a few steps, never far off */
    unsigned high = 0, low = 127, far = 0;
    uint8_t before[CHORD_NOTES];
    for (unsigned i = 0, root = 48; i < 12; ++i, root = 48 + (root - 48 + 7) % 12) {
        unsigned n = song_chord_voice(&song, 2, root, CHORD_MAJOR, &play, out), moved = 0;
        for (unsigned k = 0; i && k < n; ++k) { /* each note to the nearest of the last chord's */
            unsigned best = 128;
            for (unsigned j = 0; j < 3; ++j) { unsigned d = out[k] > before[j] ? out[k] - before[j] : before[j] - out[k]; if (d < best) best = d; }
            moved += best;
        }
        far += moved > 4;
        memcpy(before, out, 3);
        if (out[n - 1] > high) high = out[n - 1];
        if (out[0] < low) low = out[0];
    }
    CHECK_EQ(far, 0);
    CHECK(low >= 44 && high <= 67);
    song_chord_reset(&play, &song, 2);                       /* the first chord: close, root position */
    static const int g_close[3] = {67, 71, 74};
    CHECK(same(out, song_chord_voice(&song, 2, 67, CHORD_MAJOR, &play, out), g_close, 3));
    static const int g7_from_c[4] = {65, 67, 71, 74};         /* from G: the 7 lands on F below */
    CHECK(same(out, song_chord_voice(&song, 2, 67, CHORD_SEVEN, &play, out), g7_from_c, 4));
}

/* Strum offsets, up, down, alternating and random, over an 8th: first note
 * to last. In exact audio frames at three tempos, through the live queue. */
static uint8_t sent_pitch[64];
static uint64_t sent_before[64];
static unsigned sent;
static uint64_t window;
static void sent_note(unsigned track, unsigned pitch, unsigned velocity)
{
    (void)track; (void)velocity;
    sent_before[sent] = window;
    sent_pitch[sent++] = (uint8_t)pitch;
}
static void test_strum(void)
{
    uint8_t off[CHORD_NOTES];
    song_clear(&song, 4, 12000);
    song.seed = 77;
    set(3, 1, CHORD_CLOSE, 4, CHORD_UP);
    song_chord_strum(&song, 3, 4, &play, off);
    CHECK(off[0] == 0 && off[1] == 80 && off[2] == 160 && off[3] == 240);
    song_chord_strum(&song, 3, 3, &play, off);
    CHECK(off[0] == 0 && off[1] == 120 && off[2] == 240);
    set(3, 1, CHORD_CLOSE, 2, CHORD_DOWN);
    song_chord_strum(&song, 3, 3, &play, off);
    CHECK(off[0] == 60 && off[1] == 30 && off[2] == 0);
    set(3, 1, CHORD_CLOSE, 0, CHORD_UP);
    song_chord_strum(&song, 3, 3, &play, off);
    CHECK(off[0] == 0 && off[1] == 0 && off[2] == 0);         /* strum off: together */
    set(3, 1, CHORD_CLOSE, 3, CHORD_ALTERNATE);
    song_chord_reset(&play, &song, 3);
    for (unsigned k = 0; k < 6; ++k) {
        song_chord_strum(&song, 3, 3, &play, off);
        CHECK(k % 2U ? off[0] == 120 && off[2] == 0 : off[0] == 0 && off[2] == 120); /* up, down, up... */
    }
    /* Random: a fresh order each chord, the same orders again from the same seed. */
    set(3, 1, CHORD_CLOSE, 4, CHORD_RANDOM);
    uint8_t first[40][CHORD_NOTES];
    unsigned orders = 0, permutations = 1;
    song_chord_reset(&play, &song, 3);
    for (unsigned k = 0; k < 40; ++k) {
        song_chord_strum(&song, 3, 4, &play, first[k]);
        unsigned seen = 0;
        for (unsigned i = 0; i < 4; ++i) seen |= 1U << first[k][i] / 80U;
        permutations &= seen == 15U;
        orders += k && memcmp(first[k], first[k - 1], 4);
    }
    CHECK(permutations);
    CHECK(orders > 25);
    song_chord_reset(&play, &song, 3);
    unsigned repeat = 1;
    for (unsigned k = 0; k < 40; ++k) { song_chord_strum(&song, 3, 4, &play, off); repeat &= !memcmp(off, first[k], 4); }
    CHECK(repeat);
    song.seed = 78;
    song_chord_reset(&play, &song, 3);
    song_chord_strum(&song, 3, 4, &play, off);
    unsigned differs = memcmp(off, first[0], 4) != 0;
    for (unsigned k = 1; k < 6; ++k) { song_chord_strum(&song, 3, 4, &play, off); differs |= memcmp(off, first[k], 4) != 0; }
    CHECK(differs);
    /* The queue: each note at frame + offset in exact frames, sent before the block holding it renders. */
    static const uint16_t tempos[3] = {12000, 13300, 9050};
    static const uint8_t pitch[4] = {60, 64, 67, 70}, offset[4] = {240, 0, 160, 80}; /* down the order, as random gave */
    unsigned wrong = 0;
    for (unsigned k = 0; k < 3; ++k) {
        static struct song_strum q;
        memset(&q, 0, sizeof q);
        uint64_t from = 1000003 + k * 77;
        song_strum_add(&q, from, tempos[k], 3, pitch, offset, 4, 100);
        for (unsigned j = 0; j < 4; ++j) { /* each at its exact frame, rounded down */
            wide num = (wide)from * 480 * tempos[k] + (wide)offset[j] * SONG_UNITS_PER_CLOCK, den = (wide)480 * tempos[k];
            wrong += q.frame[j] != (uint64_t)(num / den);
        }
        sent = 0;
        for (uint64_t block = from / 128; q.count && block < from / 128 + 400; ++block) {
            window = (block + 1) * 128;
            song_strum_due(&q, window, sent_note);
        }
        CHECK_EQ(sent, 4);
        static const uint8_t order[4] = {64, 70, 67, 60};
        for (unsigned i = 0; i < 4; ++i) {
            unsigned j = 0;
            while (pitch[j] != order[i]) ++j;
            wide num = (wide)from * 480 * tempos[k] + (wide)offset[j] * SONG_UNITS_PER_CLOCK, den = (wide)480 * tempos[k];
            uint64_t frame = (uint64_t)(num / den);                /* exact, rounded down */
            wrong += sent_pitch[i] != order[i] || sent_before[i] != (frame / 128 + 1) * 128;
        }
    }
    CHECK_EQ(wrong, 0);
    static struct song_strum q;
    memset(&q, 0, sizeof q);
    song_strum_add(&q, 0, 12000, 2, pitch, offset, 4, 100);
    sent = 0;
    window = 128;
    song_strum_due(&q, window, sent_note);                     /* the 0 offset goes out first */
    CHECK(sent == 1 && sent_pitch[0] == 64);
    CHECK_EQ(song_strum_drop(&q, 2, 64), 0);                   /* already out */
    CHECK_EQ(song_strum_drop(&q, 2, 70), 1);                   /* still to come: gone */
    CHECK_EQ(q.count, 2);
    for (unsigned i = 0; i < 20; ++i) song_strum_add(&q, 0, 12000, 1, pitch, offset, 4, 100);
    CHECK_EQ(q.count, STRUM_QUEUE);                            /* a full queue leaves the rest out */
}

/* Recording: a strummed chord's notes come with their leads after its first
 * note, so input quantize moves the chord as a whole, and so does
 * requantizing. A note let go before it sounded is dropped. */
static void test_record(void)
{
    static struct song_recorder r;
    song_clear(&song, 8, 12000);
    song.quantize = 5;                                         /* a 16th */
    song.strength = 100;
    song_rec_start(&r, &song, 4, SONG_REC_OVERDUB, 0);
    static const uint8_t lead[3] = {0, 40, 80};
    for (unsigned i = 0; i < 3; ++i) song_rec_lead(&r, 60 + i * 4, 100, 1000 + lead[i], lead[i]);
    for (unsigned i = 0; i < 3; ++i) song_rec_note(&r, 60 + i * 4, 0, 1300);
    song_rec_lead(&r, 72, 100, 2000, 0);                       /* a second chord, its top note dropped */
    song_rec_lead(&r, 76, 100, 2030, 30);
    song_rec_drop(&r, 76);
    song_rec_note(&r, 72, 0, 2200);
    song_rec_stop(&r, 2300);
    unsigned count;
    const struct song_event *e = song_track(&song, 4, &count);
    CHECK_EQ(count, 4);
    CHECK(count == 4 && song_event_start(&e[0]) == 960 && song_event_start(&e[1]) == 1000 && song_event_start(&e[2]) == 1040);
    CHECK(count == 4 && e[0].length == 300 && e[2].length == 220 && song_event_start(&e[3]) == 2040);
    /* Plain notes snap one by one, as before. */
    song_clear(&song, 8, 12000);
    song.quantize = 5;
    song.strength = 100;
    song_rec_start(&r, &song, 4, SONG_REC_OVERDUB, 0);
    for (unsigned i = 0; i < 3; ++i) song_rec_note(&r, 60 + i * 4, 100, 1000 + lead[i]);
    for (unsigned i = 0; i < 3; ++i) song_rec_note(&r, 60 + i * 4, 0, 1300);
    song_rec_stop(&r, 1400);
    e = song_track(&song, 4, &count);
    CHECK(count == 3 && song_event_start(&e[0]) == 960 && song_event_start(&e[1]) == 1080 && song_event_start(&e[2]) == 1080);
    /* Requantized to an 8th, the strum moves whole again. */
    song_clear(&song, 8, 12000);
    song.quantize = 5;
    song.strength = 100;
    song_rec_start(&r, &song, 4, SONG_REC_OVERDUB, 0);
    for (unsigned i = 0; i < 3; ++i) song_rec_lead(&r, 60 + i * 4, 100, 1100 + lead[i], lead[i]);
    for (unsigned i = 0; i < 3; ++i) song_rec_note(&r, 60 + i * 4, 0, 1400);
    song_rec_stop(&r, 1500);
    e = song_track(&song, 4, &count);
    CHECK(count == 3 && song_event_start(&e[0]) == 1080 && song_event_start(&e[2]) == 1160);
    song.quantize = 7;                                         /* an 8th */
    CHECK_EQ(song_rec_requantize(&r), 3);
    e = song_track(&song, 4, &count);
    CHECK(count == 3 && song_event_start(&e[0]) == 1200 && song_event_start(&e[1]) == 1240 && song_event_start(&e[2]) == 1280);
}

/* Step entry: a chord at the pointer, strummed or not, ends together; repeat
 * enters the strum again and tie lengthens each note to the same end. */
static void test_step(void)
{
    static struct song_step st;
    static const struct song_output none = {0};
    static const uint8_t pitch[3] = {60, 64, 67}, flat[3] = {0, 0, 0}, strum[3] = {0, 30, 60};
    song_clear(&song, 8, 12000);
    step_init(&st, &song, &none);
    step_begin(&st, 5, 0);
    st.velocity = 100;
    step_pick_length(&st, STEP_QUARTER);
    step_chord(&st, pitch, flat, 3);
    CHECK_EQ(step_commit(&st), 0);
    unsigned count;
    const struct song_event *e = song_track(&song, 5, &count);
    CHECK_EQ(count, 3);
    for (unsigned i = 0; i < count; ++i) CHECK(song_event_start(&e[i]) == 0 && e[i].a == pitch[i]);
    uint32_t gate = e[0].length;
    step_chord(&st, pitch, strum, 3);
    CHECK_EQ(step_commit(&st), 0);
    e = song_track(&song, 5, &count);
    CHECK_EQ(count, 6);
    for (unsigned i = 0; i < 3 && count == 6; ++i)
        CHECK(song_event_start(&e[3 + i]) == 480 + strum[i] && e[3 + i].a == pitch[i] && e[3 + i].length == gate - strum[i]);
    CHECK_EQ(step_repeat(&st), 0);                             /* the strum again at 960 */
    e = song_track(&song, 5, &count);
    CHECK(count == 9 && song_event_start(&e[7]) == 990 && song_event_start(&e[8]) == 1020);
    CHECK_EQ(step_tie(&st), 0);                                /* each to the same later end */
    e = song_track(&song, 5, &count);
    CHECK(count == 9 && e[6].length + 0U == e[7].length + 30U && e[7].length + 30U == e[8].length + 60U && e[6].length > gate);
}

/* Song files: the settings and types round trip in version 4, a version 3
 * file loads with chord mode off, and settings out of range are refused. */
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
    song_add_note(&song, 0, 0, 60, 100, 100);
    for (unsigned t = 0; t < SONG_TRACKS; ++t) {
        song.chord[t] = song_chord_setting(t & 1U, t % 3U, t % 5U, t % 4U);
        song.chord_type[t] = (uint8_t)(t * 3U % CHORD_TYPES);
    }
    uint32_t bytes = write_song(&song);
    CHECK(bytes > 0 && file[0] == 4);
    CHECK_EQ(song_file_read(&other, file, bytes, &sounds, &n), 0);
    CHECK(!memcmp(other.chord, song.chord, sizeof song.chord) && !memcmp(other.chord_type, song.chord_type, sizeof song.chord_type));
    CHECK_EQ(write_song(&other), bytes);
    file[0] = 3;                                               /* version 3: no chord bytes, chord mode off */
    CHECK_EQ(song_file_read(&other, file, bytes - 16U, &sounds, &n), 0);
    unsigned any = 0;
    for (unsigned t = 0; t < SONG_TRACKS; ++t) any |= other.chord[t] | other.chord_type[t];
    CHECK_EQ(any, 0);
    CHECK_EQ(song_file_check(file, bytes, &sounds, &n), -1);   /* version 3 with chord bytes after */
    file[0] = 4;
    unsigned refused = 0, bad = 0;
    for (unsigned v = 0; v < 256; ++v) { /* each value of track 2's settings byte, and of its type */
        file[bytes - 16U + 2U] = (uint8_t)v;
        unsigned valid = (v >> 1 & 3U) < 3U && (v >> 3 & 7U) < 5U;
        int r = song_file_check(file, bytes, &sounds, &n);
        refused += r != 0;
        bad += (r != 0) == valid;
    }
    CHECK_EQ(bad, 0);
    CHECK_EQ(refused, 256 - 2 * 3 * 5 * 4);
    file[bytes - 16U + 2U] = song.chord[2];
    for (unsigned v = 0; v < 256; ++v) {
        file[bytes - 8U + 5U] = (uint8_t)v;
        bad += (song_file_check(file, bytes, &sounds, &n) != 0) != (v >= CHORD_TYPES);
    }
    CHECK_EQ(bad, 0);
}

int main(void)
{
    test_types();
    test_in_key();
    test_voicings();
    test_strum();
    test_record();
    test_step();
    test_file();
    return report("test_chords");
}
