# Changelog

The version is `FM1_QY_VERSION` in `app/version.h` (semver), and each release is tagged `v<version>`.

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
