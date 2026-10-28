/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Songs as stored. See song_file.h. */
#include "song_file.h"
#include "song_config.h"
#include <string.h>
#if SONG_UI

static void put8(struct pool_writer *w, unsigned v) { uint8_t b = (uint8_t)v; pool_put(w, &b, 1); }
static void put16(struct pool_writer *w, unsigned v) { put8(w, v); put8(w, v >> 8); }
static void put32(struct pool_writer *w, uint32_t v) { put16(w, v & 0xffffU); put16(w, v >> 16); }

SONG_SMALL void song_file_write(struct pool_writer *w, const struct song *s, const uint8_t *sounds, uint32_t sounds_bytes)
{
    put8(w, SONG_FILE_VERSION);
    put16(w, sounds_bytes);
    pool_put(w, sounds, sounds_bytes);
    pool_put(w, s->name, sizeof s->name);
    put16(w, s->kit);
    pool_put(w, s->base_program, sizeof s->base_program);
    put16(w, s->bars);
    put8(w, s->tempos);
    for (unsigned k = 0; k < s->tempos; ++k) { put32(w, s->tempo[k].clock); put16(w, s->tempo[k].bpm100); }
    put8(w, s->meters);
    for (unsigned m = 0; m < s->meters; ++m) { put16(w, s->meter[m].bar); put8(w, s->meter[m].beats); put8(w, s->meter[m].unit); }
    put8(w, s->markers);
    for (unsigned i = 0; i < s->markers; ++i) { put32(w, s->marker[i].clock); pool_put(w, s->marker[i].name, SONG_MARKER_NAME); }
    put32(w, s->loop_start); put32(w, s->loop_end); put8(w, s->loop_on);
    const uint8_t settings[10] = {s->swing, s->swing_grid, s->swing_tracks, s->key_root, s->key_scale,
                                  s->rec_mode, s->count_in, s->metronome, s->quantize, s->strength};
    pool_put(w, settings, sizeof settings);
    pool_put(w, s->knob_cc, sizeof s->knob_cc);
    unsigned glitches = 0;
    for (unsigned i = 1; i < 32; ++i) glitches += s->glitch[i] != 0;
    put8(w, glitches);
    for (unsigned i = 1; i < 32; ++i) if (s->glitch[i]) { put8(w, i); put32(w, s->glitch[i]); }
    pool_put(w, s->humanize_time, sizeof s->humanize_time);
    pool_put(w, s->humanize_velocity, sizeof s->humanize_velocity);
    put32(w, s->seed);
    for (unsigned t = 0; t < SONG_TRACKS; ++t) {
        unsigned count;
        const struct song_event *e = song_track(s, t, &count);
        put16(w, count);
        for (unsigned i = 0; i < count; ++i) { put32(w, e[i].at & ~SONG_FRESH); put8(w, e[i].a); put8(w, e[i].b); put16(w, e[i].length); }
    }
}
SONG_SMALL uint32_t song_file_bytes(const struct song *s, uint32_t sounds_bytes)
{
    struct pool_writer w;
    memset(&w, 0, sizeof w);
    song_file_write(&w, s, NULL, 0);
    return w.written + sounds_bytes;
}
SONG_SMALL uint32_t song_file_crc(const struct song *s, const uint8_t *sounds, uint32_t sounds_bytes)
{
    struct pool_writer w;
    memset(&w, 0, sizeof w);
    w.crc = ~0U;
    song_file_write(&w, s, sounds, sounds_bytes);
    return ~w.crc;
}

