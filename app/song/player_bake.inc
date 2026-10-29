/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* song-mode: Normalize's capture (phase 6), included by player.c. A private
 * player runs the song from the top one clock at a time, with no loop or
 * metronome, and every note it sends on the tracks asked for is
 * written down at the clock it went out, ending at the clock its note-off
 * went out. Played back as plain notes they land on the same blocks: a
 * block sends what is due before its end, and nothing on a plain track
 * waits behind anything else. */
struct bake { struct song_event *out; uint8_t *track; unsigned room, count, tracks, full; uint32_t clock; int16_t open[SONG_TRACKS][128]; };
SONG_SMALL static void baked(void *context, unsigned t, unsigned pitch, unsigned velocity)
{
    struct bake *b = context;
    if (!(b->tracks >> t & 1U)) return;
    if (velocity) {
        if (b->count == b->room) { b->full = 1; return; }
        b->open[t][pitch] = (int16_t)b->count;
        b->out[b->count] = (struct song_event){b->clock, (uint8_t)pitch, (uint8_t)velocity, 1};
        b->track[b->count++] = (uint8_t)t;
    } else if (b->open[t][pitch] >= 0) {
        struct song_event *e = &b->out[b->open[t][pitch]];
        uint32_t length = b->clock - song_event_start(e);
        e->length = (uint16_t)(length < 1 ? 1 : length > 0xffff ? 0xffff : length);
        b->open[t][pitch] = -1;
    }
}
SONG_SMALL int song_player_bake(const struct song_player *live, struct song *s, unsigned tracks, struct song_event *out,
                                uint8_t *track, unsigned room)
{
    static struct song_player p;
    static struct bake b;
    const struct song_output output = {baked, &b, NULL};
    memset(&b, 0, sizeof b);
    memset(b.open, 0xff, sizeof b.open);
    b.out = out; b.track = track; b.room = room; b.tracks = tracks;
    song_player_init(&p, s, &output);
    memcpy(p.pad_count, live->pad_count, sizeof p.pad_count);
    memcpy(p.pads, live->pads, sizeof p.pads);
    p.pass = live->pass; /* relocate counts it on, as the next start will */
    relocate(&p, 0, 0, 0);
    p.playing = 1;
    for (uint32_t c = 0, end = song_end(s); c < end && !b.full; ++c) {
        b.clock = c;
        emit_tracks(&p, c + 1U);
    }
    b.clock = song_end(s);
    all_off(&p);
    return b.full ? -1 : (int)b.count;
}
