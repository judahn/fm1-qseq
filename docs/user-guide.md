# FM1 QY user guide

This covers the song mode as it is built, in the full device build (`FM1_SONG_DEVICE_FULL=1`) and in the
browser preview, which has all of it. The default device build has only a diagnostics page, described at the
end. Nothing here has run on hardware yet.

Names are the ones printed on the panel and shown on the screen. Where the screen and the stock manual use
different names, both are given.

## The panel

| Control | Name in this guide | In song mode |
|---|---|---|
| Buttons | FX | SHIFT on every song page |
| | SCL (the stock manual says SEL) | Markers, delete, open a clip, depending on the page |
| | ENV, LFO | Loop and metronome, picking, depending on the page |
| | EDIT | Sound editing, as in the stock firmware. SHIFT+EDIT opens the edit jobs |
| | GLO | Song settings. A hold is still the stock mixer |
| | HOME, SAVE | Leave to the stock overview, and save |
| | ARP | GLITCH: the stutter, and the glitch layer in step entry |
| | SEQ | The song view, and from there the clip grid |
| | PLAY, REC | Transport and recording |
| | OCT- / OCT+ | Move the keys an octave, two at most each way |
| Encoders | SELECT | The track |
| | ALGO (ALGORITHM) | Turns pages on pages that have them |
| | K1 to K4 | The four knobs above the screen. Their labels sit at the bottom of the screen |
| | PRESETS | The pitch knob in step entry, the lane in the automation view |
| Keys | F3 to G5 | 27 keys: 16 white, 11 black |

The white keys from left to right are F3 G3 A3 B3 C4 D4 E4 F4 G4 A4 B4 C5 D5 E5 F5 G5. The first eight, F3 to
F4, often stand for tracks 1 to 8.

## Getting around

The FM-1 starts on the stock overview. SEQ goes to the song view.

| Page | How to get there | How to leave |
|---|---|---|
| Song view | SEQ from any stock page, or from the grid | SEQ to the grid, HOME to the stock overview |
| Clip grid | SEQ in the song view | SEQ back to the song view |
| A clip in the editor | SCL on a grid cell | SEQ closes it and goes back to the grid |
| REC chooser | REC in the song view, stopped | REC or PLAY starts, HOME leaves |
| Step entry | REC, pick STEP with K1, then REC or PLAY | REC leaves, PLAY leaves and plays from the pointer's bar |
| Song settings | Tap GLO on a song page | Tap GLO again |
| Device settings | SHIFT+GLO on a song page (or HOME+GLO anywhere) | HOME, or SEQ |
| Automation view | SHIFT+LFO in the song view | SHIFT+LFO again |
| Edit jobs | SHIFT+EDIT in the song view | HOME or EDIT |
| Song list | SHIFT+SAVE on any page | HOME |
| Sound editing | EDIT | EDIT again returns to the song page |

These work on every song page:

| Buttons | Action |
|---|---|
| SAVE | Save the song to flash, in the song view, the automation view and the grid |
| SHIFT+SAVE | The song list |
| HOME+SAVE | Undo, up to 64 steps (fewer when steps are large) |
| SHIFT+HOME+SAVE | Redo |
| HOME+PLAY | Stop everything and go to the stock overview |
| SHIFT+SEQ | Every track back to the song (clips, see below) |
| SELECT | The track |
| OCT- / OCT+ | The keyboard's octave |

Each poll's edits are one undo step, so a knob turned over many moves undoes as one. A recording take is one
step. Flash writes happen only while stopped. A recovery copy of an unsaved song is written after playback
stops, at most every 15 seconds, and the song list offers it after a restart.

## Song view

Tracks are rows, bars run across. The loop region is shaded and is also the selection.

| Control | Action |
|---|---|
| PLAY | Play and stop |
| SHIFT+PLAY | Back to the loop start, or the song start when the loop is off |
| REC | Stopped: the REC chooser. Playing with a clip open: record into the clip (below) |
| K1 | Move the playhead one snap step. While playing, a jump waits for the next bar line |
| SHIFT+K1 | Marker to marker |
| K2 | Loop start |
| SHIFT+K2 | Select the marker section around the playhead. Turning on adds the next sections |
| K3 | Loop end |
| K4 | Zoom: 16, 8, 4 or 2 bars across. The snap follows: a bar, half a bar, a beat or an 8th |
| ENV | Loop on or off |
| SHIFT+ENV | Metronome: off, on, or only while recording |
| SCL | Add a marker at the playhead (M1, M2 and so on, 16 at most) |
| SHIFT+LFO | The automation view |
| SHIFT+EDIT | The edit jobs |
| Hold LFO, tap F3 to F4 | Add or remove tracks 1 to 8 from the selection |
| Hold LFO, tap G4 | All tracks, or back to just the current one |