/* Reading, with every count and value checked against the bounds. */
struct reader { const uint8_t *p; uint32_t left; int bad; };
static const uint8_t *take(struct reader *r, uint32_t n)
{
    if (r->bad || n > r->left) { r->bad = 1; return NULL; }
    const uint8_t *at = r->p;
    r->p += n; r->left -= n;
    return at;
}
static unsigned get8(struct reader *r) { const uint8_t *b = take(r, 1); return b ? b[0] : 0; }
static unsigned get16(struct reader *r) { unsigned lo = get8(r); return lo | get8(r) << 8; }
static uint32_t get32(struct reader *r) { uint32_t lo = get16(r); return lo | (uint32_t)get16(r) << 16; }
static void copy(struct reader *r, void *to, uint32_t n, unsigned apply)
{
    const uint8_t *b = take(r, n);
    if (b && apply) memcpy(to, b, n);
}
/* One pass checks (apply 0), the next fills the song. */
SONG_SMALL static int parse(struct song *s, const uint8_t *data, uint32_t bytes, unsigned apply,
                            const uint8_t **sounds, uint32_t *sounds_bytes)
{
    struct reader r = {data, bytes, 0};
    if (get8(&r) != SONG_FILE_VERSION) return -1;
    *sounds_bytes = get16(&r);
    *sounds = take(&r, *sounds_bytes);
    char name[12];
    copy(&r, name, sizeof name, 1);
    unsigned kit = get16(&r);
    uint8_t programs[SONG_TRACKS];
    copy(&r, programs, sizeof programs, 1);
    unsigned bars = get16(&r), tempos = get8(&r);
    if (!bars || bars > SONG_MAX_BARS || !tempos || tempos > SONG_TEMPOS) return -1;
    if (apply) {
        struct song_undo *undo = s->undo;
        song_clear(s, bars, 12000);
        s->undo = undo;
        memcpy(s->name, name, sizeof name);
        s->kit = (uint16_t)kit;
        memcpy(s->base_program, programs, sizeof programs);
        s->tempos = (uint8_t)tempos;
    }
    for (unsigned k = 0, previous = 0; k < tempos; ++k) {
        uint32_t clock = get32(&r);
        unsigned bpm = get16(&r);
        if (bpm < SONG_TEMPO_MIN || bpm > SONG_TEMPO_MAX || (k ? clock <= previous : clock != 0)) r.bad = 1;
        previous = clock;
        if (apply) s->tempo[k] = (struct song_tempo){clock, (uint16_t)bpm};
    }
    unsigned meters = get8(&r);
    if (!meters || meters > SONG_METERS) return -1;
    for (unsigned m = 0, previous = 0; m < meters; ++m) {
        unsigned bar = get16(&r), beats = get8(&r), unit = get8(&r);
        if (!beats || beats > 16 || (unit != 4 && unit != 8 && unit != 16) || (m ? bar <= previous : bar != 0)) r.bad = 1;
        previous = bar;
        if (apply) s->meter[m] = (struct song_meter){(uint16_t)bar, (uint8_t)beats, (uint8_t)unit};
    }
    unsigned markers = get8(&r);
    if (markers > SONG_MARKERS) return -1;
    for (unsigned i = 0, previous = 0; i < markers; ++i) {
        uint32_t clock = get32(&r);
        if (clock > SONG_MAX_CLOCK || clock < previous) r.bad = 1;
        previous = clock;
        if (apply) s->marker[i].clock = clock;
        copy(&r, apply ? s->marker[i].name : NULL, SONG_MARKER_NAME, apply);
    }
    uint32_t loop_start = get32(&r), loop_end = get32(&r);
    unsigned loop_on = get8(&r);
    uint8_t settings[10], knobs[SONG_TRACKS * 4];
    copy(&r, settings, sizeof settings, 1);
    copy(&r, knobs, sizeof knobs, 1);
    if (settings[0] > 50 || settings[3] > 11 || settings[4] >= SONG_SCALES || settings[5] > 4 || settings[6] > 2 ||
        settings[7] > 2 || settings[8] > 14 || settings[9] > 100 || loop_start > loop_end) r.bad = 1; /* files from outside */
    if (apply) {
        s->meters = (uint8_t)meters; s->markers = (uint8_t)markers;
        s->loop_start = loop_start; s->loop_end = loop_end; s->loop_on = (uint8_t)loop_on;
        s->swing = settings[0]; s->swing_grid = settings[1]; s->swing_tracks = settings[2];
        s->key_root = settings[3]; s->key_scale = settings[4]; s->rec_mode = settings[5]; s->count_in = settings[6];
        s->metronome = settings[7]; s->quantize = settings[8]; s->strength = settings[9];
        memcpy(s->knob_cc, knobs, sizeof knobs);
    }
    for (unsigned n = get8(&r), i = 0; i < n; ++i) {
        unsigned at = get8(&r);
        uint32_t word = get32(&r);
        if (!at || at > 31) r.bad = 1;
        else if (apply) s->glitch[at] = word;
    }
    copy(&r, apply ? s->humanize_time : NULL, SONG_TRACKS, apply);
    copy(&r, apply ? s->humanize_velocity : NULL, SONG_TRACKS, apply);
    uint32_t seed = get32(&r);
    if (apply) s->seed = seed;
    unsigned total = 0;
    for (unsigned t = 0; t < SONG_TRACKS && !r.bad; ++t) {
        unsigned count = get16(&r);
        uint32_t last = 0;
        if (total + count > SONG_EVENTS) return -1;
        if (apply) { s->first[t] = (uint16_t)total; s->count[t] = (uint16_t)count; }
        for (unsigned i = 0; i < count; ++i) {
            struct song_event e;
            e.at = get32(&r); e.a = (uint8_t)get8(&r); e.b = (uint8_t)get8(&r); e.length = (uint16_t)get16(&r);
            if (song_event_start(&e) < last || (song_is_note(&e) && (e.a > 127 || !e.length))) r.bad = 1;
            last = song_event_start(&e);
            if (apply) s->event[total + i] = e;
        }
        total += count;
    }
    if (r.bad || r.left) return -1;
    if (apply) {
        s->events = (uint16_t)total;
        song_tempo_refresh(s);
        ++s->revision;
    }
    return 0;
}
SONG_SMALL int song_file_check(const uint8_t *data, uint32_t bytes, const uint8_t **sounds, uint32_t *sounds_bytes)
{
    return data ? parse(NULL, data, bytes, 0, sounds, sounds_bytes) : -1;
}
SONG_SMALL int song_file_read(struct song *s, const uint8_t *data, uint32_t bytes, const uint8_t **sounds, uint32_t *sounds_bytes)
{
    if (song_file_check(data, bytes, sounds, sounds_bytes)) return -1;
    return parse(s, data, bytes, 1, sounds, sounds_bytes);
}

SONG_SMALL int song_recovery_due(struct song_recovery *r, uint32_t crc, uint32_t now, uint32_t gap)
{
    if (crc == r->written_crc || crc == r->saved_crc) { r->pending = 0; return 0; } /* nothing new */
    if (r->any && now - r->written_at < gap) { r->pending = 1; return 0; }      /* later, not now */
    r->pending = 0;
    return 1;
}
#endif
