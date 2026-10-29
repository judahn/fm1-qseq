# Song mode design proposal (draft for review, 2026-10-27)

The aim is the QY70's song mode and step entry, as described in `qy70-reference.md`, rebuilt clean-room on
the FM-1, with a few things modern DAWs do better. Song mode comes first. Pattern mode is a later phase, and
the format leaves room for it. Everything here is a proposal. The control map is tentative until the device
is in hand and the panel layout can be checked.

## 1. What we keep from the QY70, and what we change

| Area | QY70 | Here |
|---|---|---|
| Song shape | Free-form, linear, up to 999 measures | Same |
| Resolution | 480 clocks per quarter, `measure:beat:clock` | Same. Every length below is a whole number of clocks |
| Tracks | 16 sequence tracks plus pattern, chord and tempo tracks | 8 tracks, one per FM-1 sound slot, plus tempo and marker tracks. Clips per track come in phase 7 (8f), and chords are a play setting (8g) |
| Step lengths | 32nd to whole, three triplets, no 64th, no dots (p. 84) | 64th to whole, triplet on any of them, dot and double dot |
| Step controls | On-screen fields changed with -1/+1, four function keys: del, back-delete, rest, tie | Lengths on SHIFT plus keys, and on a knob. Four context buttons for del, back-delete, rest, tie, as on the QY70 |
| Quantize | Only as an edit job afterwards, plus groove quantize at playback | Edit job as on the QY70, plus input quantize while recording |
| Recording | Replace, overdub, step, no punch-in | Replace, overdub, step, plus punch and loop recording |
| Navigation | Top, back or forward one measure, jump to a measure number | Same, plus markers, a loop region and scrub on a knob |
| Undo | One level | Several levels, as far as memory allows |
| Automation | Only as controller events typed in the event list | Turn a knob while recording and it records as automation |

## 2. Timing

- Playback runs on the internal clock: the audio sample clock, counted in our own audio interrupt. Tempo and
  position are worked out from samples with the remainder carried, so nothing drifts.
- Notes reach the engine on the next 2.9 ms audio block, the same grid as the stock sequencer. Our player
  hands each note over in the block before it is due.
- **Delivery: method B, chosen by the hardware timing test (10/28).** The control loop hands the notes over,
  and while a song plays (or a strum waits) the audio interrupt wakes it after every block, so the hand-over
  comes in the block before. The test build measured the control loop on its own at 9.9 / 10.8 / 12.1 ms
  (min / average / max), 3 to 5 blocks, with a 13 ms worst run. On its own (method A), 184 notes came in
  their block, 200 one block late, 230 two late and 162 three or more. With the wake (B), 1,312 of 1,361 notes
  came in their block and the other 49 one block late, never 2. The audio stayed clean on both (no overrun, underrun,
  overload or MIDI drop), and B sounded tight. Method C, the interrupt posting the notes itself, wasn't
  needed and is gone. The full build and the browser preview always deliver by B. The default test build
  keeps the choice of A or B on its diagnostics page, so the test can be run again. Results:
  `docs/hardware-test-plan.md`.
- The stock step sequencer is not used.

## 3. The song

- **8 tracks.** Each plays one sound slot (FM, 303, 808, PSG, pluck or sampler), stored as a linear list of
  events across the whole song: notes (start, pitch, velocity, length) and controller changes (automation).
- **Tempo and time signature** changes at any position.
- **Markers:** up to 16 named positions. While stopped, a jump is immediate. While playing, it waits for the
  next bar line so the music stays in time.
- **Loop region:** start and end follow the current snap (section 6). It can be set from two markers.
  Recording inside a loop is how you build up a section, like a pattern, without leaving song mode.
- **Sounds:** each song stores its 8 sound setups and mix, so loading a song brings its sounds with it.
- **Clips and clip lanes** (8f, phase 7): looping clips per track, and blocks on each track saying which clip
  plays where. Chords are a play setting per track (8g).

**Memory.** Notes are stored compactly, roughly 4 to 6 bytes each, and automation only stores changes. There
is about 247 KB of working memory, shared with the sound engines. The real budget gets measured in the browser
preview and then on the device. A target to confirm is several thousand notes per song.

## 4. Step entry

**Lengths.** 64th, 32nd, 16th, 8th, quarter, half and whole. Triplet applies to any of them, and dot or double
dot to any of them. A dotted quarter is 720 clocks and a 64th triplet is 20, all exact at 480 per quarter.

**Entering notes.** Play a key or chord. It goes in at the pointer with the current length, velocity and gate,
and the pointer moves on by one length. A chord registers when the keys are released, so it can be built key
by key. The QY700 works this way. The QY70 manual doesn't say.

