/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* song-mode: the emitter of the editing build, included by player.c: the
 * metronome, then every track's source (player_clips.inc), with glitch
 * applied note by note as it plays (player_glitch.inc). */

SONG_SMALL static void emit_tracks(struct song_player *p, uint32_t end)
{
    for (unsigned t = 0; t < SONG_TRACKS; ++t) emit_track(p, t, end); /* player_clips.inc */
}
SONG_SMALL static void emit(struct song_player *p, uint32_t end)
{
    clicks(p, end);
    emit_tracks(p, end);
    if (end > p->next) p->next = end;
}