The selection starts as the current track and follows SELECT until tracks are picked by hand.

SHIFT plus a key:

| Keys | Action |
|---|---|
| F3 | Copy the selection |
| G3 | Cut |
| A3 | Paste at the playhead, inserting (later bars move right) |
| B3 | Paste at the playhead, over what's there |
| C4 | Duplicate: paste right after the selection and move the selection onto the copy |
| D4 | Insert empty bars the length of the selection |
| E4 | Delete the selected bars (later bars move left) |
| F#3 | Mangle: scramble the 16ths in the selection |
| G#3 | Mangle: reverse the note order |
| A#3 | Mangle: shuffle notes between the selected tracks |
| F4 to G5 (white) | Jump to markers 1 to 9 |

One copied track pastes onto the current track. Several paste back onto their own tracks. The clipboard
survives loading another song. A clip copied in the grid pastes as a clip block instead (see Clips in the
song).

## Recording

REC in the song view, while stopped, opens the chooser.

| Control | Setting |
|---|---|
| SELECT | The track to record |
| K1 | Mode: STEP, REPLACE, OVERDUB, PUNCH (inside the loop only) or LOOP (each pass overdubs) |
| K2 | Count-in: off, 1 or 2 bars |
| K3 | Input quantize: off, or 1/64 to 1/1, each with a triplet |
| K4 | Quantize strength, 0 to 100% |
| REC or PLAY | Go: step entry for STEP, otherwise a take with its count-in |
| HOME | Leave |

During a take:

| Control | Action |
|---|---|
| PLAY or REC | End the take and stop |
| K1 to K4 | Record their parameters on the take track. Which parameter each knob drives is on settings page 3 |
| Hold ARP (GLITCH) | Stutter, recorded on the FX lane (see Glitch) |

Recorded notes are stored already quantized. The take keeps its raw timing, so turning K3 or K4 in the
chooser afterwards requantizes the last take. Undo takes the whole take back.

## Step entry

| Control | Action |
|---|---|
| Keys F3 to E5 | Enter a note at the pointer. Hold several to build a chord, which goes in when every key is up |
| F5 | Rest: move on one step |
| F#5 | Repeat the last entry |
| G5 | Tie: lengthen the last entry by the current length |
| SHIFT + F3 G3 A3 B3 C4 D4 E4 | Length: 1/64, 1/32, 1/16, 1/8, 1/4, 1/2, 1/1 |
| SHIFT+F#3 | Triplet on or off |
| SHIFT+G#3 | Dot, double dot, off |
| K1 | Length |
| K2 | Velocity, 1 to 127, named by the nearest of PPP to FFFF. Past the top: random R1 to R4 |
| K3 | Gate: 50, 90, 99 or 100% |
| K4 | Move the pointer one step. SHIFT+K4 jumps note to note |
| PRESETS | Move the selected notes by scale step, by octave with SHIFT. On drum tracks, from pad to pad |
| SCL | Delete the notes at the pointer |
| ENV | Back-delete: move back one step and delete there |
| Hold LFO, tap keys | Pick which notes at the pointer K1 to K3 and delete act on |
| ARP | The glitch layer (below), and ARP again to leave it |
| SELECT | The track |

When the pointer lands on notes they're selected and play, and K1 to K3 change them in place. On an empty spot
the knobs set the next entry.

The glitch layer gives each new or selected note glitch attributes. ALGO turns its three pages:

| Page | K1 | K2 | K3 |
|---|---|---|---|
| 1 | Repeats (x2 to x8 inside the note) | Drift per repeat (SHIFT+K2: by scale step) | Fade of the last repeat |
| 2 | Chance | Octave jump chance | Octave range |
| 3 | Accent | Flam | Pad swap (drum tracks) |

## Song settings

Tap GLO on a song page. ALGO turns the five pages. They're saved with the song.

| Page | K1 | K2 | K3 | K4 |
|---|---|---|---|---|
| 1 | Tempo (SHIFT: hundredths) | Time signature beats (SHIFT: the unit) | Swing, 0 to 50% | Swing grid, 8th or 16th |
| 2 | Key | Scale | Metronome: OFF, ON, REC | Launch quantize (clips) |
| 3 | The parameter K1 records | ...K2 records | ...K3 records | ...K4 records |
| 4 | Humanize timing, up to 30 clocks | Humanize velocity, up to 30 | | |
| 5 | Chord mode on or off | Voicing | Strum | Direction |

On pages 1 and 2 the keys F3 to F4 switch swing per track, and their lights show which tracks swing. Pages 3,
4 and 5 are for the track SELECT picks. Tempo and time signature here are the song's starting values.

