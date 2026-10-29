/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* song-mode: the recovery copy and unsaved changes, included by
 * song_store.c. The song is checked against what was last saved or loaded
 * by its file's CRC, sounds included. */

/* The recovery copy: when playback stops and the song isn't what was last
 * written or saved, at most every 15 s. A stop too soon after leaves it
 * waiting, and the poll writes it once the time has passed. */
enum { RECOVERY_GAP = 15000 };
SONG_SMALL static void recover_maybe(uint32_t now)
{
    struct song *s = song_port_song();
    if (song_port_player()->playing || song_port_recorder()->active || importing) return;
    capture();
    uint32_t crc = song_file_crc(s, sounds, SOUNDS);
    if (!song_recovery_due(&recovery, crc, now, RECOVERY_GAP)) return;
    if (!write(POOL_RECOVERY, 1)) {
        recovery.written_crc = crc;
        recovery.written_at = now;
        recovery.any = 1;
    }
}
SONG_SMALL void song_store_poll(uint32_t now)
{
    unsigned playing = song_port_player()->playing;
    if (!baseline && !playing) { /* the song as it booted needs no copy */
        baseline = 1;
        capture();
        recovery.saved_crc = song_file_crc(song_port_song(), sounds, SOUNDS);
    }
    if (was_playing && !playing) recover_maybe(now);
    else if (recovery.pending && !playing && now - checked_at >= 1000U) { checked_at = now; recover_maybe(now); }
    was_playing = playing;
}
unsigned song_store_recovery(void) { return waiting; }
SONG_SMALL int song_store_restore(void)
{
    const struct pool_extent *e = pool_find(&pool, POOL_RECOVERY, 1);
    struct pool_header h;
    if (!e || pool_header(&pool, e, &h)) return -1;
    int r = load_from(POOL_RECOVERY, 1);
    if (!r) song_port_song()->id = (uint16_t)(pool_find(&pool, POOL_SONG, h.extra[0]) ? h.extra[0] : 0);
    waiting = 0;
    return r;
}
void song_store_skip_recovery(void) { waiting = 0; }
/* 1 when the song has changed since it was last saved or loaded, so loading
 * another or starting a new one would lose that. While playing, recording
 * or taking in a kit it can't look, and says 1. */
SONG_SMALL unsigned song_store_unsaved(void)
{
    if (song_port_player()->playing || song_port_recorder()->active || importing) return 1;
    capture();
    return song_file_crc(song_port_song(), sounds, SOUNDS) != recovery.saved_crc;
}
