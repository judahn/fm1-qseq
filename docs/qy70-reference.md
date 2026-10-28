# Yamaha QY70 workflow reference

A description of how the Yamaha QY70 (1997) works, written to guide custom firmware for the M-VAVE FM-1
(27 keys, 14 buttons, 7 encoders, 240x240 screen) that should behave as much like the QY70 as possible.
Priorities, in order: song mode and step recording, then real-time recording with quantize, then pattern mode.

## Sources and how they are cited

| Cite | Document | Notes |
|---|---|---|
| `p. N` | QY70 Owner's Manual, English, 252 PDF pages | Printed page N. The PDF page is N + 2. |
| `LB p. N` | QY70 List Book (voices, styles, phrases, chord types, templates, MIDI data format) | Printed page equals PDF page. |
| `DF p. N` | QY Data Filer manual for the QY70 | Printed page N. The PDF page is N + 2. |
| `QY100 p. N` | QY100 Owner's Manual | Comparison only. |
| `QY700 p. N` | QY700 Owner's Manual | Comparison only. The PDF page is N + 1. |

All five come from Yamaha's manual library (usa.yamaha.com/support/manuals):

- https://usa.yamaha.com/files/download/other_assets/8/318058/QY70E1.PDF (Owner's Manual)
- https://usa.yamaha.com/files/download/other_assets/6/317936/QY70E2.PDF (List Book)
- https://usa.yamaha.com/files/download/other_assets/7/318057/QY70E3.PDF (Data Filer)
- https://usa.yamaha.com/files/download/other_assets/8/318568/QY100E1.pdf (QY100)
- https://usa.yamaha.com/files/download/other_assets/0/327080/QY700E1.pdf (QY700)

Everything stated about the QY70 comes from the QY70 documents. QY100 and QY700 material is labelled as a
comparison. Items marked **Not in the manual** are gaps in the documentation, not facts about the machine.
Values marked *derived* are computed from documented numbers (for example clock counts from 480 per quarter).

---

## 1. Summary: what most shapes a reimplementation

1. **Resolution is 480 clocks per quarter note** (p. 86, p. 229). Positions read `measure:beat:clock`, with
   measures 001 to 999 and clocks 000 to 479 within a quarter-note beat (p. 87, p. 196). The step-record grid
   draws one dash per 32nd note, which is 60 clocks (p. 85).
2. **Step record has exactly nine note lengths**: 32nd, 16th, 16th triplet, 8th, 8th triplet, quarter,
   quarter triplet, half, whole (p. 84). There is no 64th, no dotted value and no half-note triplet. A dotted
   value is built with `tie`, which adds one more step of the current length (p. 86), or fixed afterwards in
   the event editor. Gate is one of three presets, 50%, 90% or 99% of the step (p. 84). Velocity is one of ten
   fixed levels or one of four random settings (p. 84).
3. **The QY70 has no key shortcut for note length.** STEP, VELO and GATE are fields on the step-record
   screen. You move the cursor to one and change it with [-1]/[+1] (p. 83). The four function keys are fixed
   to `del`, `Bdl` (back delete), `rst` (rest) and `tie`, top to bottom (p. 86). For a SHIFT-plus-key scheme
   there are two Yamaha precedents:
   - The QY70's own SHIFT idiom is SHIFT plus one of the ten numbered black keys to type a digit (p. 34).
   - The QY700 puts note values on its numeric keypad (QY700 p. 122): `1` whole, `2` half, `3` quarter,
     `4` 8th, `5` 16th, `6` 32nd, `7` quarter triplet, `8` 8th triplet, `9` 16th triplet, `0` adds a dot
     (press twice for a double dot). It also puts velocity on the keypad (QY700 p. 123). See 3.10.
4. **Quantize is an edit job, applied after recording** (Song Job 01, p. 104 to 106). It offers nine grids,
   Strength 0 to 100%, Swing Rate and a swing Gate Time, over a measure:beat range on one track or all 16. A
   second, non-destructive quantize runs at playback through the Groove Play Effect, per track, with 100
   templates of which 01 to 09 are plain grids (p. 96 to 98, LB p. 38). The Normalize job writes it into the
   data (p. 133). There is no input quantize and no punch-in mode.
5. **A song is 16 sequencer tracks plus three control tracks** (p. 229): `Pt` (one style and section per
   measure, p. 55), `Cd` (at most one chord per quarter note, with optional 8th or 16th anticipation and a
   slash bass, p. 57 to 59) and `Tm` (tempo, p. 50). A song runs up to 999 measures (p. 87). A style's pattern
   has 8 tracks (D1, D2, PC, BA, C1 to C4) that follow the chord track through five phrase types (p. 147).
   Expand Backing renders Pt and Cd into ordinary note data on tracks 9 to 16 (p. 132).
6. **One level of Undo/Redo** covers the last job, edit or recording pass (p. 103). SHIFT plus the bottom
   function key is the shortcut (p. 26).
7. **The QY70 keyboard is 25 keys, E to E** (p. 230). The lower octave doubles as chord roots and pattern
   section selectors, the upper octave as chord types, and the ten black keys as digits with SHIFT (p. 23,
   p. 34, p. 40). The FM-1 has two more keys than that.

---

## 2. The physical controls

### 2.1 Panel (p. 23 to 30, p. 230)

| Control | What it does |
|---|---|
| [SONG] | Selects SONG mode. Further presses cycle SONG, SONG VOICE, SONG EFFECT. With SHIFT the cycle runs backwards (p. 23 to 24). |
| [PATT] | The same for PATTERN, PATTERN VOICE, PATTERN EFFECT (p. 24). |
| [SHIFT] | Modifier for digit entry and the combinations in 2.3 (p. 24). |
| [EXIT] | Back up one level in the display hierarchy (p. 25). |
| Cursor keys (4) | Move the highlight between fields (p. 25). |
| [-1/NO] [+1/YES] | Decrease or increase the field at the cursor. Hold to repeat. Holding one and pressing the other repeats faster. They also answer yes or no prompts (p. 25). |
| [MENU] and four function keys | MENU lists the sub-modes and functions of the current mode. The function keys beside the LCD pick an item. On some screens the function keys carry fixed actions labelled beside them (p. 26). |
| [ENTER] | Commits a chord, a menu choice, a typed number or an edited event line (p. 27, p. 34, p. 196). |
| Sequencer keys | [REC] (record ready, red LED), [STOP], [PLAY] (green LED flashes at tempo), [TOP] (to measure 1), [REW] (back one measure, hold to scroll), [FF] (forward one measure, hold to scroll) (p. 28). |
| Micro keyboard | 25 keys, two octaves from E to E, polyphonic, no velocity sensing (p. 28, p. 230). |
| [OCT DOWN] [OCT UP] | Shift the keyboard by octaves across an 8-octave range. Both together reset to 0. Arrows at the top left of the LCD show the shift (p. 29). |
| LCD | 128 x 64 dots (p. 230). |

