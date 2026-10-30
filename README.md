# FM1 QY

Custom firmware for the M-VAVE FM-1, built on [FM1 Open V14](https://github.com/m-vave/fm1_open). It adds a song
mode modeled on the Yamaha QY70's song sequencer, a clip grid after Ableton Live's session view, chord mode and
glitch tools, and a browser workbench for songs and kits.

The song mode was built clean-room from descriptions in Yamaha's QY manuals. It contains no Yamaha code and no
code from any other sequencer.

This project is not affiliated with Yamaha, Ableton or M-VAVE.

The release version is `FM1_QY_VERSION` in `app/version.h` (semver, with [CHANGELOG.md](CHANGELOG.md)), and
releases are tagged `v<version>`.

## Status

**Song playback timing has been measured on an FM-1, with the default build.** That timing test chose how
song notes reach the engine (method B, below). The results are in
[docs/hardware-test-plan.md](docs/hardware-test-plan.md). The full build has run on an FM-1 since 0.1.0 (10/28).

There are two device builds, and the browser preview:

| Build | What's in it | App size |
|---|---|---|
| Default (`./build.sh`) | The hardware test build: FM1 Open V14 plus a diagnostics page that plays a built-in test song and measures its timing. To fit, it leaves out the 16 factory preset projects and the workbench's project, sample and voice transfers. Firmware updates still work | 453,608 bytes of 453,921 |
| Full (`FM1_SONG_DEVICE_FULL=1`) | The whole song mode, without the diagnostics page. It leaves out the stock step and pattern pages (their sync and MIDI pages move to the device settings), the stock project pages, the demo projects, the factory sound sets and the built-in FM factory voices (the workbench can send those to the DX7 library) | 527,816 bytes of 548,129 |
| Browser preview | The whole song mode on FM1 Open's simulator, in one HTML file | |

### What installing the full build costs

The full build makes room by giving 23 of the user flash's 131 sectors (92 KB) to code: the 15 stock project
slots and the first 8 sectors of the stock sample area. On the device that means:

- The settings sector moves, so the MIDI, clock and Bluetooth settings start from their defaults.
- The stock projects are overwritten and the stock sample bank is lost. Kits come from the workbench, which
  can send FM1 Open's 8 built-in sounds as a kit.
- The SDK's settings area (VM) takes the space between the code and the Bluetooth sector, 48 KB at the
  current size (32 KB at least), and the Bluetooth sector moves. Neither has been tried on hardware.
- The DX7 voice banks and the tuning sector keep their places. Songs and kits share the remaining 101 sectors
  (404 KB).

The workbench's "Restore original V15" puts the stock firmware and its 128 factory voices back.

## Features

The full build and the preview have all of these. The [user guide](docs/user-guide.md) has every control.

**Song mode and the player**
- 8 tracks of notes and automation, 4,096 events, 480 clocks per quarter note, up to 999 bars.
- Tempo changes (20 to 300 BPM) and time signature changes anywhere, markers and a loop region.
- Swing (8th or 16th) switched on per track, and a key and scale that played notes snap to.
- The player runs off the audio sample clock, so timing doesn't drift. Its notes reach the engine block by
  block: while a song plays, the audio interrupt wakes the control loop after every 2.9 ms block to hand over
  the next block's notes. On the FM-1 that got 1,312 of the test song's 1,361 notes into their block and the
  rest one block late.

**Step entry**
- Lengths from a 64th to a whole note, with triplets, dots and double dots.
- Velocity, gate, rest, repeat and tie on keys, chords built key by key, and editing notes in place at the
  pointer.
- A piano roll zoomed from 4 bars to half a bar, with bar, beat and 16th lines, that plays in place: PLAY
  hears it without leaving, and REC records a take there.
- A swept selection across bars, narrowed to pitches (every hi-hat, say), to delete, change velocity, length,
  gate or glitch, or transpose.

**Real-time recording and arranging**
- Replace, overdub, punch and loop recording, count-in, metronome, and input quantize with a strength that
  can be changed after the take.
- The loop region is the selection. Copy, cut, paste (inserting or over), duplicate, insert and delete bars,
  across tracks and between songs.
- Up to 64 levels of undo and redo.

**Automation**
- K1 to K4 each record a parameter while recording, chosen per track.
- An automation view for tempo, time signature, program changes and controller lanes, with chase on every
  start and jump.

**Glitch**
- Per-note repeats, pitch drift, fade, chance, octave jumps, accent, flam and pad swap.
- Humanize per track.
- Mangle: scramble 16ths, reverse, shuffle notes between tracks.

**Saving and kits**
- Songs and kits share a pool in flash, with a memory gauge and a song list (load, save, save as, new,
  rename, duplicate, delete).
- A recovery copy is written after playback stops.
- A kit is 8 pads with names and settings. A song names its kit, kits share the samples they have in common,
  and the kit table is kept twice so a power cut can't lose it.

**Edit jobs**
- Twenty jobs, most after the QY70's: quantize, velocity, crescendo, gate time, transpose, fit to scale, shift
  clock, chord sort, strum, paste N times, erase, extract, ramp, thin out, time stretch, copy, merge and
  clear track, normalize and expand clips.

**Clips**
- A grid of 8 tracks by 16 rows, launched per track or by row at a launch quantize.
- Clips are edited in the song view, even while the song plays, and can be recorded into on their next pass.
- REC in the grid records into a cell: a new clip of 1 to 16 bars, or FREE to the next bar line after REC, or an
  overdub of the clip there. The keys play its track meanwhile.
- Clips go into the song as blocks: record a performance from the grid, or paste a clip by hand.
- The Expand job turns blocks into plain notes.

**Chord mode**
- Roots on the lower 12 keys, a latched chord type on the next 12: in key, major, minor, 7, m7, maj7, 6, sus2,
  sus4, dim, aug and add9.
- Close, spread and smooth voicing, and a strum (a 64th to an 8th) up, down, alternating or random.
- Chords record as plain notes. In step entry a root enters its whole chord.

**The panel**
- Every button light stays on, on every page, so the printed labels can be read. The key lights still show
  the scale, the swinging tracks and the clips playing.

**The workbench**
- A "Songs and kits" section in the FM1 Open workbench page, for the device or the page's simulator.
- Song files, MIDI export and import, kits built from WAV files with per-pad prep, the built-in kit, and
  copying clips between songs.

## Building on a Mac

The Mac build runs FM1 Open's PowerShell build scripts in a Docker container with JieLi's Linux toolchain.

| Needs | Where | Size |
|---|---|---|
| Docker Desktop, running | | |
| JieLi's Linux toolchain | `~/.jieli/toolchain` (or set `FM1_LINUX_TOOLCHAIN`) | 135 MB |
| The build image `fm1-open-build` | Built from `tools/docker/` on the first run | 576 MB on disk |
| WASI SDK 27, for the preview and the workbench page | Downloaded into `build/tools/` on the first web build (or set `FM1_WASI_SDK`) | 518 MB download, 419 MB unpacked |

Install the toolchain once:

```sh
mkdir -p ~/.jieli
curl -fL https://pkgman.jieliapp.com/s/linux-toolchain | tar -xJ -C ~/.jieli
```

Then:

| Command | Builds | Output |
|---|---|---|
| `./build.sh` | The firmware | `build/dx7/sdk.elf` |
| `./build.sh --firmware` | The firmware and its update image | Also `build/dx7/generated/FM1_OFFICIAL_OPEN.fwsc` |
| `./build.sh --package` | The firmware, its update image, and a workbench page that carries it and the original V15 | Also `build/official-web/FM1-Official-Open-Source.html` and its `SHA256SUMS.txt` |
| `./build.sh --preview` | The browser preview | `build/preview/index.html` |
| `./build.sh --test-preview` | A preview built like the default device build: no song pages, the diagnostics page | `build/preview-test/index.html` |
| `./build.sh --web` | The same preview, with its source link filled in | `build/preview/index.html` |
| `./build.sh --help` | Prints this list | |

`--package` and `--web` need `FM1_SOURCE_URL`, the HTTPS URL of the matching source archive, which the page
links to. Upstream's README notes that a moving GitHub `tree/main` page doesn't count. This repo is private,
so a build given to anyone else needs a source archive they can reach.

For the full build, set `FM1_SONG_DEVICE_FULL=1`. Its outputs go to `build/dx7-songfull/` and
`build/official-web-songfull/`. With `COMMIT` as the commit you built:

```sh
FM1_SONG_DEVICE_FULL=1 FM1_SOURCE_URL=https://github.com/judahn/fm1-qy/archive/COMMIT.tar.gz ./build.sh --package
```

A firmware build prints its size and checks it fits. A package build also prints the flash layout, for example
`Offline package fits 1 MiB: image 548864, VM 49152, user 442368 bytes`.

Every firmware build of a kind (default or full) first clears that kind's update image and replaces its
workbench page with a "Build incomplete" stub. So after a plain `./build.sh`, run `--package` again before
installing, and keep a copy of the page you installed with.

**Never delete `build/`.** `build/tools/` holds the unpacked WASI SDK, and deleting it means another 518 MB
download. Delete a single output folder instead.

FM1 Open's Windows build (`build.bat` and `sdk/Build.ps1`, see [upstream](https://github.com/m-vave/fm1_open)) is
still here. We haven't tried it with our changes.

## Installing on the device

This is how FM1 Open's workbench installs firmware. The default build went onto an FM-1 for the timing test,
and the full build has been installed the same way since 0.1.0.

1. Build the page: `./build.sh --package`, or the full build command above.
2. Open `build/official-web/FM1-Official-Open-Source.html` (or the `-songfull` one) in desktop Chrome or Edge.
3. Plug the FM-1 in over USB, switch it on and close other MIDI apps.
4. Open the **Device update** tab and click **Connect device**. Allow MIDI and SysEx.
5. Back up first, then tick the backup box. The page says which step comes next:
   - From stock V15 or other firmware: click **Enable force flash and reconnect**, then **Force flash**, and
     accept the warning.
   - Otherwise: click **Start update**.
6. Keep the cable in and the power on until it restarts. If it stops partway or stays in its update mode,
   keep the same page, reconnect and continue as the page says.

To go back to stock, click **Restore original V15** in the same page, connect, tick the backup box and click
**Install original V15**. That also restores the 128 factory DX7 voices.

The page warns that firmware without the USB-MIDI update protocol needs its matching recovery tool, and the
hardware test plan names M-VAVE's PC firmware tool for a device that won't answer over USB. A button held at
power-on to force the update mode isn't documented in this tree or upstream, so this README doesn't give one.

## Using it

With the full build or the preview:

- **SEQ** opens the song view. **PLAY** plays and stops, and a stop comes back to where it started (the start
  point, which K1, the markers and SHIFT+PLAY set).
- A tap of **REC** (stopped) records in the song's mode, OVERDUB to begin with, and the take shows on a piano
  roll. **SHIFT+REC** is step entry, and leaves it. A held **REC** opens the record settings. **SHIFT+SEL**
  switches REPLACE and OVERDUB.
- **ALGO** zooms the song view, the automation view and step entry's roll.
- **SEQ** again opens the clip grid, which plays only launched clips, while the song view plays only the song.
- A tap of **GLO** opens the song settings, with chord mode on the fifth page.
- **SAVE** opens the song list, and **SAVE** there saves. On the list K2 picks the action, **OCT+** (YES) does it,
  **OCT-** (NO) cancels. **HOME+SAVE** undoes.
- **PLAY** or **REC** during a take ends the recording, and playing goes on.

Held, FX is SHIFT on the song pages. A tap of FX opens the effects page, and ARP is the arpeggiator, as on the
stock pages. The [user guide](docs/user-guide.md) covers every page and control, checked against the code.

## The workbench

The workbench is FM1 Open's browser page with a **Songs and kits (song mode)** section added. It's in the page
from `./build.sh --package` and in the preview, on the **Projects & sounds** tab.

1. Pick the device: **Browser simulator** (the page's own simulator) or **FM-1 over USB**.
2. Click **Refresh**. For the FM-1 the browser asks for MIDI access. Only the full build answers, because the
   default build has neither songs nor kits.
3. The memory gauge shows songs, kits and samples, and what's free.

**Songs.** The list of songs on the device. Each can be downloaded as a `.fm1song` file, exported as a MIDI
file, loaded on the device or deleted, and the working song can be saved as a new one. Open a song file or a
MIDI file from the computer, or start a new one from one of FM1 Open's 16 factory sound sets. The open song
uploads as a new song, replaces the selected one, saves as a file or exports as MIDI.

**MIDI.** Export writes a format 1 file at 480 ticks per quarter note: a conductor track with the name, tempos,
time signatures and markers, then one track per FM-1 track with notes, controllers and program changes. Every
clip block is expanded into the notes it plays. Import shows which part of the file goes to each FM-1 track,
lets you change that, and reports what doesn't fit. It makes a new song file to upload or save.

**Clips.** The open song's clips are listed. Pick one, pick a track and row, select a song on the device, and
**Copy into the selected device song** puts the clip there.

**Kits.** The list of kits on the device, to use on the pads, download as a `.fm1kit` file or delete.
**Send the built-in kit** sends FM1 Open's 8 built-in sounds (BWOOM to SHAA) as a kit named BUILT-IN, 88 KB on
the device, unless a kit with the same samples is already there. To build a kit, load a WAV on each of up to 8
pads and set its start, end, fades, gain, bit depth, sample rate, normalize and reverse. **Prepare pads**
encodes them, **Save kit file** keeps the kit on the computer, and **Write kit to the device** sends it.
Samples already on the device aren't sent twice.

**Send the factory DX7 voices** writes M-VAVE's 128 factory DX7 voices into the device's DX7 library (4 banks of
32), which the full build needs: it leaves the factory voices out, so its DX7 presets are blank until the library
holds voices. It asks before replacing a bank that holds voices of its own, and reads every bank back.

The page's other tools are FM1 Open's. With the full build, the DX7 voice library works. The stock sample
bank page works through the kits: a bank written there becomes a new kit, and reading the bank gives the
current kit. The stock project library connects, but its slots are gone: they show as empty, and saving to one is
refused.

## Development

### Tests

```sh
tests/run_host.sh
```

This builds and runs the C tests in `tests/` with the Mac's `cc`, under the address and undefined-behavior
sanitizers. The flash map test and the storage tests then run on the full build's flash layout. Last come the
workbench's Node tests.

`tests/test_deliver.c` checks note delivery as the full build runs it, against a model of the FM-1's audio
interrupt and control loop: every note of the test song and of a stress song must render in the block it's
due.

```sh
./build.sh --preview
./build.sh --test-preview
PLAYWRIGHT_MODULE=/path/to/node_modules/playwright node tests/preview_check.mjs
```

This drives the previews headless in Playwright's Chromium, phase by phase. The diagnostics phase uses
`build/preview-test/`, the rest `build/preview/`. Some checks slow the simulated LCD to the device's pace,
one 16-row band per control poll, which is where a page could be left half painted on the FM-1. It prints
each failure with its phase, name, actual and expected values, then the totals per phase.

| Variable | Effect |
|---|---|
| `PLAYWRIGHT_MODULE` | Where Playwright is, when it isn't installed in this repo |
| `PREVIEW_PHASES=7b,8` | Run only these phases (also `diagnostics`, `panel`, `keys` and `yes/no`) |
| `PREVIEW_DUMP=1` | Print each phase's full record too |

The checks save screenshots to `docs/`. Screens of the device come out the same on every run. Shots of the
workbench page are left alone when the new one is within one shade per channel of the file, so browser noise
doesn't churn them.

### Where things are

| Path | What |
|---|---|
| `app/song/` | The song mode: model, player, storage, clips, chords, and the song pages (`*.inc`, included by `app/screen.c`) |
| `app/song/song_config.h` | What each build includes |
| `app/screen.c` and other stock files | FM1 Open's code. Our changes there are small hooks with a `song-mode` comment |
| `web/workbench/` | The workbench's Songs and kits section |
| `tests/` | Host tests (`test_*.c`), the workbench test and the preview phases (`preview_*.mjs`) |
| `docs/song-mode-design.md` | The design, with the decisions behind it |
| `docs/file-formats.md` | The song and kit files and the transfer commands |
| `docs/user-guide.md` | The controls |
| `docs/hardware-test-plan.md` | The first session on hardware |
| `docs/qy70-reference.md` | How the QY70 works, from its manuals, with page cites |

### Taking FM1 Open updates

```sh
git fetch upstream
git merge upstream/main
```

Conflicts will show up where our hooks sit in stock files, mostly `app/screen.c`. The others are
`app/instrument.h`, `app/transfer.c`, `app/presets.c`, `app/project.c`, `app/samples.c`, `app/fm_library.c`,
`app/fm_factory.c`, `app/midi_router.c`, `app/board/audio.c`, `sdk/Build.ps1`, `sdk/Package.cs`,
`sdk/Simulator.js`, `sdk/Update.html.in`, `tools/Build-Web.ps1` and `web/engine/Simulator.c`.

`sdk/manifest.json` pins a SHA-256 for every file under `sdk/`, so it will likely conflict too. A file whose hash
doesn't match stops the build with `SDK payload changed:` and the file's name. After the merge, hash the four
SDK files we change and put the results in the manifest:

```sh
shasum -a 256 sdk/Build.ps1 sdk/Package.cs sdk/Simulator.js sdk/Update.html.in
```

After a merge, run the tests, and check that the default build still fits (`./build.sh` prints its size).

## License

FM1 Open's app sources are under the GNU GPL version 3 only, with M-VAVE's additional permission to link them
with the FM1 binary SDK, as set out in [LICENSE.md](LICENSE.md). Our files carry the same license tag as FM1
Open's: `GPL-3.0-only WITH GPL-3.0-linking-exception`.

The engine, `sdk/lib/libfm1_engine.a`, is M-VAVE's proprietary binary SDK under separate terms in
[THIRD_PARTY.md](THIRD_PARTY.md), which allow it on M-VAVE FM1 devices. No source for it is supplied. The
boot and update files in `sdk/tools/pack/` and the original V15 image are under the Apache License 2.0, and
the other components keep their own licenses, all listed there. When distributing a build, include
LICENSE.md and THIRD_PARTY.md, and provide the matching app source and build and install scripts.

FM1 Open is by M-VAVE: <https://github.com/m-vave/fm1_open>.