**Gate:** staccato 50%, normal 90%, slur 99% (the QY70's three), plus full 100%.
**Velocity:** the QY70's ten levels (ppp to ffff) plus four random settings.

**Function buttons.** One of the four function buttons is SHIFT in every mode (FX, tentatively, depending on
which sits best on the panel). HOME stays HOME. In step mode the other three are labeled on screen like the
QY70's function keys:

| Button | Action |
|---|---|
| FX | SHIFT |
| SEL | Delete the selected notes at the pointer |
| ENV | Back-delete: move back one length and delete there |
| LFO | SELECT: hold it and press keys to choose which notes at the pointer to edit |

**Rest and tie on keys.** The FM-1 has 27 keys, two more than the QY70's 25. In step mode all 27 enter notes,
and the functions sit on SHIFT plus a key, by the key's note name in any octave (Judah, 10/28). The first plan
kept the top three keys for rest, repeat and tie, and put the lengths on the lower octave, shortest first.

**SHIFT plus keys (step mode), in every octave:**

| Key | Action |
|---|---|
| F G A B C D E (white) | Whole, half, quarter, 8th, 16th, 32nd, 64th: the longest on F |
| F# | Triplet on or off |
| G# | Dot, double dot, off |
| C# | Rest: move on one length |
| D# | Tie: lengthen the last note by the current length. The length can be changed first, so a quarter plus an 8th tie works (QY700 behavior) |
| A# | Repeat: enter the last note or chord again (quick hi-hat runs) |

With SHIFT held no key enters a note or strikes a chord. K1 still lengthens turning right.

**Knobs (step mode):** K1 length, K2 velocity, K3 gate. K4 moves the pointer by the current length. SHIFT plus
K4 jumps from note to note. SELECT picks the track. Landing on a note plays it, as on the QY70.

**Editing what's there.** The pointer is the selection. When it lands on existing notes, they're selected
(the whole chord by default), and K1, K2 and K3 change their length, velocity and gate in place. On an empty
position the same knobs set the values for the next note entered. To narrow a chord, hold SELECT (LFO) and
press the keys of the notes you want. Delete removes whatever is selected. Range edits across many bars, like
softening a whole hi-hat part, belong to the edit jobs in phase 6.

**Screen:** one bar at a time in a grid of 32nd-note cells (the QY70's dash display), widened for 64ths when
needed. Notes are drawn as bars with their real lengths, so ties and dotted values show. Position, length,
velocity and gate are shown along the top.

## 5. Real-time recording

- **Modes:** replace, overdub, punch (records only between the loop points) and loop recording (each pass
  overdubs while the loop repeats).
- **Count-in:** off, 1 or 2 bars. Metronome on, off or record only.
- **REC (0.4.0):** a tap records at once in the song's mode, one of the four live modes (OVERDUB in a new
  song), SHIFT+REC is step entry, and a hold opens the record settings. There STEP is listed after the live
  modes and is never kept as the song's mode. SHIFT+SEL switches REPLACE and OVERDUB.
- **Input quantize:** off, or any length above, including triplets, with a strength setting. Recorded notes
  are stored already quantized. The original timing is kept until the next pass, so quantize can be undone.
- **Quantize job afterwards,** as on the QY70: grid, strength 0 to 100%, swing, over a range of bars on one
  track or all.

## 6. Navigation and playback (song view)

| Control | Action |
|---|---|
| PLAY | Play and stop |
| SHIFT + PLAY | Back to the start, or to the loop start when looping |
| REC | A tap records in the song's mode, SHIFT+REC is step entry, a hold opens the record settings (0.4.0) |
| K1 | Move the playhead by one snap step. With SHIFT, it moves marker to marker |
| K2 / K3 | Loop start / loop end, by snap step |
| K4 | Zoom the timeline. The snap follows the zoom |

**Snap follows zoom** (an adaptive grid, as in Logic and Ableton). Zoomed out, the snap is 1 bar. Zooming in
steps it down to a half bar, a beat, then an 8th, which is as fine as song view goes. The current snap is
always shown on screen (`SNAP 1 BAR`). K1, the loop edges, selections and paste positions all use it, so what
you see is the grid you move by. There's no acceleration, so a turn always moves by exactly one snap step.
Long jumps use markers, or zoom out and move by bars. Step mode is separate: its pointer moves by the note
length.
| SELECT | Track |
| FX | SHIFT |
| SEL | Stopped: add a marker at the playhead, or remove the one there. Playing: add one at the nearest snap step, never remove |
| ENV | Loop on or off. With SHIFT, metronome |
| LFO | SELECT: hold and tap track keys to choose which tracks the selection covers (6a). With SHIFT, automation view on or off |
| SHIFT + upper white keys F4 to G5 | Jump to markers 1 to 9 |

**Screen:** tracks as rows, bars across, notes as small marks, markers as flags along the top, and the loop
region shaded. The 240x240 color screen has room for all 8 tracks at once.

## 6a. Selecting, copying and pasting bars

**The loop region is the selection,** as in most DAWs. K2 and K3 set its start and end by the current snap.
Two shortcuts make selecting quick:
- SHIFT plus K2 selects the marker section around the playhead, from the marker before it to the next one.
  Turning further adds the next section.
- The selection shows as a shaded band across the timeline, with its length in bars.

**Which tracks** (Judah's scheme, 10/27). The selection starts as the track you're on. Hold SELECT (LFO) and:
- Tap a track key to add or remove that track. The first 8 white keys, F3 to F4, are tracks 1 to 8.
- Tap the 9th white key, G4, for all tracks. If all tracks are already selected, it goes back to just the
  track you're on, like a select-all checkbox.

While the selection is just the current track, it follows you when you change tracks with the SELECT knob.
Once you've picked tracks by hand, it stays put until you change it. The shaded band only covers the
included tracks' rows, so the screen always shows what a copy will take. This is the same gesture as in step mode, where holding SELECT and
pressing keys picks notes. Notes and automation are copied together.

**Actions,** on SHIFT plus the lower white keys in song view (in step mode those keys are note lengths):

| Keys | Action |
|---|---|
| F3 | Copy the selection |
| G3 | Cut |
| A3 | Paste at the playhead, inserting (later bars move right) |
| B3 | Paste at the playhead, replacing what's there |
| C4 | Duplicate: paste right after the selection and move the selection along, so repeated presses keep extending (4 bars become 8, then 12) |
| D4 | Insert empty bars the length of the selection |
| E4 | Delete the selected bars (later bars move left) |

**Where a paste lands.** Paste always goes to the playhead, which can be anywhere: move it with K1 by the
current snap (marker to marker with SHIFT), or jump to a marker, then paste. A selection that
starts on a bar line and is pasted with the playhead on beat 3 starts on beat 3. After a copy, a faint outline
shows where the paste would land as you move the playhead, so you can see it before committing. Only
Duplicate pastes right after the selection.

**Which track a paste goes to.** A single copied track pastes onto the track you're on, so a bass line can be
copied onto a synth track. Several copied tracks paste back onto their own tracks.

When bars are inserted or deleted, the markers and tempo changes after that point move with the music. The
clipboard holds one selection and survives switching songs, so bars can move between songs. Its size is capped
by memory and gets measured with everything else. Every action can be undone.

## 6b. Edit jobs (phase 6)

**Where they live.** SHIFT+EDIT in the song view opens the job page, since EDIT alone stays sound editing (8c)
and jobs are edits. It works like the QY70's Job menu: K1 picks the job, K2 to K4 set what the screen lists
(ALGO turns to the next three, SHIFT moves a setting by ten), and YES (OCT+) runs it after asking once, as the
QY's ENTER. The question says how many events the job takes on, and warns when it is too big to undo. NO
(OCT-) cancels it, and so does changing the job, a setting or the range. NO with nothing asked, HOME or EDIT
goes back, SAVE saves the song, PLAY plays, and HOME+SAVE undoes as everywhere. Every job is one undo step.

**Range.** Most jobs start with RANGE: the selection (the loop region on the 6a tracks, picked here too by
holding LFO and tapping F3 to G4) or the whole song. The screen always shows it as bars and tracks. Copy,
merge, clear and normalize work on whole tracks. An event belongs to the range by its start.

| Job | What it does |
|---|---|
| Quantize | Toward the nearest grid line of the bar, 1/64 to 1/1 with triplets, strength 0 to 100%, swing 50 to 83%, and the note ends too if GATE is on |
| Velocity | Set, or rate 0 to 200% plus an offset |
| Crescendo | Velocity ramps by -99 to +99 across the range |
| Gate time | Set in clocks, rate plus offset, or legato to the next note on the track |
| Transpose | Semitones or scale steps, only inside a note range. Drum tracks stay |
| Fit to scale | Notes up to the next note of the song's key (8a). Drum tracks stay |
| Shift clock | Notes and automation by plus or minus clocks, kept inside the range |
| Chord sort | A chord's notes low to high or high to low |
| Strum | Each chord's notes start a set number of clocks apart, in sort order. Their ends stay |
| Paste N times | The clipboard from the playhead, over what is there, each copy transposed one more step. The song grows to fit |
| Erase | Notes, controllers, program changes or all |
| Extract | One kind of event moves to another track |
| Ramp | A controller the track's engine takes, or tempo: start, end, a point every 1/64 to 1/1, curve -16 to 16. It replaces that lane in the range |
| Thin out | Every other change in a lane, or every other tempo change |
| Time stretch | 50 to 200%, gates included. Later events, markers and tempo changes move, as inserting or deleting time does. Shrinking needs the tracks left out to be empty where time closes |
| Copy track | Events, sound or both, replacing the target |
| Merge tracks | One track into another |
| Clear track | Every event of the tracks. Their clip blocks stay |
| Normalize | Swing, humanize and the glitch attributes become plain notes, as the next play from the top would roll them, so they edit and export to MIDI. The tracks' swing and humanize go off |

## 7. Automation

- While recording, K1 to K4 record parameter moves on the selected track. Each knob shows which parameter it
  controls.
- Automation travels the same path as notes: controller messages each track's engine already understands.
  All engines take volume, pan, expression, reverb send and LFO depth. The 303 adds cutoff, envelope mod and
  slide, and the PSG and pluck engines add their tone controls.
- Only changes are stored, thinned to about one per 32nd note, so a four-bar sweep costs a few hundred bytes.
- Deep FM operator parameters are left out. The engine pauses audio briefly to change them, which would click.
- The automation view shows one parameter's curve per track. Editing points comes later.

## 8. Drums

The sampler engine plays 8 pads per kit. Samples load through the browser workbench, which already handles
that. In step and real-time entry, the 8 pads sit on the white keys, matching the stock firmware's layout.
808 kits work the same way. All 8 pads share one sample bank of about 19 seconds, stored compressed.

Drum features fall into three places, depending on what the hardware allows:

**Built into the sampler (per pad, live on the device):** level, tune (plus or minus 24 semitones), length
(trims the end), choke groups (4, for open and closed hats), variation (a four-hit cycle of small pitch, level
and start changes, so repeated hits don't sound identical) and velocity filter (softer hits sound darker).

**Prepared in the browser when loading samples.** The sampler engine is closed code, so anything that changes
the audio itself happens before upload, at no cost to the device: reverse, start trim, fades, normalize, gain,
lo-fi (bit and sample-rate reduction), and **slicing**, which chops a loop or break across the 8 pads, one
slice each. A reversed copy of a sound needs its own pad and memory, since the device can't reverse on the fly.

**In our sequencer (per note, cheap):** accent, flam, rolls (repeat a hit 2, 3 or 4 times inside a step),
probability, humanize (small random timing and velocity) and per-pad swing.

**Kits** (Judah, 10/27: one kit per song, saved independently of songs, switchable).
- **A kit** is 8 samples plus their names and pad settings (level, tune, trim, choke). It's saved on its own,
  separate from any song.
- **Kit library on the device.** The engine plays 8 pads, but which audio each pad plays is decided by our
  own code (`app/samples.c` points each pad at its data in flash). So the sample memory, about 19 seconds,
  can hold several kits instead of one bank. With short one-shots that's several kits (8 hits of about 0.4 s
  each is about 3 s per kit). Kits that share a sample store it once.
- **In a song.** Each song names its kit. Switching a song's kit from the song settings is instant, because
  it only re-points the pads. Nothing is written to flash. Switching at a marker partway through a song
  looks possible for the same reason, and gets tested for clicks before it's offered.
- **On the computer.** The browser editor exports and imports kits as files, for a library of any size, and
  loads them into the device's library. This replaces the stock single-bank format with a multi-kit one.
- **Also possible:** the 808 kit (11 synthesized drums) on another track alongside the sampler kit.

**Time-stretching is out of scope** (Judah, 10/27). Samples are short one-shots, and the bank is small.

## 8a. Settings, swing and scales

**Where settings live.** GLO opens the song's settings, which are saved with the song. SHIFT plus GLO opens the
device settings (MIDI, clock, Bluetooth), which is the stock firmware's global page, kept as is. The stock sync
and MIDI pages join it there (10/28), since SEQ now opens the clip grid.

**Song settings:**
- Tempo and time signature (the starting values, since changes can happen anywhere in the song). Since 0.3.0
  the stock instrument's own tempo (whole BPM, 30 to 300, for the arpeggiator, the synced delay, the stock
  pages and MIDI clock out) follows the song's tempo at the playhead, rounded, and a tempo set on a stock page
  or by tap tempo sets the song's tempo in force there, so the two never disagree
- Swing: amount and grid (8th or 16th), plus an on/off per track so drums can swing while the bass stays
  straight. Swing is applied at playback and never changes the recorded notes, like the QY70's groove
  quantize. Turn it off and everything is straight again.
- Key and scale (below)
- Recording: mode, count-in, metronome, input quantize and strength

**Key and scale.** Chromatic is the default, which means no snapping. Pick a root and a scale and pitched
tracks follow it:
- Scales: major, natural minor, Dorian, Phrygian, Lydian, Mixolydian, Locrian, harmonic minor, melodic minor,
  major and minor pentatonic, and blues. A custom scale (toggle any of the 12 notes) can come later.
- Playing a note outside the scale moves it up to the next note in the scale. This applies in step entry,
  real-time recording and live playing.
- The pitch knob on selected notes moves by scale step, so editing stays in key. With SHIFT it moves by
  octave. In chromatic it moves by semitone. The pitch knob is the seventh encoder, the one the stock firmware
  uses for browsing presets.
- The key LEDs light the notes in the scale, so you can see the scale on the keyboard.
- Drum tracks (808 and sampler) ignore the scale, since their keys are pads rather than pitches.
- Changing the scale later doesn't rewrite notes already recorded. A "fit to scale" edit job can do that on
  purpose (phase 6), and so can a transpose job that moves notes by scale steps.

All of this is our own code and costs a few bytes per song. The stock firmware has no musical scale feature.
Its SEL button opens the FM engine's keyboard level scaling, and its per-track tuning tables are for microtonal
tuning, not keys.

## 8b. Export and import

The FM-1 has no card slot or USB drive mode. It talks to a computer over USB-C (and Bluetooth) as a MIDI
device. The stock firmware already moves projects, samples and DX7 voices that way: the browser workbench in
Chrome or Edge sends and receives them as SysEx messages (`app/transfer.c`). Songs ride the same channel with
a few new commands, and the workbench gets a Songs section. The device code stays small. Any conversion
happens in the browser.

- **Song file (our own format).** A full round trip: notes, automation, markers, loop, tempo, scale, swing and
  the 8 sound setups. Save to the computer, load back, share with someone else who has the firmware.
- **MIDI file export (.mid).** Standard MIDI File, one track per FM-1 track, with tempo and time signature
  changes. Markers become MIDI marker events, which DAWs show as markers, and automation becomes controller
  data. This is how a song gets into a DAW.
- **MIDI file import.** The browser maps the file's tracks onto the 8 FM-1 tracks (you pick which) and brings
  in notes, tempo, time signature, markers and controllers. Anything over the song's note capacity is reported
  before importing, not silently dropped.

The Standard MIDI File format is a published standard, so this stays clean-room. Transfer speed over USB MIDI
is a few seconds for a large song.

## 8c. Saving, sound editing, mixer and other defaults (agreed 10/27)

- **Saving.** SAVE writes the song to flash. Since 0.3.0 SAVE opens the song list as it comes up, and SAVE
  there writes the song, so a save is SAVE, SAVE and HOME+SAVE (undo) never shows the list. A recovery copy is
  also written automatically whenever playback stops, since it's a battery device and power-off would lose unsaved work. Flash writes briefly mute audio,
  so nothing is ever written during playback.
- **Several songs on the device.** The user flash is about 524 KB: 68 KB of stock project slots, about 436 KB
  of sample memory, and 20 KB for the DX7 voice banks and settings. Our song mode doesn't use the stock
  pattern projects, so songs and kits share one pool of about 504 KB, with the gauge showing how it's split.
  A song is roughly 2 KB of sounds and settings plus about 8 bytes per note, so a 1,000-note song is about
  10 KB. Fewer sample seconds means room for more songs, and the reverse. The song list has load, save,
  save as, new, rename, duplicate and delete, picked on K2 and done with YES (OCT+), with a question first
  when unsaved changes would go and always before a delete. The browser editor keeps an unlimited library on
  the computer.
- **New songs from factory sound sets.** The stock firmware's 16 factory projects (Acid Alley, Night Drive, Dub
  Harbour and so on) are 8-track sound setups. A new song can start from one of them, or from blank.