[OCT DOWN] is also printed BASS and [OCT UP] carries a syncopation note symbol, for chord entry (p. 29,
p. 58 to 59). Held while a keyboard note sounds, [OCT DOWN] bends the pitch up and [OCT UP] adds modulation
(p. 29, p. 238 to 239).

### 2.2 The micro keyboard legends (p. 23, p. 34 to 35, p. 40, p. 203)

Each key has several printed roles. Which one applies depends on the screen and the cursor position. Chord
type keys toggle between their two printed types on repeated presses (p. 40). The G7 example on p. 40 takes
a single press of the `7(9)/7` key to get `7`, so the lower label comes first (*inferred*).

| Key | Chord entry (root, or type: 1st press / 2nd press) | Section (pattern select) | With SHIFT |
|---|---|---|---|
| E (lowest) | root E | INTRO | sign +/- before a number (p. 35, p. 89) |
| F | root F | MAIN A | |
| F#/Gb | root F# | | digit 1 |
| G | root G | MAIN B | |
| G#/Ab | root G# | | digit 2 |
| A | root A | FILL AB | |
| A#/Bb | root A# | | digit 3 |
| B | root B | FILL BA | |
| C | root C | ENDING | |
| C#/Db | root C# | | digit 4 |
| D | root D | BLANK | |
| D#/Eb | root D# | | digit 5 |
| E | M / add9 | | |
| F | M7 / M7(9) | | |
| F# | 7(b9) / 7(#9) | | digit 6 |
| G | 6 / aug | | |
| G# | 7(13) / 7(b13) | | digit 7 |
| A | 7 / 7(9) | | |
| A# | 7(#5) / 7(#11) | | digit 8 |
| B | m / madd9 (printed "add9" for space, LB p. 35) | | |
| C | m7 / m7(9) | | |
| C# | sus4 / 7sus4 | | digit 9 |
| D | m6 / mM7 | | |
| D# | dim / --- (non-ABC) | | digit 0 |
| E (top) | m7(b5) / m7(11) | | |

In SysEx event entry, SHIFT plus the lower white keys E, F, A, B, C, D (G is skipped) types the hex digits A
to F (p. 203). The order of that mapping is *inferred* from the key order.

### 2.3 SHIFT and other combinations

| Combination | Effect | Cite |
|---|---|---|
| SHIFT + black keys, then [ENTER] | Type a number. It flashes until ENTER. | p. 34, p. 89 |
| SHIFT + lowest E | Toggle the sign before typing a number | p. 35, p. 89 |
| SHIFT + [SONG] or [PATT] | Cycle that mode's displays backwards | p. 24 |
| SHIFT + top function key | Show which drum sound is on each key at the current octave | p. 26 |
| SHIFT + second function key | Used Memory display | p. 26 |
| SHIFT + bottom function key | Undo/Redo the last operation | p. 26 |
| SHIFT + [PLAY] | Repeat playback until STOP | p. 90 |
| SHIFT + [STOP] | Stop Jump mode | p. 91 |
| SHIFT + [TOP] | Top Jump mode | p. 91 |
| SHIFT + [REW] or [FF] | Jump off | p. 91 |
| SHIFT + [+1] on a track's M/S field | Add a track to the solo set. Hold SHIFT when un-soloing one of several. | p. 92 |
| SHIFT + [-1]/[+1] on a voice field | Jump to the first voice of the previous or next category | p. 63 |
| SHIFT + note while realtime-recording a pattern phrase | Erase that note wherever the loop passes while held | p. 149 |
| [OCT DOWN] + [OCT UP] | Reset the octave shift | p. 29 |
| Hold [OCT DOWN] + key, on a chord | Set the slash bass note | p. 58 |
| [OCT UP] once or twice, on a chord | Anticipate the chord by an 8th or a 16th | p. 59 |
| Section key, with the cursor on the style number or section name | Change section at the next measure | p. 37 |

---

## 3. Step recording

### 3.1 Entering step record on a song track (p. 81 to 83, p. 87)

1. In SONG mode, select the song.
2. Move the cursor to the track strip and pick track 1 to 16. The strip shows 8 tracks at a time and scrolls.
3. Press [REC]. The red LED lights and the record mode selector appears (REPL, OVER, STEP, MULTI).
4. Move the cursor to STEP and press [+1/YES]. An inverse `R` marks the chosen mode.
5. Press [PLAY]. The step-record screen appears.
6. Set STEP, VELO and GATE, then play notes.
7. Press [STOP] to leave step record. The normal SONG play screen returns.

### 3.2 The step-record screen (p. 83 to 85)

- Header: `REC STEP` and the track, as `TR=01`.
- Time signature, and `MEAS` showing the pointer position as `measure:beat:clock`.
- Three note fields: STEP (note length), VELO (velocity), GATE (gate time).
- The note display, covering up to four quarter notes. The upper row holds beats 1 and 2, the lower row
  beats 3 and 4. Each dash is a 32nd note (60 clocks), so 8 dashes make a beat. A dot marks where a note or
  chord starts, and a chord shows as a single dot. The pointer is a small triangle that points up at the
  upper row and down at the lower row. A measure longer than four quarter notes scrolls.
- Function key labels, top to bottom: `del`, `Bdl`, `rst`, `tie`.

### 3.3 The note fields (p. 84)

**STEP**, in the order the manual lists it. Clock counts are *derived* from 480 per quarter.

| # | STEP value | Clocks |
|---|---|---|
| 1 | 32nd | 60 |
| 2 | 16th | 120 |
| 3 | 16th triplet | 80 |
| 4 | 8th | 240 |
| 5 | 8th triplet | 160 |
| 6 | quarter | 480 |
| 7 | quarter triplet | 320 |
| 8 | half | 960 |
| 9 | whole | 1920 |

**Not in the manual**: whether [+1] walks the list in exactly this order. The manual's list order is the only
evidence. The Quantize job lists its grids in a different order (finest first, triplets interleaved, p. 104).

