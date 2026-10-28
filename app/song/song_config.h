/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#ifndef FM1_SONG_CONFIG_H
#define FM1_SONG_CONFIG_H
/* Hardware test build: the diagnostics page (SHIFT+SEQ in song view) and
 * song playback on the device, through the delivery method picked there.
 * With 0, device playback stays off until a method is settled. */
#ifndef SONG_HW_TEST
#define SONG_HW_TEST 1
#endif
/* The song view, step entry and settings pages. The device's fixed flash
 * layout has room for the app image only up to 453,921 bytes, and stock V14
 * already uses 453,576, so the device build leaves these pages out until
 * that is settled. SEQ then opens the diagnostics page. The browser preview
 * has no such limit and keeps them. */
/* SONG_DEVICE_FULL (FM1_SONG_DEVICE_FULL=1 ./build.sh, never the default):
 * full song mode on the device, for a hardware test with Judah's OK. Left
 * out (option 1): (a) the stock step and pattern pages, the MIDI and sync
 * pages under SEQ staying, (b) the stock project pages, (c) the demo
 * projects and (d) the FM factory voices, which the V15 restore puts in the
 * DX7 library. The stock project area goes to code: the user partition
 * starts at logical sector 15 (the settings, then the staging sector at 16),
 * and the pool is sectors 17 to 125. Stock offsets stay logical, song_flash.h
 * moves them. */
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
#define SONG_USER_BASE (15U * 4096U)
#define SONG_POOL_FIRST 17
#define SONG_POOL_END 126
#define SONG_POOL_SKIP_FIRST 17
#define SONG_POOL_SKIP_END 17
#else
#define SONG_DROP_STEPS 0
#define SONG_DROP_PROJECTS 0
#define SONG_DROP_DEMOS 0
#define SONG_DROP_FM_FACTORY 0
#define SONG_USER_BASE 0U
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
 * past the end. With the project area given to code: first 17, end 126. */
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
#endif
