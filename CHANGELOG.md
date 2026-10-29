# Changelog

The version is `FM1_QY_VERSION` in `app/version.h` (semver), and each release is tagged `v<version>`.

## 0.4.0

- A tap of REC records. Stopped in the song view or the automation view it starts a take in the song's mode,
  with its count-in, and in the grid it records into the cell at the cursor. It acts as REC comes up. A REC that
  ends a take, calls off a count-in or ends a performance still acts as it goes down.
- SHIFT+REC is step entry: at the playhead in the song view, in the clip with one open in the editor, and in the
  grid in the cell's clip from its start. An empty cell gets a clip of the REC length first, 4 bars when it's
  FREE. Step entry still starts only while stopped. On the grid's SONG row it says it needs a row.
- REC held for 600 ms opens the record settings (the REC chooser before) from the song view, the automation
  view and the grid, and HOME goes back there. The list reads REPLACE, OVERDUB, PUNCH, LOOP, then STEP. A live
  mode picked there becomes the song's. STEP never does: with it picked, REC or PLAY goes to step entry, and the
  song keeps its live mode. With a live mode picked, REC or PLAY goes back and does what a tap of REC does
  there.
- New songs record in OVERDUB. A song saved with STEP, the old default, loads in OVERDUB.
- SHIFT+SEL switches the mode between REPLACE and OVERDUB, and from PUNCH or LOOP to OVERDUB, in the song view,
  the clip editor and the grid. The mode shows in red at the right end of the bottom line in the song view and
  the grid.
- In the grid REC on a clip follows the song's mode. REPLACE empties the clip first, keeping its length, in the
  same undo step as the take, and the other modes overdub it. REC on the SONG row arms a performance, which
  SHIFT+REC did before.
- Playback follows the view. The song view, the automation view and step entry play only the song, and the grid
  and a clip opened from it play only launched clips. Going to the grid stops every track playing the song at
  the next launch point, the transport running on, and SEQ back to the song view sends every track back to the
  song there. Stopped, or with LAUNCH Q off, it happens at once. SHIFT+SEQ is gone, since this is automatic.
- On the grid's SONG row F3 to F4 stop their track, since the song can't be heard in the grid. G4 to G5 still
  launch rows.
- A performance writes what was heard over the take. Launched clips become blocks as before, and tracks that
  were silent get stop blocks, so the song plays back as the performance sounded.
- Clip blocks in the song view are drawn in the track's full color with the clip's number in black, as the
  grid's cells are, and the clip's notes on them. The track's own notes, silent inside a block, aren't drawn
  there.
- In the full build the song list's header shows the free heap right of the battery: the largest free block
  inside the heap, measured once each time the list opens and only while nothing plays. "+" means the top of
  the probe's search, and "?" that there's no reading. The browser preview has no heap and shows a dash. The
  free flash moved up a line to make room.

## 0.3.3

- The mix page shows each track once: a tile a track, with a MUTE row and a SOLO row that light when they're on.
  The keys are the same, 1 to 8 mute and 9 to 16 solo. In the browser, a click on either row toggles it.

## 0.3.2

- The battery bar on the song list is set from a device: full at 622, a full battery read with USB unplugged,
  and empty at 505, an estimate. It read empty before, since the guessed ends were 700 to 950.
- No track starts muted. The engine's default sounds muted tracks 6 and 7, so the song at power-on had them
  muted. A song you load keeps the mutes it was saved with.

## 0.3.1

- A new song is named with the lowest number no saved song uses (1, 2, 3 and so on) instead of NEW SONG, so renaming
  it is quick. SAVE AS and DUPLICATE of a song still named with a number take the next free one.

## 0.3.0

- SAVE opens the song list, and SAVE there saves, so a save is SAVE, SAVE. HOME+SAVE and SHIFT+HOME+SAVE still undo and
  redo, in either order.
- REC in the grid records into the cell at the cursor. An empty cell records a clip of the REC length (K2: 1, 2, 4, 8 or
  16 bars, or FREE) from the next launch point, ends by itself and plays on. A clip already there is overdubbed from
  its next pass. While a cell is armed or recording the keys play its track. SHIFT+REC records a performance.
- PLAY during a take ends the recording and playing goes on, and PLAY again stops. REC does the same. In a count-in
  PLAY or REC calls the take off and stops.
- The live stutter and the FX lane are gone. Songs with FX lane events still load, without them.
- ARP on the song view, the grid, the automation view and the settings is the arpeggiator again: a tap turns it on or
  off, a hold opens its settings, and HOME comes back.
- A tap of FX on those pages opens the effects page, and FX there comes back. Held, FX is still SHIFT.
- The engine's tempo follows the song's where the playhead is, so the arpeggiator, the synced delay and MIDI clock
  out keep time with the song. A tempo set on a stock page or by tap tempo goes into the song.
- The song list's header shows the battery's reading and a bar. The bar's two ends are guesses until the reading is
  checked on a device.
- The full build's install page connects to the project library again: the stock project slots, gone in that build,
  show as empty instead of failing with error 65, and a save to one is refused cleanly.
- The workbench can send M-VAVE's 128 factory DX7 voices to the DX7 library, for the full build, which leaves them out.
  It asks first when a bank holds voices of its own.
- If a take fills the song's memory, it ends there with a notice.

## 0.2.0

- Step entry keys by note name: with SHIFT held, F to E pick 1/1 down to 1/64 in any octave, F# is triplet, G# dots,
  C# rest, D# tie and A# repeat.
- F5, F#5 and G5 enter notes in step entry and light with the scale. In chord mode they enter the last chord
  again, an octave down or an octave up.
- The song list is a menu: K2 picks the action, OCT+ is YES and OCT- is NO. Loading or a new song asks only when
  unsaved changes would go, and deleting always asks.
- YES and NO on the edit jobs and when naming songs and grid rows.
- SEL in the song view adds a marker at the playhead or removes the one there. Playing, it only adds.
- Deleting the unsaved song on the song list removes its recovery copy from flash, so a restart doesn't offer it
  again.
- The version shows on the boot screen, in the song list's header and in the workbench page's version badge.

## 0.1.0

- The first full song mode build put on an FM-1 (commit 74dd785): song mode, step entry, recording, automation,
  glitch, storage, edit jobs, clips and chord mode.
