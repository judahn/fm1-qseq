/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#ifndef FM1_SONG_RECORD_H
#define FM1_SONG_RECORD_H
#include "song.h"

/* Real-time recording (design section 5). Notes come in with the song clock
 * they were played at. A note goes into the song when its key comes up,
 * quantized as the song's settings say, and flagged as part of the take.
 * The take's raw timing is kept until the next pass or an undo, so
 * quantize can be changed or taken back afterwards.
 *   REPLACE  older notes on the track go quiet from where the take started,
 *            and the ones passed over are erased at stop.
 *   OVERDUB  everything is kept.
 *   PUNCH    as replace, but only between the loop points.
 *   LOOP     the loop repeats, every pass overdubs, and each pass starts a
 *            new set of raw timing.
 * A chord's notes (chord mode, design 8g) come with their lead, the clocks
 * after the chord's first note, so input quantize moves the chord as a
 * whole, by its first note, and a strum survives. STEP is step entry
 * (SHIFT+REC), which a song never keeps as its mode (0.4.0): a song's
 * rec_mode is one of the four live modes, OVERDUB to begin with. */
enum { SONG_REC_STEP, SONG_REC_REPLACE, SONG_REC_OVERDUB, SONG_REC_PUNCH, SONG_REC_LOOP, SONG_REC_MODES };
enum { SONG_TAKE = 512, SONG_REC_HELD = 16 };
struct song_take_note { uint32_t raw, at; uint16_t length; uint8_t pitch, velocity; }; /* raw: the clock, the lead above bit 24 */
struct song_recorder {
    struct song *song;
    struct song_take_note take[SONG_TAKE];
    struct { uint32_t at; uint8_t pitch, velocity, lead; } held[SONG_REC_HELD];
    uint32_t from, last, reach; /* start of the take, latest clock, furthest clock */
    uint16_t taken;
    uint8_t active, track, mode, held_count, passes;
};
void song_rec_start(struct song_recorder *r, struct song *s, unsigned track, unsigned mode, uint32_t from);
void song_rec_note(struct song_recorder *r, unsigned pitch, unsigned velocity, uint32_t clock);
void song_rec_lead(struct song_recorder *r, unsigned pitch, unsigned velocity, uint32_t clock, unsigned lead);
void song_rec_drop(struct song_recorder *r, unsigned pitch); /* a chord's note that never sounded */
void song_rec_follow(struct song_recorder *r, uint32_t clock); /* each poll: spots new loop passes */
void song_rec_stop(struct song_recorder *r, uint32_t clock);
/* Where the player should keep the track's older notes quiet. 0 if nowhere. */
int song_rec_quiet(const struct song_recorder *r, uint32_t *from, uint32_t *to);
uint32_t song_quantize(const struct song *s, uint32_t clock);
uint32_t song_quantize_grid(const struct song *s);    /* 0 when off */
int song_rec_requantize(struct song_recorder *r);     /* the last pass, from its raw timing */
#endif
