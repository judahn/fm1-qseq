/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#ifndef FM1_SONG_MANGLE_H
#define FM1_SONG_MANGLE_H
#include "song.h"

/* Mangle jobs on the selection (design section 8e): the notes in [from,
 * to) on the tracks in the mask. Destructive, journaled, so undo takes
 * them back. Automation stays where it is. Each returns how many notes
 * moved, or -1 when the selection is empty or too long. */
enum { SONG_MANGLE_SLICES = 1024 };
int song_scramble(struct song *s, unsigned tracks, uint32_t from, uint32_t to, uint32_t seed); /* 16th slices */
int song_reverse(struct song *s, unsigned tracks, uint32_t from, uint32_t to);   /* mirrored in time */
int song_shuffle_tracks(struct song *s, unsigned tracks, uint32_t from, uint32_t to, uint32_t seed);
#endif