- **Sound editing.** EDIT enters sound editing. While there, FX, SEL, ENV and LFO go back to their stock jobs
  (effects, FM scaling, envelopes, LFO). Leaving restores the song-mode functions.
- **Mixer.** The stock overview page on HOME stays: volume per track on K1 to K4 (four tracks per page), mute
  and solo on the white keys, per-track reverb and delay sends, the shared reverb and delay, and the master
  knob. Pan gets added. The engine supports it, but the stock screens only use pan as an LFO target. The mix
  saves with the song, and volume, pan and sends can be automated.
- **Effects.** Stock: a shared reverb and a shared delay, a send to each per track, and each engine's own sound
  shaping (303 filter and drive, FM operators, PSG and pluck tone, sampler pad controls). Per-track insert
  effects aren't possible, because the closed engine mixes the tracks internally and our code only sees the
  finished stereo mix. Master-bus effects are possible in our own output stage (`app/audio_output.c`): a
  filter, EQ, compressor or lo-fi, automatable. That's a later option, once CPU headroom is measured on the
  device.
- **Tempo, time signature and sound changes mid-song** (agreed 10/27). A conductor lane in the automation view
  holds tempo and time-signature changes, much like the QY70's tempo track plus its per-measure meter. Move
  the playhead to a bar, then add, edit or delete a change there. Time signatures follow the QY70's range
  (1/16 to 16/16, 1/8 to 16/8, 1/4 to 8/4), up to 32 changes per song, each starting on a bar. Program
  (sound) changes are a lane on each track.