**VELO**: ten levels shown as bars, named ppp, pp, p, mp, normal, mf, f, ff, fff, ffff, plus four random
settings R1 (least variation) to R4 (most) (p. 84, p. 77). The MIDI values behind the levels are **not in the
manual**. For comparison, the QY700 uses ppp 8, pp 24, p 40, mp 56, (unnamed) 64, mf 72, f 88, ff 104,
fff 120 (QY700 p. 123). An external MIDI keyboard's velocity always overrides VELO in step record (p. 84).

**GATE**: three settings shown as bar icons. Staccato is 50% of the step, normal 90%, slur 99% (p. 84). A
normal quarter is therefore 432 clocks, which p. 196 confirms as the standard quarter-note gate.

### 3.4 Entering notes and chords (p. 28, p. 84)

Play a key on the micro keyboard or an external MIDI keyboard. A note with the current STEP, VELO and GATE
goes in at the pointer, a dot appears and the pointer advances by one STEP. The manual sets the fields and
then plays, with no step to move the cursor away first, so notes can be entered while the cursor sits on a
field (p. 83 to 84, *inferred*). The keyboard is
polyphonic, so a chord goes in by playing several keys together, and it shows as one dot. Use [OCT DOWN] and
[OCT UP] to reach other octaves (p. 29).

**Not in the manual**: whether a note registers on key press or on release, and how the QY70 decides that
several keys form one chord. For comparison, the QY700 registers notes on release, so a chord can be built
key by key and released together (QY700 p. 120).

### 3.5 Moving the pointer (p. 54, p. 85 to 86)

- Move the cursor down into the note display. The pointer turns solid and moves with the cursor keys.
  Landing on an existing note plays it. Moving past the end of the display shows the next measure (p. 85).
- Put the cursor on the measure in `MEAS` and set it with [-1]/[+1] or SHIFT digits to jump the pointer to
  any measure (p. 86).
- [TOP] goes to measure 1, [REW] and [FF] move one measure. The manual states this for every step-record
  display in its section on Pt and Cd step record (p. 54).
- `rst` advances the pointer by a rest, and `Bdl` moves it back by one STEP (p. 86).

**Not in the manual**: how far one cursor-key press moves the pointer inside the note display (one 32nd
dash, or one STEP).

### 3.6 Rests, ties and deleting (p. 86, p. 103)

| Function key | Label | Action |
|---|---|---|
| top | `del` | Delete the note or chord at the pointer. |
| 2nd | `Bdl` | Move the pointer back by the current STEP and delete the note there. |
| 3rd | `rst` | Enter a rest of the current STEP length at the pointer, which advances it. |
| bottom | `tie` | Lengthen the note just entered by another note of the STEP length and the same pitch. It works only immediately after entering a note. |

- **Dotted and odd lengths**: with STEP at 8th, enter a note and press `tie` twice for a dotted quarter
  (3 x 240 clocks, *derived*). The manual does not say whether STEP may be changed between the note and the
  tie. The QY700 allows it, so a quarter plus an 8th tie is possible there (QY700 p. 124).
- **Undo**: one level, as Job 00 or SHIFT plus the bottom function key (p. 103, p. 26). Anything else is
  fixed in the event list editor (6.8).
- **Adding to existing data**: step record can add to or tidy a track recorded in real time (p. 81). The
  manual does not say whether a new note at an occupied position replaces or joins the old one.

### 3.7 Exact control map during song step record

| Control | Effect |
|---|---|
| Micro keyboard (or MIDI in) | Enter a note or chord at the pointer and advance by STEP |
| [OCT DOWN] / [OCT UP] | Octave of entered notes |
| Cursor keys | Move between MEAS, STEP, VELO, GATE and the note display. In the note display, move the pointer. |
| [-1] / [+1] | Change MEAS, STEP, VELO or GATE |
| SHIFT + black keys, [ENTER] | Type a measure number into MEAS |
| Function keys, top to bottom | `del`, `Bdl`, `rst`, `tie` |
| [TOP] / [REW] / [FF] | Measure 1, previous measure, next measure |
| [STOP] | Leave step record |
| SHIFT + bottom function key | Undo/Redo (general shortcut, p. 26) |
| [PLAY], [REC], [MENU], [EXIT] | **Not in the manual** for note step record |

### 3.8 Step recording a pattern phrase (p. 144 to 150)

The same as song step record (p. 150), with these differences:

- Choose a user style (U01 to U64) and section, set the source chord, pick the track and its user phrase
  (p. 144 to 145), then press [REC]. The pattern record selector offers only OVER and STEP (p. 146).
- Set the phrase TYPE and voice before recording (p. 146 to 147). See section 8.
- Set the pattern length (1 to 8 measures) and time signature on an empty pattern (p. 148).
- `MEAS` shows the pattern's total number of measures as well as the current one (p. 150).
- The `cler` function key clears the user phrase from the track. Selecting the user phrase again recalls it.
  The Clear Track job deletes it for good (p. 150).

### 3.9 Step recording the Pt and Cd tracks (p. 51 to 60)

Select `Pt` or `Cd` in the track strip, press [REC], choose STEP, press [PLAY]. While in step record, [MENU]
switches between the Pt and Cd screens (p. 53). [STOP] leaves (p. 53).

**Pt screen** (p. 54 to 56): one row per measure, with METER, STYLE and SECTION columns. The cursor keys move
between columns and scroll through measures past the top or bottom of the screen. [-1]/[+1] set values.
[TOP], [REW] and [FF] move through the song. The `ers` function key erases the style or section at the
cursor. Rules are in 6.3.

**Cd screen** (p. 57 to 60): one row per measure, divided into quarter-note cells. Put the cursor on a cell
and enter a chord as root plus type on the keyboard (p. 40, p. 57), committed with [ENTER] (p. 41). Then:

- Hold [OCT DOWN] (BASS) and press a key to set the slash bass. This also works on an existing chord.
  Re-entering the chord removes the bass (p. 58 to 59).
- Press [OCT UP] once to start the chord an 8th early, twice for a 16th. Pressing again until the symbol
  disappears removes it (p. 59 to 60).
- `ers` erases the chord at the cursor (p. 60).
- [MENU] then `Sound` turns chord audition on or off (p. 60).
- With FNGR on, play the chord shape instead. A key below the fingered zone while holding the chord sets the
  bass (p. 57, p. 59, p. 227).

### 3.10 Comparison: the QY700's step-entry keys (QY700 p. 120 to 124)

