/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Clips (phase 7a) in the player: launches landing exactly on the launch
 * quantize, a clip keeping its phase across a song loop and a jump, clean
 * note-offs as a track changes source, swing and glitch on clip notes, a
 * clip open in the editor, the song going round while clips play, and
 * playback following the view (0.4.0). The model and saving are in
 * test_clips.c. */
#include "harness.h"
#include "clips.h"
#include "glitch.h"

static struct song song;
static struct song_player player;

static void clip_note(unsigned cell, uint32_t clock, unsigned pitch, unsigned length)
{
    song_add_note(&song, song_cell_part(cell), clock, pitch, 100, length);
}
static uint32_t block_at(uint32_t clock) { return exact_block(&song, 0, 0, clock); }
/* Plays block by block until the song clock reaches `clock`. */
static uint32_t run_to(uint32_t block, uint32_t clock)
{
    while (song_player_clock(&player) < clock) run_blocks(&player, block++, 1);
    return block;
}
/* Note-ons of a pitch on a track, and whether each landed on `expect(k)`. */
static unsigned landed(unsigned track, unsigned pitch, uint32_t first, uint32_t every, unsigned *wrong)
{
    unsigned k = 0;
    for (unsigned i = 0; i < heard_count; ++i)
        if (heard[i].velocity && heard[i].track == track && heard[i].pitch == pitch)
            *wrong += heard[i].block != block_at(first + k++ * every);
    return k;
}

/* A launch at the bar quantize lands exactly on the bar line, at 133 BPM
 * where a bar is no whole number of frames. The song's note still sounding
 * there ends in that block, and the clip loops at its own length. */
static void test_launch_quantize(void)
{
    song_clear(&song, 16, 13300);
    for (unsigned beat = 0; beat < 64; ++beat) song_add_note(&song, 0, beat * 480U, 60, 100, 600);
    unsigned cell = song_cell(0, 2), wrong = 0;
    song.clip_length[cell] = 1920;
    clip_note(cell, 0, 72, 240);
    clip_note(cell, 960, 74, 240);
    song_player_init(&player, &song, &listener);
    reset_capture();
    song_player_start(&player, 0);
    uint32_t block = run_to(0, 2600);
    song_player_launch(&player, 0, 2);
    CHECK_EQ(player.launch_at[0], 3840);
    CHECK_EQ(player.waiting[0], 2);
    run_blocks(&player, block, 4000);
    CHECK_EQ(player.from[0], 2);
    CHECK_EQ(landed(0, 72, 3840, 1920, &wrong), (unsigned)((song_player_clock(&player) - 3840U) / 1920U + 1U));
    CHECK(landed(0, 74, 4800, 1920, &wrong) > 3);
    CHECK_EQ(landed(0, 60, 0, 480, &wrong), 8);              /* the song's notes before bar 3, none after */
    CHECK_EQ(wrong, 0);
    unsigned last = 0; /* the note from 3360 ends at the switch, not at 3960 */
    for (unsigned i = 0; i < heard_count; ++i)
        if (heard[i].track == 0 && heard[i].pitch == 60 && !heard[i].velocity) last = heard[i].block;
    CHECK_EQ(last, block_at(3840));
    /* Beat, two and four bars, and off: each from where playing is. */
    song.launch_q = 1; song_player_launch(&player, 0, 3);
    uint32_t at = player.next, beat = (at + 479U) / 480U * 480U;
    CHECK_EQ(player.launch_at[0], beat);
    song.launch_q = 3; song_player_launch(&player, 0, 3);
    CHECK_EQ(player.launch_at[0] % 3840U, 0);
    CHECK(player.launch_at[0] >= at && player.launch_at[0] < at + 3840U);
    song.launch_q = 4; song_player_launch(&player, 0, 3);
    CHECK(player.launch_at[0] % 7680U == 0 && player.launch_at[0] >= at && player.launch_at[0] < at + 7680U);
    song.launch_q = 0; song_player_launch(&player, 0, 3);
    CHECK_EQ(player.launch_at[0], at);
    song_player_launch(&player, 0, 2);                     /* the clip it plays: launched again */
    CHECK_EQ(player.waiting[0], 2);
    song_player_stop(&player);
    CHECK(player.from[0] == SONG_FROM_SONG && player.waiting[0] == SONG_FROM_NONE);
    CHECK(balanced());
}