- **External MIDI keyboard.** Recording takes notes from an external keyboard with real velocity, which also
  covers the FM-1's keys probably not sensing it. MIDI clock out stays available so other gear can follow.
- **Memory gauge.** Song memory used is always visible, like the QY70's Used Memory display.

## 8d. Engineering notes

- Our code lives in its own files (`app/song/`), with small, marked hooks into stock files, so M-VAVE's
  updates stay easy to merge.
- The song format and player logic get unit tests that build and run natively on the Mac, alongside the
  browser preview.
- Before the first flash, the stock firmware and any projects are backed up.
- The firmware is GPL. If it's ever shared, its source goes with it.
- **Flash for the full device build** (Judah, 10/28): 8 more storage sectors (32 KB) go to code, about 1.4
  seconds of sample time out of about 19.6. The timing test settled the delivery method (B, section 2), so
  the hardware diagnostics page is out of the full build. The default test build keeps it.

## 8e. Glitch (agreed 10/27)

**0.3.0:** the live stutter and the FX lane are gone, and ARP on the song pages is the stock arpeggiator again
(a tap on or off, a hold for its settings). Per-note glitch in step entry and the mangle jobs stay. Song files
with FX lane events still load, and the events are dropped. What follows is the plan as agreed in 10/27.

**Per note, on any track (synths as well as drums):** rolls and ratchets (repeat a note 2, 3, 4, 6 or 8 times
inside its length, with optional pitch drift and velocity fade across the repeats), probability, random octave
jumps, and random pad swaps on drum tracks.

