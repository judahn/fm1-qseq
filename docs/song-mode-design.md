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
| Tracks | 16 sequence tracks plus pattern, chord and tempo tracks | 8 tracks, one per FM-1 sound slot, plus tempo and marker tracks. Pattern and chord tracks come with pattern mode |
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
  hands each note over in the block before it is due. Checking that it keeps up while the screen redraws is
  the first hardware test.
- The stock step sequencer is not used. It stays as a fallback if that test fails.

## 3. The song

- **8 tracks.** Each plays one sound slot (FM, 303, 808, PSG, pluck or sampler), stored as a linear list of
  events across the whole song: notes (start, pitch, velocity, length) and controller changes (automation).
- **Tempo and time signature** changes at any position.
- **Markers:** up to 16 named positions. While stopped, a jump is immediate. While playing, it waits for the
  next bar line so the music stays in time.
- **Loop region:** start and end follow the current snap (section 6). It can be set from two markers.
  Recording inside a loop is how you build up a section, like a pattern, without leaving song mode.
- **Sounds:** each song stores its 8 sound setups and mix, so loading a song brings its sounds with it.
- **Reserved for later:** a pattern lane (which pattern plays at each bar) and a chord track, so pattern mode
  can slot in without changing existing songs.

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
| SCL | Delete the selected notes at the pointer |
| ENV | Back-delete: move back one length and delete there |
| LFO | SELECT: hold it and press keys to choose which notes at the pointer to edit |

**Rest and tie on keys.** The FM-1 has 27 keys, two more than the QY70's 25. In step mode the top three keys
stop playing notes, and entry uses the two octaves below them (F3 to E5, with the octave buttons for range):

| Key | Action |
|---|---|
| F5 | Rest: move on one length |
| F#5 | Repeat: enter the last note or chord again (quick hi-hat runs) |
| G5 | Tie: lengthen the last note by the current length. The length can be changed first, so a quarter plus an 8th tie works (QY700 behavior) |

**SHIFT plus keys (step mode):**

| Keys, lower octave | Action |
|---|---|
| F3 G3 A3 B3 C4 D4 E4 (white) | 64th, 32nd, 16th, 8th, quarter, half, whole |
| F#3 | Triplet on or off |
| G#3 | Dot, double dot, off |

Shorter lengths sit on the left, longer on the right, so lengths read like the keyboard. The other black keys
stay free for later.

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
- **Input quantize:** off, or any length above, including triplets, with a strength setting. Recorded notes
  are stored already quantized. The original timing is kept until the next pass, so quantize can be undone.
- **Quantize job afterwards,** as on the QY70: grid, strength 0 to 100%, swing, over a range of bars on one
  track or all.

## 6. Navigation and playback (song view)

| Control | Action |
|---|---|
| PLAY | Play and stop |
| SHIFT + PLAY | Back to the start, or to the loop start when looping |
| REC | Arm recording. The mode is set on screen |
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
| SCL | Add a marker at the playhead |
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
(ALGO turns to the next three, SHIFT moves a setting by ten), and SAVE runs it after asking once, as the QY's
ENTER. The question says how many events the job takes on, and warns when it is too big to undo. HOME or EDIT
goes back, PLAY plays, and HOME+SAVE undoes as everywhere. Every job is one undo step.

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
| Clear track | Every event of the tracks. The FX lane stays |
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
device settings (MIDI, clock, Bluetooth), which is the stock firmware's global page, kept as is.

**Song settings:**
- Tempo and time signature (the starting values, since changes can happen anywhere in the song)
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
Its SCL button is the FM engine's keyboard level scaling, and its per-track tuning tables are for microtonal
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

- **Saving.** SAVE writes the song to flash. A recovery copy is also written automatically whenever playback
  stops, since it's a battery device and power-off would lose unsaved work. Flash writes briefly mute audio,
  so nothing is ever written during playback.
- **Several songs on the device.** The user flash is about 524 KB: 68 KB of stock project slots, about 436 KB
  of sample memory, and 20 KB for the DX7 voice banks and settings. Our song mode doesn't use the stock
  pattern projects, so songs and kits share one pool of about 504 KB, with the gauge showing how it's split.
  A song is roughly 2 KB of sounds and settings plus about 8 bytes per note, so a 1,000-note song is about
  10 KB. Fewer sample seconds means room for more songs, and the reverse. The song list has new, save,
  rename, duplicate and delete. The browser editor keeps an unlimited library on the computer.
