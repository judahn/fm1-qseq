# FM1 QY user guide

This covers the song mode as it is built, in the full device build (`FM1_SONG_DEVICE_FULL=1`) and in the
browser preview, which has all of it. The default device build is the hardware test build: it has only a
diagnostics page, described at the end. Song playback timing has been measured on the FM-1 with that build.
The full build has run on the FM-1 since 0.1.0.

Names are the ones printed on the panel and shown on the screen.

Every button light is on, all the time, on every page, so the printed labels can be read. The lights don't
show which page or mode is active. The key lights work as described below.

## The panel

| Control | Name in this guide | In song mode |
|---|---|---|
| Buttons | FX | SHIFT on every song page while held. A tap opens the effects page from the song view, the grid, the automation view and the settings |
| | SEL | Markers, delete, open a clip, depending on the page |
| | ENV, LFO | Loop and metronome, picking, depending on the page |
| | EDIT | Sound editing, as in the stock firmware. SHIFT+EDIT opens the edit jobs |
| | GLO | Song settings. A hold is still the stock mixer |
| | HOME, SAVE | Leave to the stock overview, and the song list |
| | ARP | The arpeggiator, as on the stock pages: a tap turns it on or off, a hold opens its settings. In step entry, the glitch layer |
| | SEQ | The song view, and from there the clip grid |
| | PLAY, REC | Transport and recording |
| | OCT- / OCT+ | Move the keys an octave, two at most each way. On the song list, the edit jobs and while naming they are NO and YES |
| Encoders | SELECT | The track |
| | ALGO (ALGORITHM) | Turns pages on pages that have them |
| | K1 to K4 | The four knobs above the screen. Their labels sit at the bottom of the screen |
| | PRESETS | The pitch knob in step entry, the lane in the automation view |
| Keys | F3 to G5 | 27 keys: 16 white, 11 black |

The white keys from left to right are F3 G3 A3 B3 C4 D4 E4 F4 G4 A4 B4 C5 D5 E5 F5 G5. The first eight, F3 to
F4, often stand for tracks 1 to 8.

## Getting around

The FM-1 starts on the stock overview. SEQ goes to the song view.

**YES and NO.** On the song list, the edit jobs and while naming a grid row, OCT+ is YES and OCT- is NO, as
on the QY70's +1/YES and -1/NO keys. A question shows as a box over the page with `OCT+ YES   OCT- NO` under
it. It stays until YES, NO, or a change that cancels it (another song, action or setting, or leaving the
page). It never times out. With nothing open, NO goes back to the song view. In the song view and step entry
OCT- and OCT+ still move the keyboard's octave.

| Page | How to get there | How to leave |
|---|---|---|
| Song view | SEQ from any stock page, or from the grid | SEQ to the grid, HOME to the stock overview |
| Clip grid | SEQ in the song view | SEQ back to the song view |
| A clip in the editor | SEL on a grid cell | SEQ closes it and goes back to the grid |
| REC chooser | REC in the song view, stopped | REC or PLAY starts, HOME leaves |
| Step entry | REC, pick STEP with K1, then REC or PLAY | REC leaves, PLAY leaves and plays from the pointer's bar |
| Song settings | Tap GLO on a song page | Tap GLO again |
| Device settings | SHIFT+GLO on a song page (or HOME+GLO anywhere) | HOME, or SEQ |
| Automation view | SHIFT+LFO in the song view | SHIFT+LFO again |
| Edit jobs | SHIFT+EDIT in the song view | HOME, EDIT or NO (OCT-) |
| Song list | SAVE on any page | HOME or NO (OCT-) |
| Sound editing | EDIT | EDIT again returns to the song page |
| Effects | Tap FX on the song view, the grid, the automation view or the settings | FX goes back, SEQ goes to the song view |
| Arpeggiator settings | Hold ARP on those pages | HOME goes back |

These work on every song page:

| Buttons | Action |
|---|---|
| SAVE | The song list, which opens as SAVE comes up, from the song view, the automation view, the grid, the edit jobs and the stock pages. There SAVE saves the song, so a save is SAVE, SAVE. Not while a question or a naming is open, or while recording |
| SHIFT+SAVE | The same |
| HOME+SAVE | Undo, up to 64 steps (fewer when steps are large). HOME or SAVE can go down first, and the song list doesn't open |
| SHIFT+HOME+SAVE | Redo |
| HOME+PLAY | Stop everything and go to the stock overview |
| SHIFT+SEQ | Every track back to the song (clips, see below) |
| SELECT | The track |
| OCT- / OCT+ | The keyboard's octave. NO and YES on the song list, the edit jobs and while naming |