**Sequencer glitches (no audio memory).** The player schedules every note, so it can mangle the music itself:
- **Stutter:** while held, the player loops the last 1/4 to 1/64 of the song on every track. The synths really
  retrigger, which sounds different from audio stutter. A knob drifts each repeat's pitch up or down.
- **Mangle jobs on a selection:** scramble the 16ths, reverse the note order, shuffle notes between tracks.
  Destructive but undoable.

**Sound glitches through automation:** fast-stepped automation (303 cutoff, pitch-bend jumps, pan flicks) and
the sampler's variation setting.

**Master-bus effects** in our output stage (`app/audio_output.c`):
- Cheap, no buffer: bitcrush, sample-rate reduction, a tempo-synced gate or chop, and filter sweeps.
- Needs a short audio buffer: audio beat repeat, reverse of the last slice, tape stop. An 8th note at 120 BPM
  is about 22 KB in mono, so short slices fit and bar-long scrambles don't. Sized after measuring free memory
  on the device.

**Playing it.** Hold GLITCH (ARP in song view, tentatively). The lower keys pick a repeat length (1/4 down to
1/64, with triplets), the upper keys pick an effect (reverse, tape stop, crush, gate, filter), and K1 to K4
set pitch drift, decay, crush amount and filter. While recording, glitches go to an FX lane, so they become
part of the song and can be repeated, edited and undone.

**Responsibly:** a limiter on the master output catches level spikes, every glitch can be undone, and the
buffer effects stop as soon as the button is released.

## 8f. Clips (phase 7, agreed 10/28)

Judah, 10/28: patterns work like Ableton Live's session view, and the song holds references to clips, with an
Expand job to turn them into notes. This replaces the 8-track patterns proposed on 10/27.

**A clip** is one track's loop: notes and automation in the same event format as a song track, from 1 beat to
16 bars long. Step entry, loop recording, glitch, automation and the edit jobs all work inside a clip
unchanged. Each clip has its own length, so a 3-bar bass line can run against a 4-bar beat. Clips belong to the
song and share its event pool, sounds and conductor lane. The clipboard survives switching songs, so a clip
copies to another song on the device, and the workbench can copy clips between songs too.

**The grid.** SEQ switches between the song view and the clip grid: 8 tracks across, 16 rows down in two pages
of 8, and a SONG row at the top. Rows are numbered and can take a short name, like markers. A filled cell is a
clip, a playing cell shows its progress, and a waiting cell blinks. SELECT picks the track (the column), as
everywhere, and K1 moves the row cursor. Function buttons are labeled on screen, as in step mode.

The stock sync and MIDI pages that SEQ used to reach move to the device settings on SHIFT+GLO (8a).