- **New songs from factory sound sets.** The stock firmware's 16 factory projects (Acid Alley, Night Drive, Dub
  Harbour and so on) are 8-track sound setups. A new song can start from one of them, or from blank.
- **Sound editing.** EDIT enters sound editing. While there, FX, SCL, ENV and LFO go back to their stock jobs
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

## 8e. Glitch (agreed 10/27)

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

## 8f. Pattern mode and the chord track (phase 7 proposal, for Judah's review)

**Patterns.** A pattern is a looping clip, 1 to 8 bars in any time signature, with 8 tracks (one per sound
slot). It uses the same event format as song tracks, so step entry, loop recording, glitch, automation and the
edit jobs all work inside a pattern unchanged.
- **Pattern mode:** a pattern view that looks like the song view with the loop set to the pattern. While
  playing, the lower white keys queue the next pattern, which starts when the current one ends (the QY70's
  section keys, and how most grooveboxes switch).
- **Where patterns live:** inside the song, up to 16 per song, numbered and optionally named. The browser
  editor can copy patterns between songs, so a library builds up without a separate on-device style store.
- **In the song:** a pattern lane, like the QY70's pattern track, says which pattern plays from each bar.
  Pattern tracks and free-form tracks share the 8 sound slots and both play, so you can lay a free-form
  melody over a pattern groove. Recording while switching patterns live writes the pattern lane. An "expand
  patterns" job turns the lane into ordinary notes for detailed editing (the QY70's Expand Backing).

**Chord track.** A song-level lane, alongside the conductor lane: at most one chord per beat, with an
optional 8th or 16th anticipation and a slash bass, using the QY70's 25 chord types plus root-only.
- **Entry:** the lower 12 keys pick the root, and the upper 12 keys pick the type, with a second press for
  the key's second type, as on the QY70. SHIFT plus a key sets the slash bass, and a knob sets anticipation.
  Entering a chord can play it on a chosen track, and the key LEDs light its notes.
- **Following chords:** each pattern track has a phrase type and the pattern has a source chord (C major by
  default). Bypass is for drums and never changes. Parallel moves everything by the root difference. Chord 1
  maps each note from its role in the source chord (root, third, fifth, seventh, or a scale step between) to
  the same role in the new chord. Chord 2 does the same but picks the inversion closest to the previous
  voicing, for smooth changes. Bass works like Chord 1, but the first note after a change is the root, or the
  slash note. Low and high limits per track fold notes by octaves to stay in range. The QY70 manual only
  describes these in prose, so the exact tables are ours, tuned by ear.
- **Free-form tracks** ignore the chord track by default. A per-track "follow chords" switch could come later.

**Cost:** about 15 to 25 KB more on the device, so it depends on the flash decision like everything since
phase 1.

**Questions for Judah:**
1. Patterns inside each song (proposed), or a global library like the QY70's styles?
2. Numbered patterns 1 to 16 with optional names (proposed), or QY-style named sections (Intro, Main A,
   Main B, Fill, Ending)?
3. Pattern tracks and free-form tracks both playing on the shared sound slots (proposed), or a pattern
   silencing the song's own notes on its tracks while it plays?
4. Free-form tracks following the chord track: later and off by default (proposed), or part of phase 7?

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
   The master-bus effects follow once free memory is measured on the device.
5. **Saving and transfer:** songs saved to and loaded from flash, the song list, and export and import through
   the browser workbench (song files and MIDI files).
6. **Edit jobs:** quantize, transpose, copy, insert and delete bars, velocity and gate changes.
7. **Pattern mode and the chord track.**

Hardware checkpoints when the FM-1 arrives: the timing test, then saving to flash.

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
- A song settings page with swing, key and scale. Out-of-scale notes snap up to the next scale note, and the
  pitch knob moves selected notes by scale step.

## 13. Still open

1. **Velocity sensing.** Judah guesses the keys don't sense velocity. That gets checked on the device. If they
   do, live playing records real velocity and K2 matters mainly in step entry.
2. **Which function button is SHIFT.** Settled by where they sit on the panel.
3. **Track count.** 8 tracks, one per sound slot. More tracks could share the 8 sounds later if that's useful.