/* A clip keeps its phase on the transport through song loops and a marker
 * jump, and launched while stopped it starts with playing. */
static void test_phase(void)
{
    song_clear(&song, 8, 12000);
    song_add_note(&song, 0, 0, 60, 100, 100);
    unsigned cell = song_cell(1, 0), wrong = 0;
    song.clip_length[cell] = 3840;
    clip_note(cell, 0, 50, 100);
    clip_note(cell, 1920, 52, 100);
    song_set_loop(&song, 0, 5760);
    song.loop_on = 1;
    song_player_init(&player, &song, &listener);
    reset_capture();
    song_player_launch(&player, 1, 0);
    song_player_start(&player, 0);
    run_blocks(&player, 0, 8400);                          /* 23,405 clocks: the 3-bar loop's fifth pass */
    CHECK_EQ(landed(1, 50, 0, 3840, &wrong), 7);
    CHECK_EQ(landed(1, 52, 1920, 3840, &wrong), 6);
    CHECK_EQ(landed(0, 60, 0, 5760, &wrong), 5);
    CHECK_EQ(wrong, 0);
    song_player_stop(&player);
    CHECK(balanced());
    /* A jump to bar 9 while playing: the clip's next pass still comes 3 bars after its last. */
    song_clear(&song, 16, 12000);
    song.clip_length[song_cell(2, 0)] = 5760;
    clip_note(song_cell(2, 0), 0, 45, 100);
    song_add_marker(&song, 8U * 1920U, "J");
    song_player_init(&player, &song, &listener);
    reset_capture();
    song_player_launch(&player, 2, 0);
    song_player_start(&player, 0);
    uint32_t block = run_to(0, 2400);
    song_player_jump(&player, 8U * 1920U);
    run_blocks(&player, block, 5000 - block);              /* 13,932 clocks, short of the song's end */
    CHECK(song_player_clock(&player) > 8U * 1920U);
    CHECK_EQ(song_player_transport(&player) - song_player_clock(&player), 3840U - 8U * 1920U); /* the shift */
    CHECK_EQ(landed(2, 45, 0, 5760, &wrong), 3);
    CHECK_EQ(wrong, 0);
    song_player_stop(&player);
    CHECK(balanced());
}

/* Song to clip, back to the song, then stopped: each change ends what the
 * track was sounding in the block it happens, and the song picks up where
 * it is. Swing and glitch apply to clip notes, timed from each pass. */
