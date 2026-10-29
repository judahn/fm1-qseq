// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
// Phase 3 of the preview check: the selection, the clipboard, undo and redo,
// and real-time takes, on a fresh simulator. Copy and paste inserting,
// undo and redo, deleting bars, a marker, an overdub with count-in and
// requantize played back on exact blocks, and a loop take. Saves
// docs/phase3-select.png, docs/phase3-rec.png and docs/phase3-recording.png.
import { simPage, savePng, checker } from './preview_lib.mjs';

export async function phase3(browser, root) {
  const page = await simPage(browser, root);
  const out = await page.evaluate(() => {
    const r = previewRig(), { B, c, same } = r, info = r.info;
    const state = i => info(29, 0, i); // 0 recording 1 taken 2 passes 3 mode 4 count-in 5 quantize 6 strength
    // 7 metronome 8 clicking 9 undo levels 10 redo levels 11 clip length 12 clip tracks 13 bars 14 quiet track
    const notesOf = t => Array.from({ length: info(0, t) }, (_, i) => ({ clock: info(1, t, i), pitch: info(2, t, i), length: info(3, t, i) }));
    const song = () => ({
      tracks: Array.from({ length: 8 }, (_, t) => notesOf(t)),
      tempos: Array.from({ length: info(4) }, (_, k) => [info(5, 0, k), info(6, 0, k)]),
      markers: Array.from({ length: info(12) }, (_, k) => info(13, 0, k)),
      end: info(11), loop: [info(14), info(15)] });
    const byClock = list => list.sort((a, b) => a.clock - b.clock || a.pitch - b.pitch || a.length - b.length);
    // Inserting `length` at `at` moves everything from there on.
    const inserted = (s, at, length) => ({ ...s,
      tracks: s.tracks.map(list => list.map(n => n.clock >= at ? { ...n, clock: n.clock + length } : n)),
      tempos: s.tempos.map(([clock, bpm], k) => [k && clock >= at ? clock + length : clock, bpm]),
      markers: s.markers.map(m => m >= at ? m + length : m), end: s.end + length,
      loop: [s.loop[0] >= at ? s.loop[0] + length : s.loop[0], s.loop[1] > at ? s.loop[1] + length : s.loop[1]] });
    const undoKey = redo => {                      // HOME, then SAVE with it, SHIFT for redo
      if (redo) c.sim_control(0, B.FX, 1);
      c.sim_control(0, B.HOME, 1); c.sim_control(0, B.SAVE, 1);
      c.sim_control(0, B.SAVE, 0); c.sim_control(0, B.HOME, 0);
      if (redo) c.sim_control(0, B.FX, 0);
    };
    const out = { arrange: {}, take: {}, loopTake: {} };
    const a = out.arrange, t = out.take, l = out.loopTake;
    r.render(32);
    r.tap(B.SEQ);

    // 1. Selection: LFO + G3 adds track 2, K3 pulls the loop end in to bars
    // 3 and 4. SHIFT+F3 copies, K1 moves to bar 5, SHIFT+A3 pastes inserting.
    r.held(B.LFO, () => r.key(2));
    r.turn(4, -2);
    a.loop = [info(14), info(15)];
    const original = song(), [from, to] = a.loop;
    r.held(B.FX, () => r.key(0));
    a.clip = [state(11), state(12)];
    r.turn(2, 4);
    a.playhead = info(8);
    r.render(4);
    a.selectPng = r.png();
    r.held(B.FX, () => r.key(4));
    const pasted = song(), want = inserted(original, to, to - from);
    for (const tr of [0, 1])
      want.tracks[tr] = byClock([...want.tracks[tr], ...original.tracks[tr].filter(n => n.clock >= from && n.clock < to)
        .map(n => ({ ...n, clock: n.clock - from + to, length: Math.min(n.length, to - n.clock) }))]);
    pasted.tracks = pasted.tracks.map(byClock); want.tracks = want.tracks.map(byClock);
    a.pasteOk = same(pasted, want);
    a.pasteDiff = a.pasteOk ? null : { end: [pasted.end, want.end], tempos: [pasted.tempos, want.tempos], markers: [pasted.markers, want.markers],
      loop: [pasted.loop, want.loop], first: pasted.tracks.map((list, tr) => list.findIndex((n, i) => !same(n, want.tracks[tr][i])))
        .map((i, tr) => i < 0 ? null : [tr, pasted.tracks[tr][i], want.tracks[tr][i]]).filter(Boolean).slice(0, 3) };
    a.levelsAfterPaste = state(9);
    // 2. Undo and redo, then delete those bars again and take that back.
    undoKey(0);
    a.undoPage = r.page();
    const undone = song(); undone.tracks = undone.tracks.map(byClock);
    a.undoOk = same(undone, { ...original, tracks: original.tracks.map(byClock) });
    a.redoLevels = state(10);
    undoKey(1);
    const redone = song(); redone.tracks = redone.tracks.map(byClock);
    a.redo = { same: same(redone, pasted), redoLevels: state(10) };
    r.held(B.FX, () => r.key(11));                 // SHIFT+E4: delete bars 3 and 4
    const deleted = song();
    a.deleted = { end: deleted.end - pasted.end, tempo: deleted.tempos[1][0] - pasted.tempos[1][0], markers: deleted.markers.length - pasted.markers.length,
      tracks: deleted.tracks.every((list, tr) => list.length === pasted.tracks[tr].filter(n => n.clock < from || n.clock >= to).length) };
    undoKey(0);
    const back = song(); back.tracks = back.tracks.map(byClock);
    a.deleteUndoOk = same(back, pasted);
    r.tap(B.SCL);                                  // a marker at the playhead, then undo it
    a.markersAdded = info(12) - pasted.markers.length;
    undoKey(0);
    a.markersAfterUndo = info(12) - pasted.markers.length;
    a.stillOnSong = r.page();

    // 3. An overdub on track 7, from the top, with a bar of count-in and
    // 16th quantize. REC held opens the record settings, on OVERDUB, a new
    // song's mode, and REC there goes (0.4.0).
    r.held(B.FX, () => r.tap(B.PLAY));             // to the top
    r.turn(0, 6);                                  // track 7
    r.long(B.REC);
    t.chooserPage = r.page();
    r.turn(4, 5);
    t.settings = [state(3), state(4), state(5), state(6), state(7)];
    r.render(4);
    t.recPng = r.png();
    c.sim_song_capture(1);
    r.tap(B.REC);
    t.page = r.page();
    t.recording = state(0);
    t.quiet = state(14);
    const bpm100 = info(6, 0, 0), beatFrames = 44100 * 60 * 100 / bpm100;
    r.render(Math.floor(beatFrames * 1.5 / 128)); // in the count-in
    const countInClock = info(28) | 0;
    r.key(7);
    r.render(Math.ceil(beatFrames * 3 / 128));     // past the count-in
    const played = [];
    const play = (k, wait, hold) => {
      r.render(wait);
      const on = info(28); c.sim_control(1, k, 1);
      r.render(hold);
      const off = info(28); c.sim_control(1, k, 0);
      played.push({ on, off });
    };
    play(9, 37, 20); play(11, 61, 13); play(12, 45, 50);
    r.render(10);
    t.recordingPng = r.png();
    play(7, 29, 30);
    r.render(40);
    r.tap(B.PLAY);                                 // PLAY ends the take, and playing goes on
    t.ended = [state(0), info(7), state(8), r.page()];
    r.render(40);
    r.tap(B.PLAY);                                 // PLAY again stops
    t.stopped = info(7);
    t.taken = state(1);
    t.countInClock = countInClock;
    const quantized = raw => { const bar = Math.floor(raw / 1920) * 1920; return bar + Math.floor((raw - bar + 60) / 120) * 120; };
    const takeNotes = () => notesOf(6).filter(n => n.clock < 2 * 1920);
    const got = takeNotes();
    t.raw = played.map(p => p.on);
    t.notes = got.map(n => n.clock);
    t.quantizedOk = got.length === 4 && got.every((n, i) => n.clock === quantized(played[i].on) && n.length === played[i].off - played[i].on);
    // The clicks: four count-in beats, accented on one, then every beat of
    // the take in the same block as the hats on those beats.
    const list = r.heard(), clicks = list.filter(h => h.track === 7 && h.pitch === 37 && h.velocity);
    const hats = list.filter(h => h.track === 7 && h.pitch === 42 && h.velocity);
    t.clickVelocities = clicks.slice(0, 6).map(h => h.velocity);
    const first = clicks[0]?.block, songStart = hats[0]?.block;
    t.countInFrames = (songStart - first) * 128;
    t.countInOk = clicks.length >= 6 && same(t.clickVelocities, [127, 80, 80, 80, 127, 80]) &&
      Math.abs(t.countInFrames - 4 * beatFrames) < 128 &&
      [1, 2, 3].every(k => Math.abs((clicks[k].block - first) * 128 - k * beatFrames) < 128);
    t.clicksOnBeats = clicks.slice(4).every((h, k) => hats[2 * k] && hats[2 * k].block === h.block);
    // Requantize from the record settings: strength 0 gives back the raw
    // timing, 100 puts it back on the grid. Then three undos take the whole
    // take away, and three redos bring it back.
    r.long(B.REC);
    r.turn(5, -20);
    t.strength0 = takeNotes().map(n => n.clock);
    r.turn(5, 20);
    t.regridded = takeNotes().map(n => n.clock);
    undoKey(0); undoKey(0); undoKey(0);
    t.undonePage = r.page();
    t.undone = { notes: takeNotes().length, strength: state(6) };
    undoKey(1); undoKey(1); undoKey(1);
    t.redone = takeNotes().map(n => n.clock);

    // 4. Playback: the take plays in exact time with the rest.
    r.tap(B.HOME);
    t.backToSong = r.page();
    r.held(B.FX, () => r.tap(B.PLAY));
    c.sim_song_capture(1);
    r.tap(B.PLAY);
    r.render(1700);
    r.tap(B.PLAY);
    const start = info(9), heard2 = r.heard().filter(h => h.track === 6), { block, frameNum, den } = r.tempo(), expected = [];
    for (const n of takeNotes()) {
      expected.push(block(start, 0, n.clock) + ':' + n.pitch + ':on');
      expected.push(block(start, 0, n.clock + n.length) + ':' + n.pitch + ':off');
    }
    const gotBlocks = heard2.map(h => h.block + ':' + h.pitch + ':' + (h.velocity ? 'on' : 'off'));
    t.playbackOk = same([...expected].sort(), [...gotBlocks].sort());
    t.clicksAfter = r.heard().filter(h => h.track === 7 && h.pitch === 37).length;

    // 5. Loop recording over bars 3 and 4, no count-in: each pass adds, and
    // the loop goes back off afterwards.
    r.long(B.REC);
    r.turn(2, 2); r.turn(3, -1);
    l.settings = [state(3), state(4)];
    const before = notesOf(6).filter(n => n.clock >= from && n.clock < to).length;
    r.tap(B.REC);
    l.loopOn = info(16) & 1;
    l.startClock = info(10);
    const passFrames = Number(frameNum(to) - frameNum(from)) / Number(den);
    r.render(50); r.key(7);
    r.render(Math.ceil(passFrames / 128));
    l.passes = state(2);
    r.key(9);
    r.render(30);
    r.tap(B.PLAY);
    l.after = [state(0), info(16) & 1, state(2)];
    l.added = notesOf(6).filter(n => n.clock >= from && n.clock < to).length - before;
    return out;
  });
  savePng(root, 'phase3-select', out.arrange, 'selectPng');
  savePng(root, 'phase3-rec', out.take, 'recPng');
  savePng(root, 'phase3-recording', out.take, 'recordingPng');
  await page.close();
  const k = checker('3'), { arrange: a, take: t, loopTake: l } = out;
  k.eq('LFO+G3 and K3: the selection is bars 3 and 4', a.loop, [3840, 7680]);
  k.eq('SHIFT+F3 copies the selection on two tracks', a.clip, [3840, 3]);
  k.eq('K1 moves the playhead to bar 5', a.playhead, 7680);
  k.yes('SHIFT+A3 pastes inserting, everything later moving on', a.pasteOk, a.pasteDiff);
  k.eq('the paste is one undo step', a.levelsAfterPaste, 1);
  k.eq('still on the song view after the edits', a.stillOnSong, 19);
  k.eq('undo stays on the song view', a.undoPage, 19);
  k.yes('undo gives back the song as it was', a.undoOk);
  k.eq('one step to redo', a.redoLevels, 1);
  k.eq('redo brings the paste back, nothing left to redo', a.redo, { same: true, redoLevels: 0 });
  const span = a.loop[1] - a.loop[0];
  k.eq('SHIFT+E4 deletes the bars, the tempo change and a marker moving left',
    a.deleted, { end: -span, tempo: -span, markers: -1, tracks: true });
  k.yes('undo puts the deleted bars back', a.deleteUndoOk);
  k.eq('SEL adds a marker', a.markersAdded, 1);
  k.eq('undo takes the marker back', a.markersAfterUndo, 0);
  k.eq('REC held opens the record settings', t.chooserPage, 23);
  k.eq('record settings: overdub, a bar of count-in, 1/16 at 100%, metronome while recording', t.settings, [2, 1, 5, 100, 2]);
  k.eq('REC there goes back to the song view', t.page, 19);
  k.eq('the take is recording', t.recording, 1);
  k.eq('overdub keeps the old notes playing (no quiet track)', t.quiet, 255);
  k.eq('PLAY ends the take: not recording, still playing, no clicks, song view', t.ended, [0, 1, 0, 19]);
  k.eq('PLAY again stops', t.stopped, 0);
  k.eq('four notes taken', t.taken, 4);
  k.yes('the take is quantized to 16ths, lengths as played', t.quantizedOk, { raw: t.raw, notes: t.notes });
  k.yes('the count-in is a bar of clicks, accented on one', t.countInOk, { velocities: t.clickVelocities, frames: t.countInFrames });
  k.yes('the clicks land with the hats on the beats', t.clicksOnBeats);
  k.eq('requantize at strength 0 gives back the raw timing', t.strength0, t.raw);
  k.eq('strength 100 puts it back on the grid', t.regridded, t.notes);
  k.eq('undo from the record settings stays there', t.undonePage, 23);
  k.eq('three undos take the take and the strength back', t.undone, { notes: 0, strength: 100 });
  k.eq('three redos bring the take back', t.redone, t.notes);
  k.eq('HOME goes back to the song view', t.backToSong, 19);
  k.yes('the take plays back on exact blocks', t.playbackOk);
  k.eq('no clicks when only playing', t.clicksAfter, 0);
  k.eq('loop recording, no count-in', l.settings, [4, 0]);
  k.eq('loop recording turns the loop on', l.loopOn, 1);
  k.eq('loop recording starts at the loop start', l.startClock, 3840);
  k.eq('one pass done', l.passes, 1);
  k.eq('after: not recording, the loop back off, one pass', l.after, [0, 0, 1]);
  k.eq('each pass added its note', l.added, 2);
  return { phase: '3', checks: k.list, out };
}