The QY700 is the QY with a key-per-length scheme. It is the closest precedent for SHIFT-plus-key note lengths.

| Key | Step time (QY700 p. 122) | Velocity (QY700 p. 123) |
|---|---|---|
| 1 | whole (1920) | ppp (8) |
| 2 | half (960) | pp (24) |
| 3 | quarter (480) | p (40) |
| 4 | 8th (240) | mp (56) |
| 5 | 16th (120) | (64) |
| 6 | 32nd (60) | mf (72) |
| 7 | quarter triplet (320) | f (88) |
| 8 | 8th triplet (160) | ff (104) |
| 9 | 16th triplet (80) | fff (120) |
| 0 | dot (twice: double dot) | EXT (use keyboard velocity) |

Other QY700 differences: step time can also be typed as any clock count from 1 to 9999, gate can be typed as
1% to 120%, REST is the [MUTE] key, TIE is the [SOLO] key and may use a different length from the note,
`BkDelete` and `Delete` sit on F5 and F6, and notes register on key release (QY700 p. 120 to 124). The
QY700's keypad works on the field that the cursor is on (D2 for step time, D3 for velocity), so it is
keys-per-value rather than a modifier chord. The QY100's step record matches the QY70's exactly (QY100
p. 91 to 96).

---

## 4. Real-time recording

### 4.1 Record modes

| Mode | Applies to | What it does | Cite |
|---|---|---|---|
| REPL (replace) | Song tracks 1-16, Pt, Cd, Tm | Records what is played, replacing what was there | p. 6, p. 78, p. 240 |
| OVER (overdub) | Song tracks 1-16, pattern phrases | Adds to what is there | p. 78, p. 146 |
| STEP | Song tracks 1-16, Pt, Cd, pattern phrases | Section 3 | p. 82, p. 51, p. 150 |
| MULTI | Song | Records MIDI channels 1 to 16 onto tracks 1 to 16 at once | p. 80 |

The Tm track takes realtime replace only (p. 50). PATTERN mode offers only OVER and STEP (p. 146). The manual
shows Pt and Cd with REPL and STEP and does not say whether OVER applies to them.

### 4.2 Procedure for a song track (p. 76 to 80)

Select the song and track, press [REC], choose REPL or OVER, set tempo and time signature, press [PLAY], play,
press [STOP]. Recording ends and play mode returns. A recorded track shows the lowest segment of its velocity
meter while stopped (p. 79). Before recording, assign the track's voice in SONG VOICE (p. 75).

### 4.3 Count-in and click (p. 46, p. 79, p. 220 to 221)

- Count-in defaults to one measure and is set in Utility, `REC COUNT`: OFF or 1 to 8 measures. During the
  count-in the measure display shows negative numbers (with the default one measure, `-01`), and recording
  begins at measure 001 (p. 46).
- `CLICK MODE`: Off, Record (default, click while realtime recording), Rec/Play, Always (p. 220).
- `CLICK BEAT`: 16, 8, 4 (default), 2 or 1, meaning a click every 16th, 8th, quarter, half or whole (p. 220).

### 4.4 Where recording starts and what it does at the end

- The measure field on the SONG screen sets where playback or recording starts (p. 90).
- Song recording runs until [STOP]. It does not loop.
- Pattern phrase overdub loops the pattern from first to last measure, so notes can be added pass after pass
  (p. 148).
- **Not in the manual**: whether REPL erases anything beyond the point where [STOP] is pressed.

### 4.5 Velocity and expression from the micro keyboard (p. 29, p. 77, p. 93)

The keyboard has no velocity sensing. The `VELO` field on the SONG screen sets its velocity: ten levels or the
random settings R1 to R4. An external keyboard sends its own velocity. Holding [OCT DOWN] while a note sounds
bends it up, holding [OCT UP] adds modulation.

### 4.6 Erasing while recording (p. 149)

During pattern phrase overdub, hold SHIFT and play a note to erase every matching note the loop passes while
the key is held. The song chapter does not describe this for song tracks.

### 4.7 Punch-in and input quantize

Neither exists on the QY70. The specifications list only realtime replace, realtime overdub, step and multi
(p. 229), and quantize is a job run afterwards (section 5). For comparison, the QY700 adds a PUNCH realtime
mode (QY700 p. 118). Its quantize is also a job.

### 4.8 Real-time Pt, Cd and Tm recording (p. 44 to 50)

- **Pt**: on [PLAY] the cursor jumps to the style number. Pick styles with [-1]/[+1] or SHIFT digits and
  sections with the section keys. Every change takes effect at the top of the next measure. INTRO then plays
  MAIN A, FILL AB then MAIN B, FILL BA then MAIN A, automatically. End with ENDING or style 129 (the end
  pattern), otherwise the accompaniment plays on forever (p. 47).
- **Cd**: on [PLAY] the cursor jumps to the chord. Enter root, type and [ENTER], or play the shape with FNGR
  on (no ENTER needed). Chord changes land on quarter-note divisions, so press ENTER on or just before the
  beat. Anticipated chords are only possible in step record (p. 48 to 49).
- **Tm**: REPL only. On [PLAY] the cursor jumps to the tempo. Change it with [-1]/[+1] or SHIFT digits in
  time. Record the starting tempo at the top of the song first (p. 50).
- **MULTI**: set MIDI SYNC to External, MIDI CONTROL to In or In/Out, REC COUNT to OFF, arm, select MULTI,
  then start the external sequencer (p. 80).

---

## 5. Quantize

### 5.1 The Quantize job (Song Job 01, p. 104 to 106. Pattern Job 01, p. 160)

From SONG play: [MENU], `Job`, select `01 Quantize`, [ENTER] (p. 101). Set the fields and press [ENTER] to
run it. "Completed" appears. Job 00 undoes it (p. 103).

| Field | Values |
|---|---|
| TR | 01 to 16, or All (song). D1, D2, PC, BA, C1 to C4 (pattern). |
| M range | First measure:beat to last measure:beat. The example screen shows `M001:1 ~ 999:4`. |
| Quantize | 32nd, 16th triplet, 16th, 8th triplet, 8th, quarter triplet, quarter, 16th + 16th triplet, 8th + 8th triplet |
| Strength | 0% (no change) to 100% (exactly on the grid). 50% moves an off-grid note halfway. |
| Swing Rate | Delays the even-numbered grid points. 50% to 75% for straight grids, 66% to 83% for triplet grids, 50% to 66% for the mixed grids. |
| Gate Time | 0% to 200%. Scales the gate times of the notes the swing moves. 100% keeps their original relation. |