static void test_switching(void)
{
    song_clear(&song, 8, 12000);
    song_add_note(&song, 3, 0, 40, 100, 7680);
    song_add_note(&song, 3, 3840, 41, 100, 600);
    song_add_note(&song, 3, 5000, 42, 100, 2000);
    unsigned cell = song_cell(3, 1), wrong = 0;
    song.clip_length[cell] = 1920;
    clip_note(cell, 0, 70, 3000);                          /* rings past its pass */
    song_player_init(&player, &song, &listener);
    reset_capture();
    song_player_start(&player, 0);
    uint32_t block = run_to(0, 1000);
    song_player_launch(&player, 3, 1);
    block = run_to(block, 2500);
    song_player_launch(&player, 3, SONG_FROM_SONG);
    block = run_to(block, 4500);
    song_player_launch(&player, 3, SONG_FROM_STOP);
    run_blocks(&player, block, 2000);
    CHECK_EQ(player.from[3], SONG_FROM_STOP);
    unsigned offs[128] = {0};
    for (unsigned i = 0; i < heard_count; ++i) /* when each pitch last ended */
        if (heard[i].track == 3 && !heard[i].velocity) offs[heard[i].pitch] = heard[i].block + 1U;
    CHECK_EQ(offs[40], block_at(1920) + 1U);              /* the song's long note, at the launch */
    CHECK_EQ(offs[70], block_at(3840) + 1U);              /* the clip's, back to the song */
    CHECK_EQ(offs[42], block_at(5760) + 1U);              /* the song's, at the stop */
    CHECK_EQ(landed(3, 70, 1920, 1920, &wrong), 1);
    CHECK_EQ(landed(3, 41, 3840, 1920, &wrong), 1);       /* the song again, from where it is */
    CHECK_EQ(landed(3, 42, 5000, 1920, &wrong), 1);
    CHECK_EQ(wrong, 0);
    unsigned after = 0;
    for (unsigned i = 0; i < heard_count; ++i) after += heard[i].track == 3 && heard[i].velocity && heard[i].block > block_at(5760);
    CHECK_EQ(after, 0);
    song_player_stop(&player);
    CHECK(balanced());
    /* Swing: an 8th off-beat in the clip, 50% late, every pass. Glitch: two repeats. */
    song_clear(&song, 8, 12000);
    song.swing = 50;
    cell = song_cell(5, 0);
    song.clip_length[cell] = 1920;
    clip_note(cell, 240, 66, 100);
    clip_note(cell, 960, 67, 480);
    song.glitch[1] = song_glitch_turn(0, GLITCH_REPEATS, 1);
    song.event[song.first[song_cell_part(cell)] + 1].at |= 1U << SONG_GLITCH_SHIFT;
    song_player_init(&player, &song, &listener);
    reset_capture();
    song_player_launch(&player, 5, 0);
    song_player_start(&player, 0);
    run_blocks(&player, 0, 2800);                          /* four passes */
    CHECK_EQ(landed(5, 66, 360, 1920, &wrong), 4);
    unsigned k = 0;
    for (unsigned i = 0; i < heard_count; ++i)
        if (heard[i].velocity && heard[i].pitch == 67) wrong += heard[i].block != block_at(960 + k / 2U * 1920U + k % 2U * 240U), ++k;
    CHECK_EQ(k, 8);
    CHECK_EQ(wrong, 0);
    song_player_stop(&player);
    CHECK(balanced());
}

/* A clip open in the editor plays as its track's song, looped to the clip,
 * while other tracks' clips play on and the song's tracks wait. While clips
 * play the song goes round from the top at its end. */
static void test_editing_and_end(void)
{
    song_clear(&song, 8, 12000);
    for (unsigned beat = 0; beat < 32; ++beat) song_add_note(&song, 0, beat * 480U, 60, 100, 100);
    unsigned edited = song_cell(0, 4), wrong = 0;
    song.clip_length[song_cell(1, 0)] = 1920;
    clip_note(song_cell(1, 0), 0, 50, 100);
    song.clip_length[edited] = 960;
    clip_note(edited, 480, 72, 100);
    song_clip_open(&song, edited);
    song_player_init(&player, &song, &listener);
    reset_capture();
    song_player_launch(&player, 1, 0);
    song_player_start(&player, 0);
    run_blocks(&player, 0, 2700);                          /* four bars */
    CHECK_EQ(landed(0, 72, 480, 960, &wrong), 8);
    CHECK_EQ(landed(0, 60, 0, 480, &wrong), 0);
    CHECK_EQ(landed(1, 50, 0, 1920, &wrong), 4);
    CHECK_EQ(wrong, 0);
    song_player_stop(&player);
    song_clip_close(&song);
    CHECK(balanced());
    song_clear(&song, 2, 12000);
    song_add_note(&song, 0, 0, 60, 100, 100);
    song.clip_length[song_cell(1, 0)] = 5760;
    clip_note(song_cell(1, 0), 0, 50, 100);
    song_player_init(&player, &song, &listener);
    reset_capture();
    song_player_launch(&player, 1, 0);
    song_player_start(&player, 0);
    run_blocks(&player, 0, 3000);
    CHECK(player.playing);
    CHECK_EQ(landed(0, 60, 0, 3840, &wrong), 3);           /* the 2-bar song round again */
    CHECK_EQ(landed(1, 50, 0, 5760, &wrong), 2);
    CHECK_EQ(wrong, 0);
    song_player_launch(&player, 1, SONG_FROM_SONG);
    run_blocks(&player, 3000, 2000);
    CHECK(!player.playing);                                 /* no clips now: the end stops it */
    CHECK(balanced());
}