Each poll's edits are one undo step, so a knob turned over many moves undoes as one. A recording take is one
step, and so is a take into a cell of the grid, with the clip it made. Flash writes happen only while stopped. A recovery copy of an unsaved song is written after playback
stops, at most every 15 seconds, and the song list offers it after a restart.

## Song view

Tracks are rows, bars run across. The loop region is shaded and is also the selection.

| Control | Action |
|---|---|
| PLAY | Play and stop. During a take, end the recording and play on |
| SHIFT+PLAY | Back to the loop start, or the song start when the loop is off |
| REC | Stopped: the REC chooser. Playing with a clip open: record into the clip (below). During a take, end the recording and play on |
| K1 | Move the playhead one snap step. While playing, a jump waits for the next bar line |
| SHIFT+K1 | Marker to marker |
| K2 | Loop start |
| SHIFT+K2 | Select the marker section around the playhead. Turning on adds the next sections |
| K3 | Loop end |
| K4 | Zoom: 16, 8, 4 or 2 bars across. The snap follows: a bar, half a bar, a beat or an 8th |
| ENV | Loop on or off |
| SHIFT+ENV | Metronome: off, on, or only while recording |
| SEL | Stopped: add a marker at the playhead, or remove the one already there. Playing: add one at the nearest snap step |
| SHIFT+LFO | The automation view |
| SHIFT+EDIT | The edit jobs |
| Hold LFO, tap F3 to F4 | Add or remove tracks 1 to 8 from the selection |
| Hold LFO, tap G4 | All tracks, or back to just the current one |

New markers are named M1, M2 and so on, taking the first free number, 16 at most. To remove one, go to it
with SHIFT+K1, which lands on it exactly, and press SEL. Removing only happens while stopped. HOME+SAVE brings
a removed marker back.

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
| PLAY or REC | End the take. Playing goes on, and PLAY again stops. In the count-in, before anything is recorded, they call the take off and stop |
| K1 to K4 | Record their parameters on the take track. Which parameter each knob drives is on settings page 3 |

Recorded notes are stored already quantized. The take keeps its raw timing, so turning K3 or K4 in the
chooser afterwards (stopped) requantizes the last take. Undo takes the whole take back. If the song's memory
fills during a take, the take ends there and says so.

## Step entry

| Control | Action |
|---|---|
| Keys F3 to G5 | Enter a note at the pointer. Hold several to build a chord, which goes in when every key is up |
| SHIFT + F G A B C D E | Length: 1/1, 1/2, 1/4, 1/8, 1/16, 1/32, 1/64 |
| SHIFT+F# | Triplet on or off |
| SHIFT+G# | Dot, double dot, off |
| SHIFT+C# | Rest: move on one step |
| SHIFT+D# | Tie: lengthen the last entry by the current length |
| SHIFT+A# | Repeat the last entry |
| K1 | Length |
| K2 | Velocity, 1 to 127, named by the nearest of PPP to FFFF. Past the top: random R1 to R4 |
| K3 | Gate: 50, 90, 99 or 100% |
| K4 | Move the pointer one step. SHIFT+K4 jumps note to note |
| PRESETS | Move the selected notes by scale step, by octave with SHIFT. On drum tracks, from pad to pad |
| SEL | Delete the notes at the pointer |
| ENV | Back-delete: move back one step and delete there |
| Hold LFO, tap keys | Pick which notes at the pointer K1 to K3 and delete act on |
| ARP | The glitch layer (below), and ARP again to leave it |
| SELECT | The track |

With SHIFT held a key works by its note name, in any octave, so SHIFT+F3, SHIFT+F4 and SHIFT+F5 all pick 1/1. It
enters no note.

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