Swing details (p. 105): with an 8th grid, the 2nd, 4th, 6th and 8th 8ths of the measure are delayed. With a
triplet grid, the last note of each triplet group moves. With a mixed grid, the even-numbered back beats move.
The example screen shows Strength 100%, Swing Rate 50%, Gate Time 100% (p. 104).

### 5.2 Groove quantize, a Play Effect (p. 94 to 98, LB p. 38)

From SONG play: [MENU], `Play Fx`, then [MENU], `Groove`. Play Effects change playback only and are not
written to the data. They are stored per song (p. 94) and per user style (p. 155). Per track:

| Field | Values |
|---|---|
| M/S | Mute or solo, the same switches as the SONG screen |
| FX THRU | Bypass the Play Effects for the track |
| GROOVE TEMPLT | `--` (none) or 1 to 100 |
| TIMING | 000 to 200, default 100. How far the template moves note timing. |
| VELOC | 000 to 200, default 100. How far the template changes velocity. |

Templates 01 to 09 are plain grids: 32Quantize, 24Quantize, 16Quantize, 16+24Quant, 12Quantize, 08Quantize,
08+12Quant, 06Quantize, 04Quantize (LB p. 38). The numbers count divisions of a whole note, so 24 is the 16th
triplet (*inferred*). Templates 10 to 100 are named feels such as 16 Swing, 08 Push2&4 or 12Shfflin1.

Job 21 Normalize writes the Play Effects into the data and resets them (p. 133). Drum Table remapping is the
other Play Effect (p. 98 to 100).

---

## 6. Song mode

### 6.1 Songs and the SONG screen (p. 88 to 93)

There are 20 user songs plus three demo songs, numbers 21 to 23 (p. 89). The SONG screen shows (p. 88):
song number and name, measure number, octave indicator, tempo, time signature, `VELO`, `TRNS` (transpose),
`FNGR` (fingered chord on or off), `JUMP` mode and measure, the track strip (Pt, Cd, Tm and 8 of the 16
sequencer tracks) with a 5-segment velocity meter and M/S switch under each, style number and name, section
name, the chord, and the chord template (`USR` or a preset number).

- [PLAY] plays to the end and stops. SHIFT + [PLAY] repeats until [STOP] (p. 89 to 90).
- Selecting a song recalls its voices and effects, so the manual advises waiting about a second before
  [PLAY] (p. 89).
- The measure field sets the start point. [REW] and [FF] step measures from anywhere (p. 90).
- Jump modes: Stop Jump returns to the set measure on [STOP]. Top Jump goes there on [TOP] when past it,
  otherwise to measure 1. Jump Off (p. 90 to 91).
- Tempo 25 to 300 BPM (p. 91). Transpose -24 to +24 semitones (p. 93).
- On a track, [-1] toggles mute and [+1] toggles solo. `M` or `S` appears under the track (p. 92).

### 6.2 Track layout (p. 91, p. 229)

| Track | Holds | Record modes |
|---|---|---|
| 1 to 16 | Notes and other MIDI events. Each track has its own voice, pan, volume and sends (p. 61 to 64). | REPL, OVER, STEP, MULTI |
| Pt | Style and section per measure, and the per-measure time signature | REPL, STEP |
| Cd | Chords, at most one per quarter note | REPL, STEP |
| Tm | Tempo changes | REPL, plus insert in the event editor |

During recording the sequencer takes MIDI on all channels into the selected track. MULTI is the exception and
splits channels 1 to 16 onto tracks 1 to 16 (LB p. 52).

### 6.3 The pattern track (Pt) (p. 36, p. 47, p. 55 to 56)

- Each measure can hold one style and one section. A pattern starts from its beginning at the measure where
  it is entered and keeps playing until a different one is entered. To change only the section, enter only
  the section (p. 55).
- Styles: presets 001 to 128, user U01 to U64, and 129, the end pattern (p. 55).
- Sections: INTRO, MAIN A, MAIN B, FILL AB, FILL BA, ENDING, and BLANK, a silent pattern for breaks (p. 36).
- INTRO is followed by MAIN A, FILL AB by MAIN B, FILL BA by MAIN A, automatically. Playback stops after
  ENDING or at style 129 (p. 36, p. 55).
- Time signature per measure (step display METER column): 1/16 to 16/16, 1/8 to 16/8, 1/4 to 8/4. A measure
  shorter than the pattern cuts it off. A longer one repeats the pattern to fill it (p. 56).
- A song plays to its end and stops (p. 89). Once Pt holds patterns, the end is an ENDING section or style
  129, and without one the accompaniment plays on (p. 47).
- The pattern volume has its own `pat` fader in SONG VOICE, next to the `mst` master fader (p. 64).

### 6.4 The chord track (Cd) (p. 39 to 43, p. 48 to 49, p. 57 to 60)

- **Entry**: press a root key (lower octave) and a type key (upper octave) in either order, then [ENTER].
  Until ENTER the new chord flashes. Entering only a root keeps the previous type, and only a type keeps the
  previous root (p. 40 to 41).
- **Resolution**: at most one chord per quarter note (p. 57). Real-time entries snap to quarter notes (p. 49).
- **Anticipation**: a chord can start an 8th or a 16th before its beat, set with [OCT UP] in step record
  (p. 59).
- **Slash bass**: any root as the bass, set with [OCT DOWN] plus a key (p. 58). A Bass-type phrase then plays
  that note (p. 147).
- **Chord types**: 26, including the non-ABC type `---`, which moves patterns to the new root without
  re-harmonizing them (p. 41, LB p. 35). See the table in 8.3.
- **Fingered chord**: with FNGR on, chords played within the fingered zone are recognized and no ENTER is
  needed. A key below the zone's LOW note while holding a chord sets the slash bass. In this mode, notes below
  the zone's HIGH note do not sound. The zone and the MIDI receive channel are set in Utility (p. 43,
  p. 227 to 228). The List Book has a chart of the recognized fingerings (LB p. 36).