| Control (grid) | Action |
|---|---|
| Lower white keys, F3 to F4 (the track keys) | Launch that track's cell in the cursor row. An empty cell stops the track. On the SONG row they stop the track (0.4.0). Until then they sent it back to the song |
| Upper white keys, G4 to G5 | Launch rows 1 to 8 of the page (Live's scenes). Every track with a clip in the row plays it, and the others stop |
| SEL | Open the clip at the cursor. An empty cell gets a new 1-bar clip |
| SHIFT + lower white keys | The song view's actions (6a) on the cell at the cursor: copy, cut, paste, delete |
| REC | On the SONG row, record launches into the song (below). On a row, record into the cell (0.3.0) |
| PLAY | Play and stop. Stopping leaves every track on what the view plays (0.4.0) |

SHIFT+SEQ, which handed every track back to the song, went in 0.4.0, when playback started following the
view.

Launches wait for the launch quantize, a song setting: off, beat, bar (the default), 2 bars or 4 bars.
Launch points count on the song's bar lines, as in Live, and the song loop's wrap counts as one too, so a loop
that ends mid-bar keeps launches in time (10/28). Launching while stopped starts playback. The track keys' LEDs show each track: lit while its clip plays,
blinking while a launch waits. A clip starts from its beginning and loops at its own length, and keeps its
phase across a song loop or a marker jump.

In the grid the keys launch instead of playing notes. SEQ flips back to the song view and the keys play
again.

**A track plays a clip or the song, never both** (Live's rule). Until 0.4.0 a launched clip took the track over
from the song until that track went back to the song, whatever the view, so a melody on the song could play
over drums on a clip. Since 0.4.0 the view decides (below): the grid plays only launched clips, and the song
view only the song, its blocks included.

**Editing a clip.** SEL opens it in the song view's editor, looped to the clip, with K3 (loop end) setting its
length. Everything there works on the clip: step entry, recording, automation, glitch and the jobs. SEQ goes
back to the grid. Pasting copied song bars into a cell turns a section already written into a clip, and several
copied tracks paste across the row onto their own tracks.

Until 0.4.0 the song kept running while a clip was open, and its tracks kept playing, so a bass clip could
be written against the song's drums. Since 0.4.0 the open clip plays with the grid's other launched clips
instead, so the drums are a launched clip. REC while playing records into the open clip from its next pass, as
loop recording, while everything else plays on. Recording in the song view still starts from a stop (Judah,
10/28, after 7a).

**Clips in the song are references** (Judah, 10/28). Each track gets a clip lane of blocks, such as "clip 3
from bar 9 to bar 17", drawn as numbered blocks on the track's row in the song view (in the track's full color
with their clip's notes since 0.4.0, the track's own notes not drawn under them). Inside a block the
clip loops from the block's start and the track's own notes are silent. Outside, the track plays its own notes.
A block can also be a stop, so a stop recorded in a performance stays silent on playback. Editing a clip
changes it everywhere it's used.
- **Recording a performance.** REC in the grid arms it (SHIFT+REC in 0.3.0, REC on the SONG row since 0.4.0),
  and the take starts with the next launch or PLAY.
  Every launch writes a block for its track, from the launch point to that track's next launch, stop or return
  to the song. Tracks left on the song are untouched, and the song's loop is ignored while recording. Since
  0.4.0 every track in the grid plays a clip or is silent, and what each played as the take began is written
  from there too, silence as a stop block, so the song gets what was heard. The take is one undo step.
- **Placing by hand.** A clip copied in the grid pastes into the song view as a block at the playhead, the
  clip's length long, and Duplicate extends it. Bar edits (copy, paste, insert and delete bars) carry blocks
  the way they carry notes.
- **Expand job** (the QY70's Expand Backing). It turns the blocks in the selection into plain notes on their
  tracks, so one spot can go its own way. MIDI export expands every block on its own.

**Storage.** Song files gain the clip table and the clip lanes, and older song files load with an empty grid.
Clip events come out of the song's shared event pool, and a block costs a few bytes.

**As built in 7b** (10/28):
- A block is an event on its track (song file version 3), so bar edits, undo and saving carry it like a note.
  It ends at its length or where the track's next block starts. A block placed over others removes those
  starting inside it and ends one reaching in where it starts. That one's tail isn't kept, since it would
  start its clip over.
- Inside a block the track's notes, controllers and program changes are silent. The clip's controllers are
  chased at the block's start and the track's own again at its end.
- A take ignores jumps too, and the song grows to hold it. REC again or stopping ends it, and since 0.3.0 PLAY
  ends it with playing going on.
- The grid's copy pastes as a block only into the song it came from, while that cell still holds the clip.
  Pasting selects the block, so Duplicate extends it, and blocks of one clip back to back join into one when
  the first is a whole number of passes.
- Expand works on the blocks starting in the job's range, on the picked tracks. Swing and random glitch are
  rolled where the notes now sit, the same as before when a block starts on a bar line.
- An open clip plays on its track like a launched one, in step with the song's bar when opened while playing.
  The editor works in the clip's time, a jump while playing moves the clip's pass at the next bar line, and
  closing gives the track back to what it played. REC again ends a recording into it and playing goes on.
  PLAY stopped both until 0.3.0, and now ends the recording too, playing on.
- MIDI export expands every block in the workbench, the only place export happens.

**Recording into a cell, as built in 0.3.0** (Judah, 10/28):
- REC in the grid records into the cell at the cursor, and the performance moves to SHIFT+REC. K2 is the REC
  length: 1, 2, 4, 8 or 16 bars (4 to start), or FREE. It lives in RAM, not the song.
- An empty cell becomes a clip of that length. Playing, the cell launches on its track at the launch quantize,
  or at the next beat with it off, and the take starts exactly there. From a stop, playing starts with the REC
  chooser's count-in. A set length ends the take at the clip's end, and the clip plays on, launched. FREE runs
  to 16 bars until REC, which ends it at the next bar line, its length.
- A cell with a clip is overdubbed as loop recording from its next pass, the open clip's path.
- While the take runs, the clip is open in the editor behind the grid, in the pass it was launched in. So the
  take is an open clip's take: the keys play its track, live, and land at the clip's clocks, and record
  quantize and the metronome's while-recording setting apply. Opening and closing don't break the pass: a clip
  playing as launched goes on unbroken through both. The take is one undo step, with a new clip's length.
- PLAY or REC ends any live take with playing going on, and a second PLAY stops. In a count-in, before
  anything is recorded, either calls the take off and stops. A take that fills the song's event memory ends
  there with a notice.
- SEQ, SHIFT+SEQ (until 0.4.0), the song list, undo and the grid's cell edits wait while a cell's take is armed
  or running.

**REC and playback, as built in 0.4.0** (Judah, 10/29):
- REC on the song view, the automation view and the grid tells a tap from a hold. A tap, let go within 600 ms,
  acts as it comes up: stopped in the song view a take in the song's mode with its count-in, in the grid a
  take into the cell at the cursor, and on the SONG row a performance armed. A hold of 600 ms opens the record
  settings (the REC chooser before), and HOME there goes back. A REC that ends or calls off something still
  acts as it goes down. SHIFT+REC is step entry: at the playhead, in an open clip, or in a grid cell's clip
  from its start, an empty cell first getting a clip of the REC length (4 bars for FREE). It starts only
  while stopped, as before. REC or PLAY in it goes back to the view it was opened from (0.4.1): from the
  grid with the clip closed, PLAY launching the clip and playing.
- The record settings list REPLACE, OVERDUB, PUNCH, LOOP and STEP. A live mode picked becomes the song's, and
  STEP never does. REC or PLAY there goes back and does what REC (a live mode) or SHIFT+REC (STEP) does
  there. New songs record in OVERDUB, and a file with STEP, the default before, loads as OVERDUB, so the song
  file format is unchanged.
- SHIFT+SEL switches REPLACE and OVERDUB in the song view, the clip editor and the grid, and those show the
  mode in red at the bottom line's right end. In the automation view SHIFT+SEL still clears lanes.
- In the grid, REPLACE empties a clip already there as REC is tapped, keeping its length, inside the take's
  undo step. PUNCH and LOOP act as OVERDUB there.
- Playback follows the view (player.h, song_player_view): each track's `home`, what it plays when not
  launched, is the song in the song view, the automation view and step entry, and nothing in the grid and a
  clip opened from it. Switching sends every track to the new home at the next launch point, at once when
  stopped. A clip launch waiting as the grid opens still happens. The grid's SONG row keys stop their track,
  and in the grid the transport goes round at the song's end. SEQ waits while a performance is armed or
  running.
- A performance keeps what each track played as its take began, so silent tracks get stop blocks and a clip
  already playing gets a block from the take's start. A block plays its clip from the top, so such a clip
  restarts there if it was partway through a pass.
- Blocks are drawn in the track's full color with the clip's number in black and its notes, and the track's
  own notes are left out inside them.
- The full build's song list shows the free heap, the diagnostics' probe run once per opening, stopped, to see
  whether SONG_EVENTS can grow from 4096 to 8192 (32 KB more).

**Later:** Live's follow actions, such as "after 4 bars, play a random clip from this column", next to the
glitch tools.

**Cost:** measured in the build. The full device build had about 10 KB of flash left after phase 6, so phase 7
gets measured against that before anything else is cut.

## 8g. Chord mode (phase 8, agreed 10/28)

Judah, 10/28: chords are a play setting on each instrument track, not the QY70's chord track. The QY approach
(a chord track, phrase types and chord following) is dropped.

**Playing.** With chord mode on, a track's keyboard splits in two:
- **Roots on the lower 12 keys,** F3 to E4. Each key plays a full chord on that root. The octave buttons move
  the roots.
- **Chord types on the next 12 keys,** F4 to E5. A press picks the type, and it stays picked, so chords play
  with one hand. Picking a type while a root is held changes the chord that's sounding. The same keys hold the
  types in step mode.
- **Types (proposed):** major, minor, 7, maj7, m7, sus2, sus4, dim, aug, 6, add9 and "in key". In key builds
  each root's chord from the song's key and scale (8a), so in C major the D key plays D minor. The common types
  sit on the white keys and the color ones on the black keys. The exact layout gets tuned on the device.
- Drum tracks ignore chord mode.

**Settings,** per track and saved with the song:
- Chord mode on or off.
- Voicing: close or spread, plus smooth, which picks the inversion nearest the last chord so changes move
  less.
- Strum length: off, or the time from the first note to the last, in note lengths from a 64th to an 8th so it
  follows the tempo.
- Strum direction: up, down, alternating (like a guitar's down and up strokes) or random.

**Recording.** A chord records as plain notes, strum offsets included, as played. Editing, the jobs, glitch and
MIDI export all work on it, and the player needs nothing new. Input quantize moves a strummed chord as a whole,
by its first note, so the strum survives. In step entry a root key enters the whole chord at the pointer,
strummed when strum is on.

**Timing.** A strum spreads its notes over a few milliseconds, so live strums go out through the same
block-accurate delivery as playback, not the control loop. Under method B the interrupt's wake runs while a
strum waits as well as while the song plays, and a strum's first notes go out as the chord is struck.

**Cost:** should be small (12 chord shapes and a strum scheduler), and gets measured against what phase 7 leaves.

**As built in phase 8** (10/28):
- **The type keys.** White keys: F4 in key, G4 major, A4 minor, B4 7, C5 m7, D5 maj7, E5 6. Black keys: F#4 sus2,
  G#4 sus4, A#4 dim, C#5 aug, D#5 add9. A new song starts on in key.
- **The top three keys** in real-time play: F5 plays the last chord again, F#5 an octave down and G5 an octave
  up, each strummed afresh. In step entry they enter those chords (since 10/28, when rest, repeat and tie moved
  to SHIFT).
- **In key** is the root and the second and fourth notes of the song's scale above it, whether the root is in
  the scale or not. So in C major D gives D minor and C# gives C# dim. A pentatonic or blues scale gives its own
  stacked shapes, and with no key set it's major.
- **Spread** lifts the close chord's second note an octave: root, fifth and tenth for a triad, and root, fifth,
  seventh and tenth for a 7.
- **Smooth** tries each inversion of the close chord, and each of those an octave down, and keeps the one whose
  notes sit nearest the last chord's, each note counted to the nearest one there. A tie goes to root position,
  and the first chord on a track is close.
- **The strum** spaces the notes evenly from the first to the last. Alternating starts up. Random draws each
  chord's order from the song's seed, so it repeats the same way from a load.
- **Playing.** One chord sounds at a time, while its key is held, at velocity 100, and a new root ends the last.
  Chord mode plays on the song pages. The stock pages play single notes as before.
- **The settings** are the fifth page of the song settings, beside the recording knobs and humanize, for the
  track SELECT picks. Like the recording settings they're preferences, not edits, so undo leaves them.
- **Recording.** A chord let go mid-strum keeps only the notes that sounded. Song file version 4 holds the
  settings.

## 9. Undo

Several levels, stored as what each edit added and removed rather than as whole copies of the song, so it
costs little memory. HOME plus SAVE stays undo, as in the stock firmware.

## 10. Phases

1. **Player and song format,** in the browser preview: play a test song on all 8 tracks from the internal
   clock, with tempo, markers and the loop region.
2. **Step entry,** with the full control map.
3. **Real-time recording and arranging:** modes, count-in, input quantize, undo, and selecting, copying,
   pasting and duplicating bars.
4. **Automation:** knob recording and playback, the conductor lane (tempo and time signature) and program
   changes.
4b. **Glitch, sequencer side:** rolls and ratchets on any track, probability, stutter, mangle jobs, the FX lane.
   (The stutter and the FX lane went in 0.3.0.)
   The master-bus effects follow once free memory is measured on the device.
5. **Saving and transfer:** songs saved to and loaded from flash, the song list, and export and import through
   the browser workbench (song files and MIDI files).
6. **Edit jobs:** quantize, transpose, copy, insert and delete bars, velocity and gate changes.
7. **Clips,** like Live's session view: the grid, launching, clip lanes in the song, recording a performance
   and the Expand job (8f).
8. **Chord mode** (8g): roots on the left keys, chord types on the right, voicing and strum.

Hardware checkpoints: the timing test is done and chose method B (section 2). Saving to flash comes with the
full build.

## 11. Polyphony

Tracks are polyphonic, so a step can hold a chord. Voices per track, by engine: FM 8, 303 4, 808 one per drum
(11), PSG 8, pluck 8, sampler 24. Any track can be set to mono. All tracks share one CPU budget, and the engine
trims voices on its own when it runs out.

## 12. Decisions so far (Judah, 10/27)

- SHIFT is a function button (FX, tentatively). Holding HOME was also fine, but one dedicated SHIFT is simpler
  to learn.
- Rest and tie go on keys, freeing the function buttons for SHIFT, delete, back-delete and select.
- Velocity is set mainly with a knob (K2).
- Notes can be selected and edited in place, with the pointer as the selection.
- Multi-level undo.
- Drum features: reverse and other sample prep in the browser, flams, rolls, probability and humanize in the
  sequencer. No time-stretching: samples are short one-shots.
- Copy, cut, paste and duplicate whole bars or marker sections, with the loop region as the selection.
- Songs export and import through the browser workbench, as our own song files and as standard MIDI files.
- Patterns work like Ableton Live's session view: clips per track, launched from a grid (10/28).
- Every button light stays on, on every page, so the printed labels can be read. Lights that showed state are
  gone, with no setting to bring them back. The key lights are unchanged (10/28).
- The button printed SEL is SEL on screen too. The stock code's identifier for it is still SCL (10/28).
- The song holds references to clips, and an Expand job turns them into notes (10/28).
- Chords are a play setting per track, roots on the left and types on the right, with strum. The QY70's
  chord track is dropped (10/28).
- OCT+ is YES and OCT- is NO on the song list, the edit jobs and while naming, after the QY70's +1/YES and
  -1/NO keys. The song list's actions move from the note keys to K2, one at a time, with the hint saying what
  YES will do. A question is a box over the page that stays until it is answered or cancelled. In the song
  view and step entry the two keys still move the octave (10/28).
- A song settings page with swing, key and scale. Out-of-scale notes snap up to the next scale note, and the
  pitch knob moves selected notes by scale step.

## 13. Still open

1. **Velocity sensing.** Judah guesses the keys don't sense velocity. That gets checked on the device. If they
   do, live playing records real velocity and K2 matters mainly in step entry.
2. **Which function button is SHIFT.** Settled by where they sit on the panel.
3. **Track count.** 8 tracks, one per sound slot. More tracks could share the 8 sounds later if that's useful.
