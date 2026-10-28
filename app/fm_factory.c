/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#include "fm_patch.h"
#include "song/song_config.h" /* song-mode */
#if !SONG_DROP_FM_FACTORY /* song-mode: SONG_DEVICE_FULL takes the voices from the DX7 library */
#include "fm_factory.inc"
#endif

/* Expand once during startup, before the audio owners are running. MIDI
 * program changes then read SRAM without decompression or heap allocation. */
#if SONG_DROP_FM_FACTORY /* song-mode: one blank voice */
static uint8_t factory_raw[128];
#define FACTORY_VOICES 1
#else
static uint8_t factory_raw[FM1_FM_PROGRAMS*128];
#define FACTORY_VOICES FM1_FM_PROGRAMS
#endif
static unsigned factory_ready;
static FM1_DSP_CODE void factory_init(void)
{
    if (factory_ready) return;
#if SONG_DROP_FM_FACTORY /* song-mode */
    factory_ready=1;
#else
    uint8_t *out=factory_raw;
    for (unsigned at=0;at<sizeof factory_packed;) {
        unsigned token=factory_packed[at++];
        if (token<128) *out++=(uint8_t)token;
        else {
            unsigned low=factory_packed[at++],distance=low&127U;
            if (low&128U) distance|=(unsigned)factory_packed[at++]<<7;
            for (unsigned n=token-128+3;n;--n) { *out=out[-(int)distance]; ++out; }
        }
    }
    factory_ready=1;
#endif /* song-mode */
}
FM1_DSP_CODE const uint8_t *groove_fm_factory_raw(unsigned program)
{
    factory_init();
    return factory_raw+(program%FACTORY_VOICES)*128; /* song-mode */
}
FM1_DSP_CODE void groove_fm_factory(uint8_t p[156], unsigned program)
{
    groove_fm_unpack(p,groove_fm_factory_raw(program));
}