- **Templates**: the template field to the right of the chord picks `USR` (the song's own Cd track) or one of
  99 preset progressions (LB p. 37). Presets loop until [STOP] and are written for 4/4. Copy Event can copy a
  template into USR for editing (p. 43, p. 118 to 119).
- **Transpose**: Job 05 can transpose the Cd track (p. 113).
- Preset INTRO and ENDING patterns carry their own chord progressions, so only the chord root applies to them
  and the type is ignored (p. 42).

### 6.5 Tempo (p. 38, p. 50, p. 206)

Tempo is 25 to 300 BPM on the SONG screen. The Tm track holds tempo changes, recorded in real time (REPL only)
or inserted in the event editor at 25.0 to 300.0, so the stored tempo has 0.1 BPM steps (p. 206). Jobs that
work on Tm: Shift Clock, Copy Event, Erase Event, Create Continuous (TMP), Thin Out (TMP) and Clear Track.

### 6.6 Time signature (p. 56, p. 78, p. 127)

The song's starting time signature is set on the SONG screen (p. 78). Per-measure changes live in the Pt step
display's METER column (p. 56). Create Measure inserts measures in a chosen signature (p. 127). Copy Event
copies the time signature with the data (p. 119). Range: 1/16 to 16/16, 1/8 to 16/8, 1/4 to 8/4.

### 6.7 Length and position (p. 86 to 87, p. 104, p. 196)

- Measures 001 to 999 (p. 87). The Quantize screen's full range reads `001:1 ~ 999:4` (p. 104).
- Position: measure, beat (01 to 04 in 4/4), clock within the beat (000 to 479 for a quarter-note beat)
  (p. 87, p. 196). For /8 and /16 meters the beat is presumably the denominator's note value, with
  correspondingly fewer clocks (*inferred* from p. 196's wording).
- Gate times in the event editor read `quarter-beats:clocks`, from 00:001 to 99:479 (p. 196).

### 6.8 The event list editor (p. 192 to 209)

Put the cursor on a track in SONG play, then [MENU], `Edit`.

- **Change mode** (default): one event per line, in time order, with top-of-song and end-of-song markers and
  solid lines at measure boundaries. A note line shows location (beat:clock), pitch (C-2 to G8), gate and
  velocity (1 to 127). [-1]/[+1] change the field. The edited line flashes until [ENTER], and moving off the
  line cancels (p. 193 to 197).
- **Delete**: the `del` function key deletes the event on the cursor line (p. 197).
- **Insert mode**: [MENU], `Insert`. Choose the type with [-1]/[+1]: Note, PB (-8192 to +8191), PC (with bank
  MSB and LSB), CC, CAT, PAT, RPN, NRPN, Exc (typed in hex), XG RPN, XG NRPN, XG Exc System, Effect, Multi
  and Drum, and Tempo (Tm only). [ENTER] inserts, [EXIT] returns to change mode (p. 197 to 207).
- **XG View** shows XG events by name. **View Filter** hides event types: Note, PB, PC, CC, CAT, PAT, RPN,
  NRPN, Exc (p. 208 to 209).
- Editing Pt or Cd opens the same screens as their step record (p. 194).
- [SONG], [PATT] or [EXIT] leaves the editor (p. 207).

### 6.9 Voices and mixer (p. 61 to 74)

SONG VOICE is a mixer for the 16 tracks: M/S, voice, pan (L63 to R63 or random), volume 0 to 127 (default
100), plus the `mst` and `pat` faders. Voices come in 21 categories, including Ds1 and Ds2, the two editable
drum sets for songs (p. 61 to 64). Pattern drum tracks use Ds3 (p. 154). Effect sends and voice editing sit
under [MENU] (p. 66 to 74).

---

## 7. Song jobs (p. 101 to 136)

[MENU], `Job` from SONG play lists them. Function keys jump to the Event, Measure, Track and Song categories
(p. 101 to 102). Each job runs on [ENTER] and can be undone with Job 00.

| # | Job | What it does |
|---|---|---|
| 00 | Undo/Redo | Reverses the last job, edit or recording, or redoes it. Not for Clear Song All or Song Name (p. 103). |
| 01 | Quantize | Moves notes toward a grid with strength, swing and gate settings (p. 104). |
| 02 | Modify Velocity | Sets all velocities to one value 1 to 127, or scales by Rate 0 to 200% and adds Offset -99 to +99 (p. 107). |
| 03 | Modify Gate Time | Sets all gates to 1 to 9999 clocks, or scales by Rate 0 to 200% and adds Offset -9999 to +9999 (p. 109). |
| 04 | Crescendo | Ramps velocity by -99 to +99 across the range (p. 111). |
| 05 | Transpose | Shifts notes -99 to +99 semitones within a note range, on tracks 1 to 16 or Cd (p. 113). |
| 06 | Shift Clock | Moves events -9999 to +9999 clocks on tracks 1 to 16 or Tm, kept inside the range (p. 114). |
| 07 | Chord Sort | Orders notes that share a time low to high (Normal) or high to low (Reverse) (p. 116). |
| 08 | Chord Separate | Spreads notes that share a time by 0 to 999 clocks, in Chord Sort order (p. 117). |
| 09 | Copy Event | Copies a range from one track (1 to 16, Pt, Cd, Tm or All) to a destination measure:beat, N times, with transpose -24 to +24, overwriting. Copies the time signature. On Cd it can copy a preset template into USR (p. 118). |
| 10 | Erase Event | Blanks a range on one track (1 to 16, Pt, Cd, Tm). The measures stay (p. 120). |
| 11 | Extract Event | Removes one event type (Note, PC, PB, CC, CAT, PAT, Exc) from a range, optionally merging it into another track (p. 121). |
| 12 | Create Continuous | Writes a ramp of PB, CC, CAT, master volume or tempo events with start and end values, spacing 0 to 999 clocks and curve -16 to +16 (p. 123). |
| 13 | Thin Out | Removes every other PB, CC, CAT, PAT or tempo event in a range (p. 125). |
| 14 | Time Stretch | Scales a range's timing 50% to 200%, gates included, on one track or all (p. 126). |
| 15 | Create Measure | Inserts N blank measures with a chosen time signature on all tracks, pushing later measures back (p. 127). |
| 16 | Delete Measure | Removes a range of measures from all tracks and closes the gap (p. 128). |
| 17 | Copy Track | Copies one track to another, with Event, Play Effect and Voice checkboxes (p. 129). |
| 18 | Mix Track | Merges two tracks into a third (p. 130). |
| 19 | Clear Track | Clears tracks 1 to 16, Pt, Cd, Tm or All, with Event, Play Effect and Voice checkboxes (p. 131). |
| 20 | Expand Backing | Renders Pt and Cd as notes onto tracks 9 to 16 (D1 to 9, D2 to 10, PC to 11, BA to 12, C1 to C4 to 13 to 16), overwriting them (p. 132). |
| 21 | Normalize | Writes the Play Effects into the data and resets them (p. 133). |
| 22 | Copy Song | Copies a whole song with its name, Play Effects and voices (p. 134). |
| 23 | Clear Song | Clears one song or all, voices and Play Effects included. All cannot be undone (p. 135). |
| 24 | Song Name | Names the song, up to 8 characters (p. 136). |

---

## 8. Chord transposition: how pattern phrases follow the chord track

### 8.1 Phrase types (p. 146 to 148)

Each phrase in a pattern has a TYPE, which the manual calls the Note Transition Table type, and a source
chord, the chord it was written over (p. 144, p. 146). The ABC system re-harmonizes the phrase from the source
chord to the current chord according to its type (p. 147):

| Type | Behavior |
|---|---|
| Bypass | Not changed by chords. For drums and percussion. |
| Bass | Re-harmonized like Chord 1, except the first note after a chord change is always the root. With a slash chord it plays the slash note. |
| Chord 1 | Re-harmonized from the root of the new chord. |
| Chord 2 | Re-harmonized with the closest voice leading from the previous chord, for the smoothest changes. |
| Parallel | Transposed to the new root without re-harmonization. |

Default types for user phrases: D1, D2 and PC Bypass, BA Bass, C1 to C4 Chord 1 (p. 147).

### 8.2 The Phrase Table (p. 210 to 211)

In the pattern event editor, [MENU], `Phrase Table` sets per phrase:

- **TYPE**: Chord 1, Chord 2, Bass, Bypass or Parallel.
- **PHRASE VOICE**.
- **SOURCE CHORD**: the chord the phrase is written over. A slash chord is not allowed.
- **LO LIMIT, HI LIMIT** (C-2 to G8): re-harmonized notes outside the range move by octaves until they fit.
- **HI KEY** (C to B): the highest root, for Chord 1 and Bass types. The manual gives no more detail.
- **CURRENT CHORD** and **FNGR**: the playback chord in PATTERN mode. Only these two can be edited on preset
  phrases.

### 8.3 Chord types (LB p. 34 to 35)

| Type | Degrees | Type | Degrees |
|---|---|---|---|
| M | 1, 3, 5 | m | 1, b3, 5 |
| 6 | 1, 3, 5, 6 | m6 | 1, b3, 5, 6 |
| M7 | 1, 3, 5, 7 | m7 | 1, b3, 5, b7 |
| M7(9) | 1, 3, 5, 7, 9 | m7(9) | 1, b3, 5, b7, 9 |
| add9 | 1, 3, 5, 9 | madd9 | 1, b3, 5, 9 |
| 7 | 1, 3, 5, b7 | mM7 | 1, b3, 5, 7 |
| 7(9) | 1, 3, 5, b7, 9 | m7(11) | 1, b3, 5, b7, 11 |
| 7(b9) | 1, 3, 5, b7, b9 | m7(b5) | 1, b3, b5, b7 |
| 7(#9) | 1, 3, 5, b7, #9 | dim | 1, b3, b5, 6 |
| 7(13) | 1, 3, 5, b7, 13 | aug | 1, 3, #5 |
| 7(b13) | 1, 3, 5, b7, b13 | sus4 | 1, 4, 5 |
| 7(#11) | 1, 3, 5, b7, #11 | 7sus4 | 1, 4, 5, b7 |
| 7(#5) | 1, 3, #5, b7 | --- | non-ABC: root transposition only |

### 8.4 What the manual says and does not say

- It gives the behavior of each type, not the transition tables. The exact mapping (which note of a source
  phrase becomes which note over each target chord, and how Chord 2 picks its voice leading) is **not in the
  manual**.
- Yamaha's rules for writing user phrases: stay within the source chord's harmony, use mainly the root, 3rd,
  5th and major 7th, and keep phrases rhythmic rather than melodic (p. 148).
- Put Phrase re-harmonizes a pattern track to the current pattern chord before copying it into a song track
  (p. 185).
- So the exact transposition rules have to be designed for the build, guided by the type descriptions above,
  the chord degree table in 8.3 and listening tests.

---

## 9. Pattern mode (secondary)

- **Styles and sections**: 128 preset styles and 64 user styles, each with INTRO, MAIN A, MAIN B, FILL AB,
  FILL BA and ENDING, so 768 preset and 384 user patterns (p. 229). The PATTERN screen selects style
  (U01 to U64 for user), section, length 1 to 8 measures, time signature and tempo (default 120) (p. 139).
  INTRO, MAIN A, MAIN B and ENDING default to 2 measures, the fills to 1 (p. 140).
- **Tracks**: D1 and D2 (drums), PC (percussion), BA (bass), C1 to C4 (chord backing) (p. 140).
- **Phrases**: a pattern is a set of phrase references, one per track. There are 4,167 preset phrases,
  numbered by category (Da, Db, Fa, Fb, PC, Ba, Bb, Ga, Gb, GR, KC, KR, PD, BR, SE), beat (16 beat, 8 beat,
  3/4) and number (p. 137 to 138). Each user style has 48 user phrases, one per section and track (p. 145,
  p. 229). Phrases longer than the pattern are cut off (p. 141). The `Cler` key clears a phrase (p. 142).
- **Playback chord**: entered as in song mode, with slash bass via [OCT DOWN] (p. 142).
- **Recording**: realtime overdub loops the pattern (p. 148). Step record is as in section 3 (p. 150).
- **Voices**: each phrase has its own voice. PATTERN VOICE can override per track, but overrides on preset
  patterns are not stored (p. 151).
- **Pattern jobs** (p. 157 to 191): 00 Undo/Redo, 01 to 14 the same event jobs as the song list on D1 to C4,
  15 Copy Phrase (a preset phrase into a track as a user phrase, repeated or cut to length), 16 Get Phrase
  (song track measures into a pattern track), 17 Put Phrase (pattern track into a song track at a measure,
  re-harmonized to the current chord), 18 Copy Track, 19 Mix Track, 20 Clear Track, 21 Copy Pattern (a style
  section or all six into a user style), 22 Clear Pattern, 23 Style Name.

---

## 10. Limits

| Item | Value | Cite |
|---|---|---|
| Note capacity | About 32,000 notes, shared by songs, user patterns and user phrases | p. 229, p. 233 |
| Songs | 20, plus 3 demo songs | p. 89, p. 229 |
| Song length | 999 measures | p. 87 |
| Song tracks | 16 sequencer tracks plus Pt, Cd and Tm | p. 229 |
| Styles | 128 preset, 64 user, 6 sections each | p. 229 |
| Pattern length | 1 to 8 measures | p. 139 |
| Pattern tracks | 8 | p. 140 |
| Phrases | 4,167 preset, 48 user per user style | p. 137, p. 229 |
| Sequencer polyphony | 64 notes | p. 229 |
| Tone generator polyphony | 32 notes, counted in 1-element voices | p. 229, p. 238 |
| Multitimbral parts | 24 (16 song tracks plus 8 pattern tracks) | p. 229 |
| Voices | 519 normal, 20 drum kits | p. 229 |
| Tempo | 25 to 300 BPM, 0.1 BPM steps in the Tm track | p. 38, p. 206 |
| Time signatures | 1/16 to 16/16, 1/8 to 16/8, 1/4 to 8/4 | p. 56 |
| Chord types | 26, including non-ABC | p. 229 |
| Chord templates | 99 preset, 1 user progression per song | p. 229 |
| Groove templates | 100 | p. 97 |
| Count-in | OFF or 1 to 8 measures | p. 221 |
| Undo | 1 level | p. 103 |
| Song name | 8 characters | p. 136 |
| Display | 128 x 64 dots | p. 230 |

"Memory Full" stops recording, editing, jobs and MIDI reception (p. 233). "Can't Undo OK?" warns when a job
would leave no room for undo (p. 234). SHIFT plus the second function key shows used memory (p. 26).

---

## 11. MIDI behavior

Described behavior only. The List Book also documents SysEx framing and a bulk-dump address map (LB p. 50
to 55, LB p. 63). Those formats are left out of this reference on purpose, because this is a clean-room build.

- **Clock and transport**: MIDI SYNC selects the internal clock or external MIDI clock. With External the
  QY70 will not run on its own. MIDI CONTROL (Off, In, Out, In/Out) sets whether Start, Stop and Continue
  are received, sent or both (p. 222, LB p. 53).
- **Song Position Pointer** is sent when moving to another measure in SONG play and received in standby.
  **Song Select** is sent when the song (or, in PATTERN mode, the pattern) changes, and received (LB p. 53).
- **Recording input**: during recording all channels go to the selected track. MULTI instead puts channels 1
  to 16 on tracks 1 to 16 (LB p. 52). MIDI FILTER can block PB, CC, aftertouch or SysEx while recording
  (p. 223).
- **Echo back**: Off, Thru, or RecMontr, which re-sends incoming data on the record track's channel (p. 223).
  LOCAL CONTROL Off silences the micro keyboard's internal sound (p. 223).
- **Pattern output**: `PATT OUT CH` sends the pattern tracks on channels 1 to 8 or 9 to 16, in the order D1,
  D2, PC, BA, C1, C2, C3, C4, or not at all (p. 224).
- **Remote section change**: the QY70 accepts a SysEx message that switches the playing section to INTRO,
  MAIN A, MAIN B, FILL AB, FILL BA, ENDING or BLANK (LB p. 54).
- **SysEx in tracks** is recorded at a single time position however long it took to arrive, and played back
  with an adjustable pause per 1 KB (LB p. 54, p. 221).
- **Saving**: songs and user styles are saved and loaded by bulk dump to a computer or MIDI data filer, as
  one song, one style (all six sections together) or everything (p. 225 to 226).
- **Standard MIDI Files**: the QY70 cannot read or write SMF itself. Yamaha's QY Data Filer software converts
  between a song dump and SMF on the computer. A song that uses Pt and Cd needs Expand Backing first, or the
  accompaniment is lost (DF p. 15 to 19).

---

## 12. Code and formats

**None of this was used.** This is a clean-room build, so the reference draws only on the behavior described
in Yamaha's manuals. The projects below were found while searching GitHub and the web and are listed for
awareness only.

- **charlesvestal/fm1-polly** (GPL-3.0): a QY70-inspired auto-accompaniment firmware for the FM-1.
- **afbecerra7-netizen/qy100-toolkit** (MIT code, CC BY 4.0 docs): SysEx dump and decode tools for the QY100.
- **doffu0000/qy100-qy70-syx-converter** (GPL-3.0): converts bulk dumps between the QY100 and QY70.
- **Max-Coppola/qyTools** (AGPL-3.0): browser replacement for Yamaha's Data Filer for the QY100 and QY70.
- **mamedev/mame, ymqy70.cpp** (GPL-2.0+): a skeleton QY70 driver, marked not working.
- **ryanmgilmore/schwung-qy70-editor** (MIT): Ableton Move module that edits QY70 XG parameters.
- **doffu0000/qy70-qy100-web-controller** (GPL-3.0): Web MIDI console for QY70 and QY100 XG parameters.
- **orange303-afk/QY70Controller** (MIT): VST3/AU XG parameter editor for the QY70.
- **richb93/qy700-dumpscreen** (no license stated): captures QY700 screens over MIDI.
- **antoniograzioli/VintageEmulatorStudio** (AGPL-3.0): plug-in hosting MAME instruments, including the QY70 driver.
- **colonel-blimp/m8-midi-instruments-for-qy70** (no license stated): Dirtywave M8 instrument files for QY70 voices.
- **simonlatham155-tech/QY70_live_editor** (no license stated): empty repository.
- **cicloid/awesome-fm-1** (CC0) and **aroum/fm1-custom-fw** (no license stated): directories of FM-1 firmware.

Not found: any project that reproduces the QY70's step-record workflow, a working QY70 emulator, or open
tools for the QY700's file formats.

---

## 13. Questions the manual leaves open

1. Whether a step-record note registers on key press or release, and how a chord is grouped (3.4).
2. How far one cursor press moves the step-record pointer (3.5).
3. The MIDI values of the ten VELO levels and the spread of R1 to R4 (3.3).
4. Whether STEP accepts SHIFT digit entry and the exact [+1] order of STEP values (3.3).
5. Whether `tie` can use a STEP changed after the note (3.6).
6. Whether a step-entered note replaces or joins a note already at that position (3.6).
7. What [PLAY], [REC], [MENU] and [EXIT] do during note step record (3.7).
8. Whether REPL erases data after the stop point, and whether OVER works on Pt and Cd (4.1, 4.4).
9. The exact transition tables behind Chord 1, Chord 2, Bass and HI KEY (8.4).
10. Whether a pattern restarts when the same style and section are entered again on a later measure (6.3).
