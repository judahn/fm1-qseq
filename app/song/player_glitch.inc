/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* song-mode: glitch in the player, included by player.c (design 8e).
 * Each note's attributes (glitch.h) apply as it plays: chance, pad swap or
 * octave jump, accent and humanize, a flam's grace hit a 64th early, and
 * repeats, which wait per track until their time comes, as swung notes
 * do. On a sampler track every hit tells the port its TUNE first (0.6.6),
 * untuned ones too, so a pad tuned for one note is back for the next.
 * Random choices hash the seed, the pass and the note, so a pass is the
 * same every time it is replayed and a loop rolls again each time. */

SONG_SMALL static uint32_t mix(uint32_t x)
{
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU;
    return x ^ x >> 16;
}
SONG_SMALL static uint32_t roll(const struct song_player *p, unsigned t, const struct song_event *e, unsigned salt)
{
    return mix(p->song->seed ^ mix(p->pass * 0x9e3779b9U ^ p->base[t] ^ song_event_start(e) ^ (uint32_t)e->a << 24 ^
                                   t << 20 ^ salt << 16)); /* base: a clip's every pass rolls again */
}
static int chance(uint32_t r, unsigned percent) { return r % 100U < percent; }

/* A sampler track's pad is tuned for the note about to start (0.6.6). */
SONG_SMALL static void tuned(const struct song_player *p, unsigned t, unsigned pitch, int semitones)
{
    if (p->samplers >> t & 1U && p->out.tune) p->out.tune(p->out.context, t, pitch, semitones);
}
/* Humanize moves a note both ways, and a flam starts with its grace hit. */
SONG_SMALL static uint32_t glitch_onset(const struct song_player *p, unsigned t, const struct song_event *e, uint32_t at)
{
    unsigned h = p->song->humanize_time[t];
    if (h) {
        int shift = (int)(roll(p, t, e, 1) % (2U * h + 1U)) - (int)h;
        at = shift < 0 && (uint32_t)-shift > at ? 0 : at + (uint32_t)shift;
    }
    if (song_glitch_field(song_glitch_word(p->song, e), GLITCH_FLAM))
        at = at > GLITCH_FLAM_CLOCKS ? at - GLITCH_FLAM_CLOCKS : 0;
    return at;
}
SONG_SMALL static uint32_t hit_time(const struct song_hit *h, unsigned k)
{
    return h->start + (uint32_t)(((uint64_t)k * h->length + h->repeats / 2U) / h->repeats);
}
/* Repeat k: drifted (pitched tracks), faded, and 3/4 of its share long. */
SONG_SMALL static void hit_play(struct song_player *p, unsigned t, struct song_hit *h)
{
    unsigned k = h->next++, n = h->repeats;
    int pitch = h->pitch;
    if (h->drift && !p->pad_count[t])
        pitch = h->scale ? (int)song_scale_step(p->song, h->pitch, h->drift * (int)k) : pitch + h->drift * (int)k;
    unsigned velocity = n > 1 ? h->velocity * (100U - (100U - h->fade) * k / (n - 1U)) / 100U : h->velocity;
    uint32_t length = n > 1 ? h->length * 3U / (4U * n) : h->length;
    const struct song_event e = {hit_time(h, k), (uint8_t)(pitch < 0 ? 0 : pitch > 127 ? 127 : pitch),
                                 (uint8_t)(velocity ? velocity : 1), (uint16_t)(length ? length : 1)};
    tuned(p, t, e.a, h->tune);
    note_on(p, t, &e, e.at);
}
SONG_SMALL static uint32_t next_hit(const struct song_player *p, unsigned t, unsigned *which)
{
    uint32_t at = NEVER;
    for (unsigned i = 0; i < p->hits[t]; ++i)
        if (hit_time(&p->hit[t][i], p->hit[t][i].next) < at) { at = hit_time(&p->hit[t][i], p->hit[t][i].next); *which = i; }
    return at;
}
SONG_SMALL static void hit_out(struct song_player *p, unsigned t, unsigned which)
{
    struct song_hit *h = &p->hit[t][which];
    hit_play(p, t, h);
    if (h->next >= h->repeats) *h = p->hit[t][--p->hits[t]];
}
SONG_SMALL static void glitch_note(struct song_player *p, unsigned t, const struct song_event *e, uint32_t at)
{
    uint32_t w = song_glitch_word(p->song, e);
    unsigned hv = p->song->humanize_velocity[t];
    if (!w && !hv) { tuned(p, t, e->a, 0); note_on(p, t, e, at); return; }
    if (!chance(roll(p, t, e, 2), song_glitch_percent(w, GLITCH_CHANCE))) return;
    int pitch = e->a, velocity = e->b;
    uint32_t r = roll(p, t, e, 3);
    if (p->pad_count[t]) {
        if (chance(r, song_glitch_percent(w, GLITCH_SWAP))) pitch = p->pads[t][(r >> 8) % p->pad_count[t]];
    } else if (chance(r, song_glitch_percent(w, GLITCH_OCTAVE))) {
        int jump = 12 * (1 + (int)(song_glitch_field(w, GLITCH_RANGE) & r >> 9));
        pitch += r >> 8 & 1U ? jump : -jump;
        if (pitch < 0 || pitch > 127) pitch = e->a + (r >> 8 & 1U ? -jump : jump);
        if (pitch < 0 || pitch > 127) pitch = e->a;
    }
    if (song_glitch_field(w, GLITCH_ACCENT)) velocity += GLITCH_ACCENT_BOOST;
    if (hv) velocity += (int)(roll(p, t, e, 4) % (2U * hv + 1U)) - (int)hv;
    struct song_hit h = {at, e->length, (uint8_t)pitch, (uint8_t)(velocity < 1 ? 1 : velocity > 127 ? 127 : velocity),
                         (uint8_t)song_glitch_repeats(w), 0, (uint8_t)song_glitch_percent(w, GLITCH_FADE),
                         (uint8_t)song_glitch_field(w, GLITCH_SCALE), (int8_t)song_glitch_drift(w),
                         (int8_t)song_glitch_tune(w)};
    if (song_glitch_field(w, GLITCH_FLAM)) { /* the grace hit now, the note a 64th later */
        const struct song_event grace = {at, h.pitch, (uint8_t)(h.velocity * 2U / 3U + 1U), GLITCH_FLAM_CLOCKS * 2U / 3U};
        tuned(p, t, grace.a, h.tune);
        note_on(p, t, &grace, at);
        h.start = at + GLITCH_FLAM_CLOCKS;
    } else hit_play(p, t, &h);
    if (h.next < h.repeats && p->hits[t] < SONG_HITS) p->hit[t][p->hits[t]++] = h;
}