/* Playback follows the view (0.4.0). Stopped, at once: every track stopped
 * in the grid's view and on the song in the song view's, and stopping hands
 * every track to the view's. Playing the song, the grid's view stops every
 * track at the next bar line, except one whose clip's launch waits for it,
 * and silence follows on the rest while the song goes round at its end.
 * The song view's sends every track back to the song at the next bar line,
 * where the song picks up, and with no clip left it stops at its end. */
static void test_view(void)
{
    song_clear(&song, 2, 12000);
    for (unsigned t = 0; t < 3; ++t)
        for (unsigned beat = 0; beat < 8; ++beat) song_add_note(&song, t, beat * 480U, 40 + t, 100, 100);
    unsigned cell = song_cell(1, 0), wrong = 0;
    song.clip_length[cell] = 1920;
    clip_note(cell, 0, 70, 100);
    song_player_init(&player, &song, &listener);
    CHECK_EQ(player.home, SONG_FROM_SONG);
    song_player_view(&player, 1);
    CHECK(player.home == SONG_FROM_STOP && player.from[0] == SONG_FROM_STOP && player.from[7] == SONG_FROM_STOP);
    song_player_start(&player, 0);
    song_player_stop(&player);
    CHECK(player.from[0] == SONG_FROM_STOP && player.from[7] == SONG_FROM_STOP);
    song_player_view(&player, 0);
    CHECK(player.home == SONG_FROM_SONG && player.from[0] == SONG_FROM_SONG && player.waiting[0] == SONG_FROM_NONE);
    /* The song plays, then the grid's view at 1000, track 2's clip launched just before. */
    reset_capture();
    song_player_start(&player, 0);
    uint32_t block = run_to(0, 1000);
    song_player_launch(&player, 1, 0);
    song_player_view(&player, 1);
    CHECK(player.waiting[0] == SONG_FROM_STOP && player.waiting[1] == 0 && player.waiting[2] == SONG_FROM_STOP);
    CHECK(player.launch_at[0] == 1920 && player.launch_at[2] == 1920);
    run_blocks(&player, block, block_at(5000) - block);    /* past the song's end at 3840 */
    block = block_at(5000);
    CHECK(player.playing);                                  /* the song went round */
    CHECK(player.from[0] == SONG_FROM_STOP && player.from[1] == 0 && player.from[2] == SONG_FROM_STOP);
    unsigned after = 0;
    for (unsigned i = 0; i < heard_count; ++i) after += heard[i].velocity && heard[i].track != 1 && heard[i].block >= block_at(1920);
    CHECK_EQ(after, 0);
    CHECK_EQ(landed(0, 40, 0, 480, &wrong), 4);            /* the song's first bar, then nothing */
    CHECK_EQ(landed(1, 70, 1920, 1920, &wrong), 2);        /* the clip at 1920 and 3840, on the transport */
    CHECK_EQ(wrong, 0);
    /* Back to the song view's: every track waits for the next bar line, the
     * song's 1920 at transport 5760. */
    reset_capture();
    song_player_view(&player, 0);
    CHECK(player.waiting[0] == SONG_FROM_SONG && player.waiting[1] == SONG_FROM_SONG && player.waiting[7] == SONG_FROM_SONG);
    CHECK_EQ(player.launch_at[0], 5760);
    run_blocks(&player, block, block_at(9000) - block);
    CHECK(player.from[0] == SONG_FROM_SONG && player.from[1] == SONG_FROM_SONG);
    CHECK_EQ(landed(0, 40, 5760, 480, &wrong), 4);         /* the song's second bar, from the bar line */
    CHECK_EQ(landed(2, 42, 5760, 480, &wrong), 4);
    CHECK_EQ(landed(1, 70, 0, 1920, &wrong), 0);           /* the clip gone */
    CHECK_EQ(wrong, 0);
    CHECK(!player.playing);                                 /* no clip left: the song's end stops it */
    CHECK(player.from[0] == SONG_FROM_SONG);
    CHECK(balanced());
}

int main(void)
{
    test_launch_quantize();
    test_phase();
    test_switching();
    test_editing_and_end();
    test_view();
    return report("test_clips_play");
}
