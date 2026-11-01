---
name: make-kit
description: Make a QSEQ drum kit (.fm1kit) from a folder of one-shots or by chopping a break. Use when asked to make, build, chop or update a kit, or to turn samples into something the FM-1 can load.
---

# Making a kit

A kit is 8 pads for the FM-1's sampler. `kits/README.md` has the format, the pad order and the rules for what
goes in the repo. Read it first.

## Steps

1. **Gather the sounds** in `kits/<name>/`, one file per pad, named so they sort in pad order
   (`1-KICK.wav` to `8-CRASH.wav`). Follow the pad order in `kits/README.md`.
2. **Check the rights before anything is committed.** Sounds made here, or under a license that allows
   sharing, are committed with the kit. For anything else (a record, a sample pack, a rip), commit only
   `kit.json` with a `source` note, and add the folder's audio to `.gitignore`, as `kits/amen/` does. Say so
   to the user.
3. **Write `kit.json`** when pads need trims, fades or levels, or when they slice one file. Field names are in
   `kits/README.md`.
4. **Build:** `PLAYWRIGHT_MODULE=... node tools/make-kit.mjs kits/<name> --wavs`. It needs a built page
   (`./build.sh --package` or `--preview`). It prints each pad's length and sectors.
5. **Keep it small.** Aim for 16 sectors (64 KB) or fewer a kit: hats about 0.17 s (1 sector), kicks and snares
   0.35 s or less (2), rides, crashes and stabs 0.53 s or less (3). Songs and kits share about 400 KB.
6. **Look before handing it over.** Plot the `--wavs` pads (waveform and spectrogram) and check each one starts
   on its hit, has the energy it should (low for a kick, high for a hat, both for a snare) and ends at silence.
7. **Hand over** the `.fm1kit` path, with the pad list, and how to load it (`kits/README.md`, Loading one).

## Chopping a break

To slice hits out of a drum break, find the onsets (spectral flux on short frames), then score each hit's
first 50 ms by band: 35 to 110 Hz for a kick, 150 to 350 Hz plus 1.5 to 5 kHz for a snare, 7 kHz and up for
cymbals. Pick clean examples: a kick or snare where the other is quiet, a cymbal between hits, a quiet snare
for a ghost. Start each slice at its attack (the steepest rise in a 1 ms energy envelope, 1.5 ms early) and
end it 10 ms before the next hit's attack. Skip hits that run into other instruments. Put the times in
`kit.json` so the kit can be rebuilt from the source.
