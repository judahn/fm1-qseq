/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* The install page's project library against the full device build: the
 * stock transfer commands (app/transfer.c) over the stock project slots
 * (app/project.c) and the DX7 library (app/fm_library.c), built as the
 * device builds them (SONG_DEVICE_FULL, SONG_DROP_PROJECTS). The slots'
 * sectors are code there (song_flash.h), so every slot answers empty, a
 * load answers empty and a save is refused as invalid, all without a flash
 * call. The working project still comes out, query 21 and 22 answer what
 * the page checks for, and a DX7 bank written as kind 4 lands in the
 * partition, reads back the same, and plays at once. The engine and the
 * rest of the device are stubs. Built by run_host.sh with
 * -DSONG_DEVICE_FULL=1 -DFM1_HOST_TEST. */
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "fm1.h"
#include "fm1_device.h"

static unsigned failures, checks;
#define CHECK(cond) do { ++checks; if (!(cond)) { ++failures; \
    printf("%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_EQ(a, b) do { long long x_ = (long long)(a), y_ = (long long)(b); ++checks; \
    if (x_ != y_) { ++failures; printf("%s:%d: %s = %lld, expected %lld\n", __FILE__, __LINE__, #a, x_, y_); } } while (0)

/* The SDK's user partition, 23 sectors short (the full build's), with every
 * call counted. song_flash.h maps the app's logical offsets onto these. */
enum { SECTOR = 4096, PARTITION = 108 * SECTOR };
static uint8_t partition[PARTITION];
static unsigned calls, outside;
static int in(uint32_t at, size_t n) { ++calls; if (at > PARTITION || n > PARTITION - at) { ++outside; return 0; } return 1; }
uint32_t fm1_flash_size(void) { return PARTITION; }
int fm1_flash_read(uint32_t at, void *data, size_t n) { if (!in(at, n)) return -1; memcpy(data, partition + at, n); return 0; }
int fm1_flash_write(uint32_t at, const void *data, size_t n)
{
    if (!in(at, n)) return -1;
    for (size_t i = 0; i < n; ++i) partition[at + i] &= ((const uint8_t *)data)[i];
    return 0;
}
int fm1_flash_erase(uint32_t at) { if (!in(at, SECTOR) || at % SECTOR) return -1; memset(partition + at, 0xff, SECTOR); return 0; }
const uint8_t *fm1_flash_map(uint32_t at, size_t n) { return in(at, n) ? partition + at : NULL; }

#include "../app/transfer.c" /* its command() is static */

/* The rest of the device and the engine. */
int fm1_ble_set_role(enum fm1_ble_role role) { (void)role; return 0; }
int fm1_bluetooth_configure(unsigned features) { (void)features; return 0; }
unsigned fm1_midi_input_ports(void) { return 7; }
unsigned fm1_midi_output_ports(void) { return 7; }
uint32_t fm1_millis(void) { return 0; }
void fm1_engine_pause(void) {}
void fm1_engine_resume(void) {}
void fm1_audio_stats_read(struct fm1_audio_stats *s) { memset(s, 0, sizeof *s); }
void fm1_control_request(void) {}
void fm1_engine_stats_read(struct fm1_engine_stats *s) { memset(s, 0, sizeof *s); }
unsigned fm1_midi_input_source(void) { return 0; }
int fm1_midi_send_port(unsigned port, const uint8_t *m, size_t n) { (void)port; (void)m; (void)n; return 0; }
int groove_arrangement_capture(struct groove_project *p) { memset(p, 0, sizeof *p); p->name[0] = 'W'; return 0; }
int groove_arrangement_tuning_apply(const struct groove_tuning t[GROOVE_TRACKS]) { (void)t; return 0; }
int groove_arrangement_tuning_capture(struct groove_tuning t[GROOVE_TRACKS]) { memset(t, 0, GROOVE_TRACKS * sizeof *t); return 0; }
int groove_tuning_capture(struct groove_tuning t[GROOVE_TRACKS]) { return groove_arrangement_tuning_capture(t); }
int groove_tuning_apply(const struct groove_tuning t[GROOVE_TRACKS]) { (void)t; return 0; }
void groove_clock_set(unsigned mode) { (void)mode; }
void groove_edit_checkpoint(unsigned operation) { (void)operation; }
int groove_factory_build(unsigned index, struct groove_project *p) { (void)index; (void)p; return -1; }
void groove_midi_config_apply(const struct groove_midi_config *c) { (void)c; }
void groove_midi_config_read(struct groove_midi_config *c) { memset(c, 0, sizeof *c); }
int groove_midi_config_valid(const struct groove_midi_config *c) { (void)c; return 1; }
void groove_midi_defaults(struct groove_midi_config *c) { memset(c, 0, sizeof *c); }
unsigned groove_midi_input_channel(unsigned track) { return track; }
int groove_midi_input_set(unsigned track, unsigned channel) { (void)track; (void)channel; return 0; }
unsigned groove_midi_setting(unsigned track, unsigned parameter) { (void)track; (void)parameter; return 0; }
int groove_project_apply(const struct groove_project *p) { (void)p; return 0; }
int groove_project_apply_edit(const struct groove_project *p, unsigned operation) { (void)p; (void)operation; return 0; }
void groove_project_arp_defaults(struct groove_project *p) { (void)p; }
void groove_project_clean(void) {}
void groove_project_touch(void) {}
int groove_project_valid(const struct groove_project *p) { (void)p; return 1; }
unsigned groove_track_role(unsigned track) { (void)track; return GROOVE_TRACK_NORMAL; }
int groove_track_role_set(unsigned track, unsigned role) { (void)track; (void)role; return 0; }
void groove_audio_stats(struct fm1_render_stats *s) { memset(s, 0, sizeof *s); }
unsigned groove_clock_mode(void) { return GROOVE_CLOCK_INTERNAL; }
unsigned groove_edit_recording(void) { return 0; }
int groove_fm_copy(unsigned track, uint8_t patch[156], struct groove_fm_controls *c) { (void)track; (void)c; memset(patch, 0, 156); return 0; }
int groove_fm_import(unsigned track, const uint8_t patch[156]) { (void)track; (void)patch; return 0; }
uint32_t groove_master_state(void) { return 0; }
int groove_midi_clock_accept(unsigned source, uint8_t status) { (void)source; (void)status; return 0; }
int groove_samples_begin(unsigned bytes) { (void)bytes; return 0; }
void groove_samples_cancel(void) {}
int groove_samples_finish(void) { return 0; }
int groove_samples_read(unsigned offset, uint8_t *data, unsigned bytes) { (void)offset; (void)data; (void)bytes; return 0; }
unsigned groove_samples_size(void) { return 0; }
unsigned groove_samples_status(void) { return 0; }
int groove_samples_write(unsigned offset, const uint8_t *data, unsigned bytes) { (void)offset; (void)data; (void)bytes; return 0; }
void groove_screen_mmc(unsigned command) { (void)command; }
unsigned groove_source_type(unsigned track) { (void)track; return GROOVE_DX7; }
int groove_timbre_export(unsigned track, uint8_t *data, unsigned capacity) { (void)track; (void)data; (void)capacity; return 0; }
int groove_timbre_import(unsigned track, const uint8_t *data, unsigned bytes) { (void)track; (void)data; (void)bytes; return 0; }
int song_transfer_command(unsigned cmd, const uint8_t *data, unsigned n, uint8_t *out, unsigned *count)
{ (void)cmd; (void)data; (void)n; (void)out; *count = 0; return GROOVE_PROJECT_INVALID; }

static uint8_t out[128];
static unsigned count;
/* A command as the transfer poll runs it: the status byte it would send. */
static unsigned ask(unsigned cmd, const uint8_t *data, unsigned n)
{
    int r = command(cmd, data, n, out, &count);
    return (unsigned)(r < 0 ? 64 - r : r);
}

/* Connect and refresh, as the page's library tab does them. */
static void test_slots(void)
{
    CHECK_EQ(ask(0, NULL, 0), 0);                            /* query 0 */
    CHECK(count == 14 && out[1] == 16 && out[2] < 16 && out[0] == 4);
    CHECK_EQ(ask(21, NULL, 0), 0);                           /* the page checks these exact values */
    CHECK(count == 7 && (out[1] | out[2] << 8) == 4264 && get32(out + 3) == 462848);
    CHECK_EQ(ask(22, NULL, 0), 0);
    CHECK(count == 9 && out[0] == 1 && out[1] == 128 && out[2] == 4 && get32(out + 4) == 446464);
    CHECK_EQ(out[3], 1);                                     /* the DX7 library is there */
    calls = 0;
    for (uint8_t slot = 0; slot < GROOVE_PROJECT_SLOTS; ++slot) {
        CHECK_EQ(ask(9, &slot, 1), 1);                       /* empty, which the page takes */
        CHECK(count == 12 && !memcmp(out, "EMPTY       ", 12));
    }
    uint8_t slot = 3, last = 15;
    CHECK_EQ(ask(8, &slot, 1), 1);                           /* load: that slot is empty */
    CHECK_EQ(ask(8, &last, 1), 1);
    CHECK_EQ(ask(7, &slot, 1), 71);                          /* save: refused, the project unchanged */
    CHECK_EQ(ask(7, &last, 1), 71);
    CHECK_EQ(calls, 0);                                      /* none of it touched flash */
    const uint8_t tuned[2] = {3, 0};                         /* download(0), with tuning */
    CHECK_EQ(ask(1, tuned, 2), 0);
    CHECK_EQ(out[0] | out[1] << 8, 4264);
    const uint8_t piece[3] = {0, 0, 64};
    CHECK_EQ(ask(2, piece, 3), 0);
    CHECK(count == 64 && out[0] == 0);
    CHECK_EQ(ask(6, NULL, 0), 0);
    CHECK_EQ(calls, 0);
}

/* A DX7 bank through kind 4: an unsaved bank reads as the one blank voice,
 * a written one lands in its sector of the partition, reads back the same,
 * and the program plays it with no restart. */
static void test_bank(void)
{
    static uint8_t raw[4096], back[4096];
    memset(partition, 0xff, sizeof partition);
    groove_fm_library_init();
    const uint8_t read2[2] = {4, 2};
    CHECK_EQ(ask(1, read2, 2), 0);
    CHECK_EQ(out[0] | out[1] << 8, 4096);
    for (unsigned at = 0; at < 4096; at += 64) {
        const uint8_t piece[3] = {(uint8_t)at, (uint8_t)(at >> 8), 64};
        CHECK_EQ(ask(2, piece, 3), 0);
        memcpy(back + at, out, 64);
    }
    ask(6, NULL, 0);
    unsigned blank = 0;
    for (unsigned i = 0; i < 4096; ++i) blank += back[i] != 0;
    CHECK_EQ(blank, 0);                                      /* the full build's fallback: 128 zero bytes a voice */
    for (unsigned i = 0; i < 4096; ++i) raw[i] = (uint8_t)((i * 7U + i / 128U) % 100U);
    calls = outside = 0;
    const uint8_t begin[4] = {4, 2, 0, 16};
    CHECK_EQ(ask(3, begin, 4), 0);
    for (unsigned at = 0; at < 4096; at += 64) {
        uint8_t chunk[66] = {(uint8_t)at, (uint8_t)(at >> 8)};
        memcpy(chunk + 2, raw + at, 64);
        CHECK_EQ(ask(4, chunk, 66), 0);
    }
    uint8_t crc[4];
    put32(crc, ~groove_crc32(~0U, raw, sizeof raw));
    CHECK_EQ(ask(5, crc, 4), 0);
    CHECK(calls > 0);
    CHECK_EQ(outside, 0);                                    /* every call inside the partition */
    CHECK_EQ(ask(1, read2, 2), 0);
    for (unsigned at = 0; at < 4096; at += 64) {
        const uint8_t piece[3] = {(uint8_t)at, (uint8_t)(at >> 8), 64};
        ask(2, piece, 3);
        memcpy(back + at, out, 64);
    }
    ask(6, NULL, 0);
    CHECK(!memcmp(back, raw, sizeof raw));
    int32_t at = song_flash_at(GROOVE_LIBRARY_OFFSET + 2U * SECTOR, SECTOR);
    CHECK(at > 0 && get32(partition + at) == 0x31424d46U);   /* the bank's own sector, logical 128 */
    uint8_t played[156], want[156];
    groove_fm_program(played, 64);                           /* the third bank's first voice, from the cache */
    groove_fm_unpack(want, raw);
    CHECK(!memcmp(played, want, sizeof want));
    groove_fm_library_init();                                /* and after a restart */
    groove_fm_program(played, 64);
    CHECK(!memcmp(played, want, sizeof want));
}

int main(void)
{
    test_slots();
    test_bank();
    printf("test_library_full: %u checks, %u failed\n", checks, failures);
    return failures != 0;
}
