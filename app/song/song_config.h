/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#ifndef FM1_SONG_CONFIG_H
#define FM1_SONG_CONFIG_H
/* The song view, step entry and settings pages. The device's fixed flash
 * layout has room for the app image only up to 453,921 bytes, and stock V14
 * already uses 453,576, so the default device build leaves these pages out.
 * SEQ then opens the diagnostics page. The browser preview has no such
 * limit and keeps them, as SONG_DEVICE_FULL does. */
/* SONG_DEVICE_FULL (FM1_SONG_DEVICE_FULL=1 ./build.sh, never the default):
 * full song mode on the device, for a hardware test with Judah's OK. Left
 * out (option 1): (a) the stock step and pattern pages, the MIDI and sync
 * pages staying in the device settings (SHIFT+GLO), (b) the stock project
 * pages, (c) the demo projects and (d) the FM factory voices, which the V15
 * restore puts in the DX7 library. Code takes 23 sectors of the user
 * partition: the stock project area (logical sectors 0 to 14) and the first
 * 8 of the sample area (17 to 24). The partition holds logical 15 and 16
 * (the settings and the staging sector), then 25 to 130: the pool is 25 to
 * 125, then the DX7 banks and the tuning sector. Stock offsets stay
 * logical, and song_flash.h maps them in those two pieces. With no stock
 * sample bank (it started at 17), the checks for one reaching the tuning
 * sector find none (SONG_DROP_SAMPLE_BANK). */
#ifndef SONG_DEVICE_FULL
#define SONG_DEVICE_FULL 0
#endif
#if SONG_DEVICE_FULL && !defined(FM1_OPEN_WEB)
#define SONG_UI 1
#define SONG_TRIM 0
#define SONG_DROP_STEPS 1
#define SONG_DROP_PROJECTS 1
#define SONG_DROP_DEMOS 1
#define SONG_DROP_FM_FACTORY 1
#define SONG_DROP_SAMPLE_BANK 1
#define SONG_FLASH_LOW 15U  /* logical 15 and 16 start the partition */
#define SONG_FLASH_GAP 17U  /* 17 to 24 are code */
#define SONG_FLASH_HIGH 25U /* 25 on follow right after */
#define SONG_POOL_FIRST 25
#define SONG_POOL_END 126
#define SONG_POOL_SKIP_FIRST 25
#define SONG_POOL_SKIP_END 25
#else
#define SONG_DROP_STEPS 0
#define SONG_DROP_PROJECTS 0
#define SONG_DROP_DEMOS 0
#define SONG_DROP_FM_FACTORY 0
#define SONG_DROP_SAMPLE_BANK 0
#endif
/* The hardware test build (the default device build): the diagnostics page,
 * where SEQ goes, with a choice of delivery method A or B for the timing
 * test (song_deliver.c). The full build and the browser preview deliver by
 * B and have no diagnostics page. build.sh --test-preview builds the
 * preview this way too (SONG_UI 0, SONG_HW_TEST 1). */
#ifndef SONG_HW_TEST
#if SONG_DEVICE_FULL || defined(FM1_OPEN_WEB)
#define SONG_HW_TEST 0
#else
#define SONG_HW_TEST 1
#endif
#endif
/* Room for the hardware test on the device: the stock factory projects
 * (presets.c) and workbench project, sample and voice transfers
 * (transfer.c) are left out of this build. Firmware updates and the V15
 * restore use the SDK's own update protocol and are not affected. */
#if SONG_HW_TEST && !defined(FM1_OPEN_WEB) && !defined(SONG_TRIM)
#define SONG_TRIM 1
#endif
/* The storage pool (pool.h): user-partition sectors [SONG_POOL_FIRST,
 * SONG_POOL_END), less [SONG_POOL_SKIP_FIRST, SONG_POOL_SKIP_END). Today
 * that is the stock project area and the sample area, sectors 0 to 125,
 * 496 KB. Sectors 15 and 16 stay with the stock code: the device settings,
 * and the staging sector the DX7 banks and the tuning journal go through.
 * The DX7 banks (126 to 129) and the tuning and session sector (130) are
 * past the end. SONG_DEVICE_FULL's pool, past what code took: 25 to 125. */
#ifndef SONG_POOL_FIRST
#define SONG_POOL_FIRST 0
#define SONG_POOL_END 126
#define SONG_POOL_SKIP_FIRST 15
#define SONG_POOL_SKIP_END 17
#endif
#ifndef SONG_UI
#ifdef FM1_OPEN_WEB
#define SONG_UI 1
#else
#define SONG_UI 0
#endif
#endif
#if !SONG_UI && !SONG_HW_TEST
#error "Without the song pages SEQ opens the diagnostics page: SONG_UI 0 needs SONG_HW_TEST 1"
#endif
#endif
