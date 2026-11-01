# Kits

Drum kits for QSEQ's sampler, 8 pads each. Each folder holds a kit's pads and its `.fm1kit` file.

## Loading one

1. Open the workbench page (README.md, The workbench) and connect the FM-1.
2. In **Songs and kits**, under **Kits**, use **Open a kit file** and pick the `.fm1kit`.
3. Click **Write kit to the device**.
4. On the FM-1, the song list's K3 picks the kit for the song.

Songs and kits share about 400 KB of flash, in 4 KB sectors. A kit of short one-shots takes 12 to 16 sectors
(48 to 64 KB), so keep the kits you use on the device and the rest on the computer. A sample the device already
has isn't stored twice.

## The pads

Every kit here uses the same order, so a song's drum parts make sense on any of them:

| Pad | Sound |
|---|---|
| 1 | Kick |
| 2 | Snare |
| 3 | Closed hat, or the kit's ride |
| 4 | Open hat, or a chop |
| 5 | A second snare, clap or ghost |
| 6 | Rim or percussion |
| 7 | Ride or shaker |
| 8 | Crash, stab or effect |

## Making a kit

`tools/make-kit.mjs` makes a kit file from a folder of one-shots. It runs the workbench's own sample prep and
encoder in headless Chromium, so it needs a built page (`./build.sh --package` or `--preview`) and Playwright
(set `PLAYWRIGHT_MODULE` as for the preview check).

```
node tools/make-kit.mjs kits/mykit            # pads from the folder's audio files, in name order
node tools/make-kit.mjs kits/mykit --wavs     # also saves each pad as prepared, to listen to first
```

Without a `kit.json`, the first 8 audio files (WAV, or AIFF, FLAC, MP3, OGG and M4A through ffmpeg) become the
pads in name order, named by their files, each trimmed to where its sound starts, normalized and given a 5 ms
fade-out. A `kit.json` sets the name and each pad, and lets several pads slice one file, which is how a break
gets chopped:

```json
{
  "name": "AMEN",
  "source": "where the audio came from",
  "defaults": { "file": "amen.ogg", "normalize": true, "fadeOut": 8 },
  "pads": [ { "name": "KICK", "start": 8.0998, "end": 8.3126 }, { "name": "SNARE", "start": 5.0385, "end": 5.2483 } ]
}
```

A pad takes the workbench's prep settings: `start` and `end` in seconds, `fadeIn` and `fadeOut` in ms,
`reverse`, `normalize`, `gain` in dB, `bits` (1 to 16) and `hold` (a lower sample rate in Hz), plus `trim`. The
tool prints each pad's length and sectors, and the kit's total.

## What goes in the repo

Only audio the project can share: sounds made here, or ones under a license that allows it. For a kit made from
a recording the project doesn't own, commit the `kit.json` alone, with its `source`, and list the folder's audio
in `.gitignore`, as `kits/amen/` is. Anyone with the source can rebuild the kit from the recipe. `python3 kits/synth.py` regenerates the synthesized
kits' pads (numpy, scipy and matplotlib), the same every time.

| Kit | Made from |
|---|---|
| `jungle/` | Synthesized here (`kits/synth.py`): kick, snare, hat, open hat, ghost, rim, ride, a minor 7th stab |
| `dnb/` | Synthesized here: a harder 90s kick and snare, hats, clap, shaker, ride, a reese bass hit |
| `boombap/` | Synthesized here, through 12-bit and saturation: fat kick, snare, hats, snap, rim, tambourine, horn stab |
| `lofi/` | Synthesized here, through 11-bit, tape saturation and wobble: kick, snare, hat, shaker, rim, perc, crackle, keys |
| `amen/` | The Winstons, "Amen, Brother" (1969): the recipe only. The audio is under copyright, for personal use |
| `think/` | Lyn Collins, "Think (About It)" (1972), with the WOO and YEAH shouts: the recipe only, for personal use |
| `funky/` | James Brown, "Funky Drummer" (1970), Clyde Stubblefield's break: the recipe only, for personal use |
