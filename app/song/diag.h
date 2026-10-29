/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#ifndef FM1_SONG_DIAG_H
#define FM1_SONG_DIAG_H
#include <stdint.h>

/* Hardware test measurements. Control loop intervals over a rolling window
 * and the whole run, how late each song note reached the engine, and free
 * memory. Control task only. */
enum { SONG_DIAG_WINDOW = 64, SONG_DIAG_LATE = 4 };
struct song_diag {
    uint32_t interval_us[SONG_DIAG_WINDOW];
    uint16_t interval_blocks[SONG_DIAG_WINDOW];
    uint32_t last_ticks, last_ms, last_blocks, polls, run_max_us, run_max_blocks;
    uint32_t check_ticks, check_ms, tick_khz; /* the tick clock against fm1_millis, per second */
    uint32_t late[SONG_DIAG_LATE], late_max, notes;
    uint32_t largest_free, engine_free[6];  /* largest free block, and with each engine on track 6 */
    uint32_t heap;                         /* the link map's heap, 0 where there is none (the browser) */
    int8_t heaviest;                       /* engine type that leaves least, -1 before the probe */
    uint8_t outside;                       /* malloc gave a block outside the heap: no result */
    uint8_t at, count, primed;
};
extern struct song_diag song_diag;
void song_diag_reset(void);
/* ticks: a 24 MHz counter, subtracted before converting so it may wrap. */
void song_diag_poll(uint32_t ticks, uint32_t now_ms, uint32_t blocks);
void song_diag_late(uint32_t blocks_now, uint32_t due_block);
/* Window summary: min, average and max, in microseconds and in blocks. */
void song_diag_window(uint32_t us[3], uint32_t blocks_x10[3]);
/* The memory probe: malloc by binary search, each block freed at once and
 * never written. It reaches SONG_DIAG_CEILING at most, and in the device
 * build no more than the heap, and a block only counts when it lies inside
 * the heap. A result within 16 bytes of the top reads as "or more". */
enum { SONG_DIAG_CEILING = 1U << 19 };
uint32_t song_diag_largest_free(void);
uint32_t song_diag_top(void); /* where the search stops */
int song_diag_engines(void);  /* try each engine on track 6, then put it back */
/* The full device build's free heap for the song list (0.4.0): the same
 * probe, 0 when malloc gave a block outside the heap (no result). Call it
 * stopped, with no audio-critical work going on, and not every frame. */
uint32_t song_heap_free(void);
#endif
