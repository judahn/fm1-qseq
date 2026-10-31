# Changelog

The version is `FM1_QY_VERSION` in `app/version.h` (semver), and each release is tagged `v<version>`.

## 0.5.5

- SHIFT+E4 in the song view deletes what's selected: the notes, clip blocks and automation that start in the
  selection, on the selected tracks only. Nothing moves and the other tracks are left alone. It deleted the
  bars from every track before, and later bars moved left. It works in the clip editor now too. With nothing
  there, the bottom line says `NOTHING TO DELETE`.
- Pasting no longer moves what comes after. SHIFT+A3 pasted inserting, pushing later bars right, and now
  pastes over what's there, as SHIFT+B3 does. Duplicate (SHIFT+C4) goes over the bars after the selection
  instead of inserting. Either one past the song's end grows the song to fit. SHIFT+D4 still inserts bars,
  on every track, the one edit that moves what follows.
- The selection is easier to see: a cyan outline around it on each selected track, over the notes and clip
  blocks, and a lighter fill. The paste outline stays white.
- A held GLO opens the mixer, and now it stays up when GLO is let go, from any song page, the grid and the clip
  editor included. A GLO tap there goes back to the page you were on. A tap of GLO on a song page still opens
  the song settings.

## 0.5.4

- SHIFT+D#4 clears the clipboard, in the song view, the clip editor and the grid, and the song view's paste
  outline goes with it. The bottom line says `CLIPBOARD CLEARED`, or `NOTHING COPIED` when it was empty. D#4
  is the black key beside E4, which deletes. Before this the clipboard only emptied when the FM-1 was turned
  off.

## 0.5.3

- K3 in the grid sets the length of the clip at the cursor, where it only showed it: a bar a click, or a beat
  with SHIFT, from a beat to 16 bars. Without SHIFT it stops at a bar, unless the clip is shorter already. A
  length off the bar steps onto it first: a bar and 2 beats goes to 2 bars, or back to 1. Shortening keeps
  the notes past the new end, hidden, and they play again when it grows. It works on a clip playing, which
  carries on in its new length, and on an empty cell it does nothing. Like CLIP END in the clip editor, it
  isn't an undo step.

## 0.5.2

- PRESETS and ALGO, side by side on the panel, are where you are and how wide you see on every page with a
  playhead. In the song view, the clip editor and the automation view PRESETS moves the playhead one snap step,
  the start point with it, and SHIFT+PRESETS goes marker to marker, or in the automation view change to change.
  ALGO zooms as before, and since the footer no longer shows them, a turn shows the span and the snap on the
  bottom line for a moment, for example `ZOOM 8 BARS  SNAP 1/2 BAR`. K1 used to move the playhead and K4 to
  zoom.
- The knobs under the song view and the clip editor are TEMPO, QUANTIZE, LOOP IN and LOOP OUT, or CLIP END.
  The loop moved from K2 and K3 to K3 and K4, and the marker section from SHIFT+K2 to SHIFT+K3.
- K1 is the tempo the header shows: the starting tempo, or the tempo change the playhead is past. In the clip
  editor that's where the song is, the tempo you hear, not the clip's own place. It works stopped or playing,
  heard at once, and the stock instrument's tempo follows. Settings page 1 still sets the starting tempo.
- K2 is input quantize, the record settings' K3, and turning it after a take requantizes that take, with
  `REQUANTIZED THE LAST TAKE` on the bottom line. It works on a take made in the clip editor while the clip is
  open. A take into a grid cell closes its clip and can't be requantized afterwards, as before.
- Tempo is set in whole BPM, with SHIFT for half, on every knob that sets one: K1 in the song view and the
  clip editor, settings page 1 (SHIFT gave hundredths) and the automation view's K2 on the tempo lane. A tempo
  with hundredths, from a MIDI file or an older song, keeps them until a knob turns it, and the first step lands
  on the knob's grid: 119.88 goes to 120 or 119, or with SHIFT to 120 or 119.5. A half shows with one decimal,
  79.5, and two decimals only for real hundredths.
- The automation view's knobs are LANE, VALUE, LOOP IN and LOOP OUT. K1 picks the lane, PRESETS did. K2 sets a
  change at the playhead while stopped, as SHIFT+ALGO did, and the footer shows the lane and its value there.
  SHIFT+ALGO only zooms now. The bottom line names no knobs: the row and its value, SEL DEL, and with SHIFT,
  PRESETS for change to change and SEL for clearing.
- Step entry swaps PRESETS and K4. PRESETS moves the pointer, SHIFT+PRESETS jumps note to note, and holding LFO
  while turning PRESETS sweeps a selection. K4 is the pitch knob: scale steps, octaves with SHIFT, pad to pad
  on drum tracks, and every note of a sweep. Its place in the footer reads PITCH, OCTAVE with SHIFT, or PAD on a
  drum track, and the glitch pages keep it.
- Takes are as they were: K1 to K4 record their lanes and ALGO zooms the roll. PRESETS does nothing during a
  take.
- SHIFT in the song view names PRESETS for markers on the bottom line (EDIT JOBS made room), and step entry's
  line names PRESETS for moving and sweeping.