The stock instrument keeps a tempo of its own, in whole BPM from 30 to 300, for the arpeggiator, the synced
delay, the stock pages' tempo and MIDI clock out. It follows the song's tempo where the playhead is, rounded: as
playing crosses a tempo change, after a jump, and whenever the song's tempo changes. A tempo set on a stock page
(the effects page's delay, the clock settings) or by tap tempo goes into the song instead: the starting tempo,
or the tempo change the playhead is past. With MIDI clock coming in, neither changes.

Scales: chromatic (no snapping), major, minor, Dorian, Phrygian, Lydian, Mixolydian, Locrian, harmonic minor,
melodic minor, major pentatonic, minor pentatonic and blues. With a scale set, played keys outside it move up
to the next scale note on pitched tracks, the key lights show the scale, and the pitch knob in step entry
moves by scale step. Drum tracks ignore the scale.

The song's kit is chosen on the song list (K3).

## Device settings

SHIFT+GLO opens the stock Bluetooth and clock page. ALGO turns on to the stock pages that used to live on SEQ:
SYNC / LOOP, MIDI TRACK, MIDI ROUTING and TRACK ROLE.

## Automation view

SHIFT+LFO in the song view. The conductor row sits above track 1. Turning SELECT down past track 1 reaches it.

| Control | Action |
|---|---|
| SELECT | The row: a track, or the conductor |
| PRESETS | The lane shown: one of the track engine's parameters, or programs. On the conductor: tempo or time signature |
| ALGO | Stopped: set a change at the playhead. Tempo (SHIFT: hundredths), the time signature from that bar, or a program change |
| SEL | Delete the change at the playhead |
| SHIFT+SEL | Clear the shown lanes in the selection |
| SHIFT+K1 | Move to the next or previous change on the row |

Everything else works as in the song view. Controller lanes are recorded with K1 to K4 during a take.
Tempo runs 20 to 300 BPM. Time signatures run 1/4 to 8/4, 1/8 to 16/8 and 1/16 to 16/16.

## Glitch

Per-note glitch is in step entry: ARP there turns on the glitch layer (see Step entry). Mangle is SHIFT+F#3,
G#3 and A#3 in the song view: scramble the 16ths, reverse the note order, or shuffle notes between the selected
tracks. Humanize per track is on settings page 4. Swing is on page 1.

The live stutter and the FX lane are gone since 0.3.0. A song saved with FX lane events still loads, without
them.

## Arpeggiator and effects

On the song view, the grid, the automation view and the settings, ARP and FX work as on the stock pages:

| Control | Action |
|---|---|
| Tap ARP | The arpeggiator on or off for the track |
| Hold ARP | Its settings page. HOME comes back to the song page |
| Tap FX | The effects page: the track's sends, the shared effects, and the delay, which PRESETS turns between. FX comes back, SEQ goes to the song view. On the delay page SEQ taps the tempo, as on the stock pages |
| Hold FX | SHIFT, as everywhere on the song pages |

A tap is a press and release within 600 ms with no other button, key or knob used meanwhile. In step entry,
the song list, the edit jobs and the REC chooser FX is only SHIFT, and ARP opens no settings.

## Edit jobs

SHIFT+EDIT in the song view. K1 picks the job. K2 to K4 set its settings, ALGO turns to the next three, and
SHIFT moves a setting by ten. YES (OCT+) asks first, with how many events the job takes on
(`QUANTIZE 120 EVENTS?`), and says when it is too big to undo. YES again runs it as one undo step. NO cancels
the question, and so does changing the job, a setting, the track or the picked tracks. With no question open
NO goes back to the song view, as HOME and EDIT do. SAVE opens the song list. PLAY plays. Hold LFO and tap keys to
pick tracks, as in the song view. RANGE is the selection or the whole song.

Jobs: QUANTIZE, VELOCITY, CRESCENDO, GATE TIME, TRANSPOSE, FIT TO SCALE, SHIFT CLOCK, CHORD SORT, STRUM,
PASTE N TIMES, ERASE, EXTRACT, RAMP, THIN OUT, TIME STRETCH, COPY TRACK, MERGE TRACKS, CLEAR TRACK, NORMALIZE
and EXPAND CLIPS. With a clip open, the jobs that reach other tracks or the song's time are refused.

## Song list

SAVE on any page, as SAVE comes up. A memory gauge shows songs, kits and free space, and the header shows the
firmware's version, as the boot screen does. Under it are the battery's reading from its sensor, for example
`BAT 580`, smoothed and updated at most once a second, and a bar. The bar is full at 622, a full battery
read on a device with USB unplugged, and empty at 505, an estimate. K2 picks an action and YES does it. The line at the bottom always says what YES will do and to
what, for example `OCT+ LOAD VERSE   OCT- BACK`.

| Control | Action |
|---|---|
| K1 or SELECT | Pick a song |
| K2 | The action: LOAD, SAVE, SAVE AS (a new song), NEW, RENAME, DUPLICATE or DELETE. It starts on LOAD and keeps the last pick while the FM-1 is on |
| K3 | The kit. The pads switch at once and nothing is written |
| K4 | NEW FROM: what a new song starts from |
| YES (OCT+) | Do the action |
| NO (OCT-) | Cancel a question or a naming. With nothing open, back to the song view |
| SAVE | Save the song being worked on, so a save anywhere is SAVE, SAVE. Not while a question or a naming is open |
| HOME | Back to the song view |

What YES does:

| Action | YES |
|---|---|
| LOAD | Loads the picked song. With unsaved changes it asks first (`LOAD VERSE? UNSAVED CHANGES GO`) |
| SAVE | Saves the song being worked on |
| SAVE AS | Saves it as a new song. A song still named with a number gets the next free one |
| NEW | Starts a new song from K4's pick, not saved yet, named with the lowest number no saved song uses (1, 2, 3 and so on), so renaming it is quick. With unsaved changes it asks first |
| RENAME | Names the picked song: K1 or SELECT picks the letter, K2 changes it, YES keeps the name, NO leaves the old one |
| DUPLICATE | Copies the picked song |
| DELETE | Asks (`DELETE VERSE?`), then deletes |

A question goes when another song is picked or K2 or K4 turns. The note keys do nothing on the list. Nothing
is written while the song plays: stop first. On the recovery row (RESTORE THE UNSAVED SONG) LOAD restores
the copy and DELETE removes it from flash, asking first, so a restart doesn't offer it again. RENAME and
DUPLICATE don't work there. SAVE, SAVE AS and NEW work as anywhere, since they act on the song being worked on.

In the full build the stock factory sound sets are left out, so a new song starts blank. The workbench can
build a song from a factory sound set and upload it.

## Clip grid

SEQ in the song view. Eight tracks across, 16 rows in two pages of 8, and a SONG row on top. A clip is one
track's loop, a beat to 16 bars long. A track plays either the song or a clip, never both.

| Control | Action |
|---|---|
| SELECT | The track (column) |
| K1 | The row cursor, the SONG row first |
| K2 | REC LENGTH: 1, 2, 4, 8 or 16 bars, or FREE. It starts on 4 bars and keeps the last pick while the FM-1 is on |
| K4 | Launch quantize: off, beat, bar, 2 bars or 4 bars |
| F3 to F4 | Launch that track's cell in the cursor row. An empty cell stops the track. On the SONG row the track goes back to the song |
| G4 to G5 | Launch rows 1 to 8 of the page on every track (a scene). Tracks with nothing in the row stop |
| SHIFT+SEQ | Every track back to the song |
| SEL | Open the clip at the cursor. An empty cell gets a new one-bar clip |
| ENV | Name the row: K1 picks the letter, K2 changes it, YES (OCT+) keeps the name, NO (OCT-) leaves the old one. SAVE does nothing while naming |
| SHIFT + F3, G3, A3 or B3, E4 | Copy, cut, paste, paste, delete the cell |
| PLAY | Play and stop. Launching while stopped starts playback. Stopping gives every track back to the song. During a take, end the recording and play on |
| REC | Record into the cell at the cursor (below). REC again ends it |
| SHIFT+REC | Record a performance (Clips in the song) |

Launches wait for the launch quantize, counted on the song's bar lines. The loop's wrap and the song's end
count as launch points too. A clip starts from its beginning and keeps its phase through song loops and
jumps. The track keys light while their clip plays and blink while a launch waits.

Pasting copied song bars into a cell turns them into a clip. Several copied tracks paste across the row.

### Recording into a cell

REC records into the cell at the cursor, on the cursor's track. The cell blinks red while it's armed and is red
while it records, with how far it has got. The bottom line says what is happening.

- **An empty cell** becomes a clip of the REC length. Playing, it launches on its track at the launch quantize
  (with the launch quantize off, at the next beat), and recording starts there. The track leaves the song or
  its other clip, as with any launch. Stopped, REC starts playing with the REC chooser's count-in, and
  recording starts after it. With a length set, recording ends by itself at the end of the clip, and the clip
  plays on, launched. With FREE, REC ends it at the next bar line (it can run to 16 bars), and the clip is
  that many bars long.
- **A cell with a clip** is overdubbed as loop recording, from its next pass (from its first if it launches for
  the take), until REC or PLAY.
- While a cell is armed or recording, the keys play its track (at the keyboard's octave, with chord mode when
  the track has it) and go into the take, instead of launching. OCT- and OCT+ move the octave. SELECT stays on
  the track, and the SHIFT keys, SEL and ENV wait. The keys launch again when the take ends.
- PLAY or REC ends the take, and playing goes on. In the count-in, before anything is recorded, PLAY or REC
  calls it off, stops, and an empty cell stays empty.

The REC chooser's input quantize applies, and the metronome when it is set to play while recording. The take,
and a new clip with it, is one undo step. SEQ waits until the take is over.

### A clip in the editor

SEL on a cell opens the clip in the song view's editor, looped to its own length. K3, labelled CLIP END,
sets that length. Step entry, recording, automation, glitch and the jobs that stay inside the track all work
on the clip. SELECT stays on the clip's track, and the markers are hidden. SEQ closes it.

The song keeps playing while a clip is open, so a part can be written against the other tracks. REC while
playing records into the clip as loop recording from its next pass, and the rest plays on. REC or PLAY ends the
recording and playing goes on, and PLAY again stops. Recording from a stop works as in the song view.

## Clips in the song

A block in a track's row plays a clip from the grid: "clip 3 from bar 9 to bar 17". Inside a block the clip
loops from the block's start and the track's own notes stay quiet. A stop block keeps the track silent. In
the song view blocks show as shaded bars with the clip's row number, and a stop shows an X. A block ends at
its length or where the next one on its track starts. Editing a clip changes it everywhere it's used.

**Recording a performance.** In the grid, SHIFT+REC arms it. The take starts with the next launch, or with PLAY.
Each launch writes a block for its track from the launch point to that track's next launch, stop or return
to the song. Tracks left on the song are untouched. The song's loop is ignored and the song grows to hold
the take. REC, PLAY or stopping ends it, PLAY with playing going on, and the whole take is one undo step.

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
chord at the pointer, strummed when strum is on, and F5, F#5 and G5 enter the last chord again, an octave down
or an octave up. Rest, tie and repeat stay on SHIFT there.

## The workbench

The workbench page (README.md, The workbench) has the Songs and kits section. Two of its buttons fill the
device from what FM1 Open's page carries:

| Button | What it does |
|---|---|
| Send the built-in kit | FM1 Open's 8 built-in sounds as a kit named BUILT-IN, unless they are on the device already |
| Send the factory DX7 voices | M-VAVE's 128 factory DX7 voices into the 4 banks of the DX7 library |

The full build leaves the factory voices out of the firmware, so until the DX7 library holds voices every DX7
preset there is blank. Sending them puts them in the library, in the user area, where they stay through
firmware updates. The workbench reads the 4 banks first, and when a bank holds voices of its own (saved
earlier, not the factory's) it asks before replacing it. It reads every bank back after writing and says if
one doesn't match. The new voices play at once, with no restart.

On the full build the page's project library tab connects, but it has no project slots: they show as empty, and
saving to one is refused. Songs and kits take their place.

## Diagnostics page

Only in the default device build, the hardware test build, and in the preview `./build.sh --test-preview`
makes. SEQ opens it from the stock pages, and SEQ again goes to the stock step page. The full build and the
regular preview don't have it.

| Control | Action |
|---|---|
| PLAY | Play and stop the built-in test song (12 bars) |
| K1 | The note delivery method, A or B, while stopped. It starts on B |
| SEL | Clear the numbers |
| ENV | Memory probe, with each engine on track 6 in turn (stopped) |
| LFO | Largest free memory block |
| HOME | Back to the stock overview |

The page shows control loop timing, how late notes land for the method in use, audio health and memory. A
memory figure with "+" or "OR MORE" reached the top of the probe's search, so it's that much or more. The
hardware test plan in `docs/hardware-test-plan.md` says what to write down.
