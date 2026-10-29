# Changelog

The version is `FM1_QY_VERSION` in `app/version.h` (semver), and each release is tagged `v<version>`.

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