## 0.5.1

- K1 in step entry runs from the whole note: 1/1 at the left, and each step right halves the length, down to
  1/64. It used to lengthen turning right. Now it runs the same way as SHIFT+F to E and as ALGO's zoom.
- LOAD on the song list opens the loaded song in the song view, and so does restoring the unsaved copy. NEW
  and the other actions still stay on the list.
- Early keys land on the beat. A key up to 100 ms before a take's first beat goes on it, in the count-in, before
  a grid cell's launch (new), or as a playing clip comes round (new). In a LOOP take and every take into a
  grid cell, a key that early before the loop's end goes on the loop's first beat, where the next pass
  starts. It ends where it is let go, or let go before the beat, it keeps the length it was held. It used to
  be cut to a sliver at the loop's end. The window
  stays under a 16th at fast tempos, about 94 ms at 120 BPM, and with input quantize on it is half the grid
  when wider. The count-in's window was 50 ms, and earlier keys there were lost.
- Quantize goes round the loop: input quantize and requantizing a LOOP take send a note that lands on the
  loop's end to its start, and so does the QUANTIZE job in the clip editor. It used to land just past the
  end, where the loop never plays it.
- Fixed: REC on a playing clip in the grid, with REPLACE, while its notes sounded, could leave the take
  half started, so later REC presses and undo went wrong. The player's own notes, cut as the clip emptied,
  counted as live keys. They never reach a take now.
- PLAY's light blinks with the beat while the song plays, lit for the first half of each beat, on every page
  and through a take's count-in, whether or not the metronome is on. Stopped, it is lit as before.

## 0.5.0

- PLAY in step entry plays and stays there, from the start of the pointer's bar: the song, or in a clip the
  clip with the grid's launched clips, as the clip editor plays it. A playhead runs across the roll, which
  pages to follow it, and PLAY again stops back at that bar with the pointer where it was. While it plays the
  keys only sound, SHIFT still sets the length, triplet and dots, and rest, tie and repeat wait for a stop.
- REC in step entry records a take there, as REC does in the other views: a tap records from the pointer's bar
  with the count-in, a hold opens the record settings, and HOME or REC there comes back. The take stays in
  step entry. SHIFT+REC leaves step entry now, and so does SEQ, back to the view it was opened from, the
  playhead at the pointer. Leaving while playing keeps playing, and from the grid the clip stays launched.
- ALGO zooms step entry's roll: 4 bars, 2 bars, 1 bar or half a bar, 1 bar to start, kept while the FM-1 is
  on. The span shows at the right of the header, and at 2 and 4 bars the bars are numbered over the roll. The
  dashes under the roll follow the zoom, 8ths at 4 bars down to 64ths.
- The roll has lines: bar lines brightest, beat lines dimmer, and at 1 bar or closer faint 16ths, with 32nds
  at half a bar. A level shows only where its lines are 4 px apart or more. Notes end 1 px short, so two in a
  row on one pitch read as two.
- Hold LFO and turn K4 to sweep a selection: every note from where the sweep began to the pointer, both ends
  in, at every pitch, under a lighter band, with the count on the SEL line. With LFO still held a key picks
  its pitch alone, and later keys toggle theirs, so one pad of a drum part can be picked across bars. SEL
  deletes the selection, K2 changes its velocity by the same amount for every note, K1 and K3 scale its
  lengths as the entry's length changes, the glitch knobs and PRESETS act on all of it, each as one undo step.
  A selection too big to undo is refused with a notice. Moving the pointer without LFO, entering a note or
  leaving ends it.
- The glitch layer's pages step with ARP: off, 1, 2, 3 and off again. ALGO zooms there too.
- A live take into one track shows the roll while it lasts, from the song view, the clip editor, a grid cell
  and step entry: the take's track, the playhead, the count-in, ALGO's zoom and the recording knobs. A note
  shows as its key comes up, when the take writes it. When the take ends the view comes back. A performance
  stays on the grid and a take in the automation view stays there. During a cell's take K1 to K4 record their
  lanes, as in the song view.
- PLAY starts from a start point, and every stop comes back to it: PLAY again, a take ending with a stop, or
  the song playing out. K1, SHIFT+K1, the marker keys, SHIFT+PLAY, the automation view's SHIFT+K1, leaving
  step entry and step entry's PLAY set it. A song loaded or new starts at the top. The song view's ruler marks
  it while the playhead is elsewhere. A clip in the editor has its own, from its start. Before, a stop left
  the playhead where it stopped.
- ALGO zooms the song view and the clip editor as K4 does, and the automation view too. There SHIFT+ALGO sets
  a change at the playhead, tempo in whole BPM, and hundredths are on the settings page only.

## 0.4.1

- Step entry opened from the grid goes back to the grid. REC there closes the clip and returns, and PLAY
  returns, launches the clip and plays, so entering notes and listening to them stays in the grid. It went to the
  clip editor before. Opened from the automation view it goes back there too, instead of to the song view.
  Opened from the song view or the clip editor, it goes back there as before.

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