Scales: chromatic (no snapping), major, minor, Dorian, Phrygian, Lydian, Mixolydian, Locrian, harmonic minor,
melodic minor, major pentatonic, minor pentatonic and blues. With a scale set, played keys outside it move up
to the next scale note on pitched tracks, the key lights show the scale, and the pitch knob in step entry
moves by scale step. Drum tracks ignore the scale.

The song's kit is chosen on the song list (K3).

## Device settings

SHIFT+GLO opens the stock Bluetooth and clock page. ALGO turns on to the stock pages that used to live on SEQ:
SYNC / LOOP, MIDI TRACK, MIDI ROUTING and TRACK ROLE, then the diagnostics page (end of this guide).

## Automation view

SHIFT+LFO in the song view. The conductor row sits above track 1. Turning SELECT down past track 1 reaches it.

| Control | Action |
|---|---|
| SELECT | The row: a track, or the conductor |
| PRESETS | The lane shown: one of the track engine's parameters, or programs. On the conductor: tempo, time signature or the FX lane |
| ALGO | Stopped: set a change at the playhead. Tempo (SHIFT: hundredths), the time signature from that bar, or a program change |
| SCL | Delete the change at the playhead |
| SHIFT+SCL | Clear the shown lanes (or the FX lane's stutters) in the selection |
| SHIFT+K1 | Move to the next or previous change on the row |

Everything else works as in the song view. Controller lanes are recorded with K1 to K4 during a take.
Tempo runs 20 to 300 BPM. Time signatures run 1/4 to 8/4, 1/8 to 16/8 and 1/16 to 16/16.

## Glitch

| Control | Action |
|---|---|
| Hold ARP (GLITCH) while playing | Stutter: every track repeats the last slice |
| While holding: F3 G3 A3 B3 C4 | Slice: 1/4, 1/8, 1/16, 1/32, 1/64 |
| While holding: F#3 | Triplet slice |
| While holding: K1 | Drift each repeat's pitch, up to 12 semitones |

Letting go rejoins the song where it has got to. A stutter played during a take goes on the FX lane and plays
back. Per-note glitch is in step entry. Humanize per track is on settings page 4. Swing is on page 1.

## Edit jobs

SHIFT+EDIT in the song view. K1 picks the job. K2 to K4 set its settings, ALGO turns to the next three, and
SHIFT moves a setting by ten. SAVE asks first, with how many events the job takes on, and SAVE again within
3 seconds runs it as one undo step. PLAY plays. Hold LFO and tap keys to pick tracks, as in the song view.
RANGE is the selection or the whole song.

Jobs: QUANTIZE, VELOCITY, CRESCENDO, GATE TIME, TRANSPOSE, FIT TO SCALE, SHIFT CLOCK, CHORD SORT, STRUM,
PASTE N TIMES, ERASE, EXTRACT, RAMP, THIN OUT, TIME STRETCH, COPY TRACK, MERGE TRACKS, CLEAR TRACK, NORMALIZE
and EXPAND CLIPS. With a clip open, the jobs that reach other tracks or the song's time are refused.

## Song list

SHIFT+SAVE. A memory gauge shows songs, kits and free space.

| Control | Action |
|---|---|
| K1 or SELECT | Pick a song |
| K2 | What a new song starts from |
| K3 | The kit. The pads switch at once and nothing is written |
| F3 | Load (press again to confirm, unsaved changes go) |
| G3 | Save (SAVE does the same) |
| A3 | Save as a new song |
| B3 | New song, not saved yet |
| C4 | Rename: K1 or SELECT picks the letter, K2 changes it, SAVE or C4 finishes |
| D4 | Duplicate |
| E4 | Delete (press again to confirm) |
| HOME | Back to the song view |

In the full build the stock factory sound sets are left out, so a new song starts blank. The workbench can
build a song from a factory sound set and upload it.

## Clip grid

SEQ in the song view. Eight tracks across, 16 rows in two pages of 8, and a SONG row on top. A clip is one
track's loop, a beat to 16 bars long. A track plays either the song or a clip, never both.

| Control | Action |
|---|---|
| SELECT | The track (column) |
| K1 | The row cursor, the SONG row first |
| K4 | Launch quantize: off, beat, bar, 2 bars or 4 bars |
| F3 to F4 | Launch that track's cell in the cursor row. An empty cell stops the track. On the SONG row the track goes back to the song |
| G4 to G5 | Launch rows 1 to 8 of the page on every track (a scene). Tracks with nothing in the row stop |
| SHIFT+SEQ | Every track back to the song |
| SCL | Open the clip at the cursor. An empty cell gets a new one-bar clip |
| ENV | Name the row: K1 picks the letter, K2 changes it, ENV or SAVE finishes |
| SHIFT + F3, G3, A3 or B3, E4 | Copy, cut, paste, paste, delete the cell |
| PLAY | Play and stop. Launching while stopped starts playback. Stopping gives every track back to the song |
| REC | Record a performance (below) |

Launches wait for the launch quantize, counted on the song's bar lines. The loop's wrap and the song's end
count as launch points too. A clip starts from its beginning and keeps its phase through song loops and
jumps. The track keys light while their clip plays and blink while a launch waits.

Pasting copied song bars into a cell turns them into a clip. Several copied tracks paste across the row.

### A clip in the editor

SCL on a cell opens the clip in the song view's editor, looped to its own length. K3, labelled CLIP END,
sets that length. Step entry, recording, automation, glitch and the jobs that stay inside the track all work
on the clip. SELECT stays on the clip's track, and the markers are hidden. SEQ closes it.

The song keeps playing while a clip is open, so a part can be written against the other tracks. REC while
playing records into the clip as loop recording from its next pass, and the rest plays on. REC again ends the
recording and playing goes on. PLAY stops both. Recording from a stop works as in the song view.

## Clips in the song

A block in a track's row plays a clip from the grid: "clip 3 from bar 9 to bar 17". Inside a block the clip
loops from the block's start and the track's own notes stay quiet. A stop block keeps the track silent. In
the song view blocks show as shaded bars with the clip's row number, and a stop shows an X. A block ends at
its length or where the next one on its track starts. Editing a clip changes it everywhere it's used.

**Recording a performance.** In the grid, REC arms it. The take starts with the next launch, or with PLAY.
Each launch writes a block for its track from the launch point to that track's next launch, stop or return
to the song. Tracks left on the song are untouched. The song's loop is ignored and the song grows to hold
the take. REC again or stopping ends it, and the whole take is one undo step.

**By hand.** Copy a cell in the grid (SHIFT+F3), go to the song view, and paste with SHIFT+A3 or SHIFT+B3. The
block lands at the playhead, the clip's length long, and becomes the selection. SHIFT+C4 (Duplicate) then
extends it. Copy, paste, insert and delete bars carry blocks as they carry notes. A grid copy only pastes
as a block into the song it came from.

**Expand.** The EXPAND CLIPS job turns the blocks starting in its range into plain notes on their tracks, so
one spot can go its own way. It refuses when the song can't hold the result. MIDI export in the workbench
expands every block.

## Chord mode

Chord mode is a setting per instrument track on settings page 5. It works on the song pages. The stock pages
play single notes as before, and drum tracks ignore it.

With it on, the keyboard splits:

| Keys | Action |
|---|---|
| F3 to E4 | Roots. Each plays a whole chord of the picked type. OCT- and OCT+ move them |
| F4 to E5 | Pick the type, which stays picked. Picking one while a root is held changes the sounding chord |
| F5 | Play the last chord again |
| F#5 | The last chord an octave down |
| G5 | The last chord an octave up |

The type keys:

| Key | Type | Key | Type |
|---|---|---|---|
| F4 | In key | F#4 | Sus2 |
| G4 | Major | G#4 | Sus4 |
| A4 | Minor | A#4 | Dim |
| B4 | 7 | C#5 | Aug |
| C5 | m7 | D#5 | Add9 |
| D5 | Maj7 | | |
| E5 | 6 | | |

In key builds each root's chord from the song's scale: the root and the second and fourth scale notes above
it. In C major, D gives D minor and C# gives C# dim. With no scale set it plays major.

Settings on page 5:

| Knob | Setting |
|---|---|
| K1 | On or off. A drum track shows DRUMS |
| K2 | Voicing. CLOSE is the shape on the root. SPREAD lifts its second note an octave. SMOOTH picks the inversion nearest the last chord |
| K3 | Strum, from the first note to the last: off, 1/64, 1/32, 1/16 or 1/8. It follows the tempo |
| K4 | Direction: up, down, alternating (up first) or random |

One chord sounds at a time, at velocity 100, while its key is held. A chord records as plain notes with its
strum. Input quantize moves a strummed chord as a whole, by its first note. In step entry a root enters its
chord at the pointer, strummed when strum is on, and the top three keys stay rest, repeat and tie.

## Diagnostics page

In the full build it's the last device settings page (SHIFT+GLO, then ALGO). In the default build SEQ opens it
directly, and SEQ again goes to the stock step page.

| Control | Action |
|---|---|
| PLAY | Play and stop the built-in test song (12 bars) |
| K1 | The note delivery method: A or B. With SHIFT, C, an experiment |
| SCL | Clear the numbers |
| ENV | Engine memory probe (stopped) |
| LFO | Largest free memory block |
| HOME | Back to the song view (full build) |

The page shows control loop timing, how late notes land for the method in use, audio health and memory. The
hardware test plan in `docs/hardware-test-plan.md` says what to write down.
