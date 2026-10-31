// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
// Phase 7a of the preview check: the clip grid through the panel. SEQ opens
// it, SEL makes a clip and opens it in the song view's editor, step entry
// fills it, CLIP END sets a second clip's length, a track key launches a clip
// (starting playback) and only launched clips play in the grid (0.4.0), a
// scene key launches row 1 on every track at the next bar line, checked
// against exact block arithmetic, then the SONG row's key stops a track and
// SEQ to the song view hands every track back to the song. Copy, paste,
// delete and undo on a cell, a
// row name (YES keeps it, NO leaves the old one), the launch quantize, the
// clips surviving a save and a load, K3 on the grid setting a clip's
// length, stopped and playing (0.5.3), and SHIFT+D#4 clearing the clipboard
// in the grid and the song view (0.5.4).
// Saves docs/phase7-*.png.
import { simPage, savePng, checker } from './preview_lib.mjs';

export async function phase7(browser, root) {
  const page = await simPage(browser, root);
  const out = await page.evaluate(() => {
    const r = previewRig(), { B, c } = r;
    const { tap, key, held, turn, render, info, state, png } = r, pageNow = r.page;
    const keyLed = k => state()[168 + 14 + k];
    const from = t => info(41, t) & 255, waiting = t => info(41, t) >> 8;
    const cell = (t, row) => t * 16 + row;
    const part = (t, row) => 8 + cell(t, row);
    const notes = p => Array.from({ length: info(0, p) }, (_, i) => ({ clock: info(1, p, i), pitch: info(2, p, i), length: info(3, p, i) }));
    const heard = () => r.heard().filter(h => !h.kind);
    // Exact block of a transport clock, for playing that began at block `start`
    // on clock 0 with no loop or jump since, so transport and song agree.
    const tempo = r.tempo(), blockOf = (start, clock) => tempo.block(start, 0, clock);
    const o = {};

    // 1. SEQ, SEQ: the grid. SEL on the empty cell of track 1, row 1: a new
    // one-bar clip in the editor. Step entry: four quarter notes. SEQ back.
    render(32);
    tap(B.SEQ); tap(B.SEQ);
    o.gridPage = pageNow();
    o.emptyPng = png();
    tap(B.SCL);
    o.clipPage = pageNow();
    o.opened = info(42) & 255;
    o.newLength = info(39, 0, cell(0, 0));
    r.step();                                       // SHIFT+REC: step entry in the clip
    o.stepPage = pageNow();
    held(B.FX, () => key(4));                       // SHIFT+A3: quarters
    for (const k of [0, 4, 7, 11]) key(k);
    turn(6, -4);                                    // the pointer back over them, for the picture
    o.stepPng = png();
    tap(B.SEQ);
    o.backToGrid = pageNow();
    o.closed = info(42) & 255;
    o.clip1 = notes(part(0, 0));
    // 2. Track 2's clip, two bars long by K4, two half notes in each bar.
    turn(0, 1);
    tap(B.SCL);
    turn(5, 1);                                     // CLIP END to bar 3 (K4 since 0.5.2)
    r.step();
    held(B.FX, () => key(2));                       // SHIFT+G3: halves
    for (const k of [0, 7, 4, 11]) key(k);
    r.step();                                       // SHIFT+REC: out of step entry, still in the clip
    o.inClipPage = pageNow();
    o.clipPng = png();
    tap(B.SEQ);
    o.clip2 = { length: info(39, 0, cell(1, 0)), notes: notes(part(1, 0)) };
    o.song2 = info(0, 1);                           // the song's own track 2, untouched

    // 3. F3 launches track 1's clip in row 1 while stopped: playing starts and
    // the clip plays from its start, bar after bar.
    turn(0, -1);
    c.sim_song_capture(1);
    key(0);
    o.playing = info(7);
    const start = info(9), at0 = info(10);
    render(1400);                                   // into bar 3
    o.from0 = from(0);
    o.ledLit = keyLed(0) === 1 && keyLed(2) === 0;
    let h = heard();
    const clipPitches = o.clip1.map(n => n.pitch);
    const want = [];
    for (let bar = 0; bar < 2; bar++) for (const n of o.clip1) want.push({ pitch: n.pitch, block: blockOf(start, at0 + bar * 1920 + n.clock) });
    const got = h.filter(x => x.track === 0 && x.velocity).slice(0, want.length);
    o.clipTiming = { want: want.length, ok: JSON.stringify(got.map(x => [x.pitch, x.block])) === JSON.stringify(want.map(x => [x.pitch, x.block])) };
    o.othersSilent = h.filter(x => x.track !== 0 && x.velocity).length;

    // 4. G4 launches row 1 everywhere at the next bar line: tracks 1 and 2
    // play their clips, the rest stop. The waiting keys blink meanwhile.
    key(14);
    const launchAt = info(45, 1);
    o.waiting = [0, 1, 2, 7].map(waiting);
    render(3);
    o.gridPng = png();
    const blinks = new Set();
    for (let i = 0; i < 40 && waiting(1) !== 0xfd; i++) { render(4); blinks.add(keyLed(2)); }
    o.blinked = blinks.size === 2;
    render(1500);
    o.sources = [0, 1, 2, 3, 4, 5, 6, 7].map(from);
    h = heard();
    const bar = blockOf(start, launchAt);
    o.launchBar = launchAt % 1920 === 0;
    const first1 = h.find(x => x.track === 1 && x.velocity && x.block >= bar - 2);
    o.track2OnTheBar = first1 && first1.block === bar && first1.pitch === o.clip2.notes[0].pitch;
    o.stoppedAfter = h.filter(x => x.track >= 2 && x.velocity).length;
    o.clip2Repeats = h.filter(x => x.track === 1 && x.velocity && x.block >= bar).length;
    o.relaunched = h.some(x => x.track === 0 && x.velocity && x.block === bar && x.pitch === clipPitches[0]);

    // 5. The SONG row's F3 stops track 1 alone, since the grid plays no song.
    // SEQ to the song view: every track goes back to the song at the bar.
    turn(2, -1);
    key(0);
    render(1500);
    o.afterSongRow = [0, 1].map(from);
    tap(B.SEQ);
    o.seqPage = pageNow();
    o.allWaiting = [0, 1, 2].map(waiting);
    render(1500);
    o.afterSeq = [0, 1, 2, 7].map(from);
    o.songAgain = heard().filter(x => x.track === 5 && x.velocity && x.block > bar + 3000).length > 0;
    tap(B.PLAY);
    o.stopped = info(7);
    c.sim_song_capture(0);
    tap(B.SEQ);                                     // the grid again

    // 6. Copy row 1's clip to row 2, delete it, undo. Name row 1. Launch quantize.
    turn(2, 1);
    held(B.FX, () => key(0));                       // copy
    turn(2, 1);
    held(B.FX, () => key(4));                       // paste
    o.pasted = [info(39, 0, cell(0, 1)), info(40, 0, cell(0, 1))];
    held(B.FX, () => key(11));                      // delete
    o.deleted = info(39, 0, cell(0, 1));
    c.sim_control(0, B.HOME, 1); tap(B.SAVE); c.sim_control(0, B.HOME, 0); // undo
    o.undone = [info(39, 0, cell(0, 1)), info(40, 0, cell(0, 1))];
    o.afterUndoPage = pageNow();
    turn(2, -1);
    tap(B.ENV); turn(3, 1); turn(2, 1); turn(3, 2);
    o.whileNaming = info(46, 0, 0);                 // the row keeps its name until YES
    render(2);
    o.namingKnobs = r.knobs().slice(0, 2);
    tap(B.ENV); tap(B.SAVE); render(2);             // neither ends the naming now
    o.stillNaming = r.knobs()[0];
    tap(B.YES);
    o.rowName = info(46, 0, 0);
    tap(B.ENV); turn(3, 3); turn(2, 1); turn(3, 1); tap(B.NO);
    o.cancelled = info(46, 0, 0);
    turn(5, -1);
    o.launchQ = info(42) >> 8;
    o.namedPng = png();
    // 7. SAVE keeps the grid: saved on the list SAVE opens, then row 1's clip
    // deleted in the grid, and the song loaded back.
    tap(B.SAVE); tap(B.SAVE); tap(B.HOME); tap(B.SEQ);
    held(B.FX, () => key(0));                        // copy, then delete it
    held(B.FX, () => key(11));
    o.beforeLoad = info(39, 0, cell(0, 0));
    held(B.FX, () => tap(B.SAVE));
    o.listPage = pageNow();
    tap(B.YES);                                      // the clip deleted is unsaved: YES asks first
    o.askedFirst = info(39, 0, cell(0, 0));
    tap(B.YES);
    tap(B.HOME);
    o.loaded = [info(39, 0, cell(0, 0)), info(40, 0, cell(0, 0)), info(39, 0, cell(1, 0)), info(46, 0, 0), info(42) >> 8];

    // 8. K3 sets the length of the clip at the cursor (0.5.3): a bar a detent,
    // SHIFT a beat, at least a bar unshifted, 16 bars at most. The notes past
    // the end stay, and come back as it grows. An empty cell has none to set.
    // Shortened while it plays, the clip plays its first bar only, and every
    // note it started ends.
    tap(B.SEQ);
    if (pageNow() !== 27) tap(B.SEQ);
    const k3 = () => (render(2), r.knobs()[2]), length2 = () => info(39, 0, cell(1, 0));
    turn(0, 1);                                     // track 2, row 1: two bars
    o.k3 = { page: pageNow(), before: k3() };
    turn(4, -1);
    o.k3.shorter = [length2(), info(0, part(1, 0)), k3()];
    turn(4, -1);
    o.k3.floor = length2();
    held(B.FX, () => turn(4, -1));
    o.k3.beat = [length2(), k3()];
    turn(4, 1);
    o.k3.snapped = length2();
    turn(4, 1);
    o.k3.grown = [length2(), notes(part(1, 0)).map(n => n.clock)];
    turn(4, 30);
    o.k3.most = [length2(), k3()];
    turn(4, -14);
    o.k3.back = length2();
    turn(2, 1);                                     // row 2: empty
    turn(4, 1);
    o.k3.empty = [info(39, 0, cell(1, 1)), k3()];
    turn(2, -1);
    c.sim_song_capture(1);
    key(2);                                         // G3 launches track 2's clip, playing starts
    render(1400);                                   // into its second bar
    turn(4, -1);
    const mark = heard().length, late = new Set(o.clip2.notes.slice(2).map(n => n.pitch));
    render(1400);
    const after = heard().slice(mark).filter(x => x.track === 1 && x.velocity);
    o.k3.playing = { notes: after.length > 1, firstBarOnly: after.every(x => !late.has(x.pitch)) };
    tap(B.PLAY);
    render(50);
    o.k3.ended = r.balanced(heard().filter(x => x.track === 1));
    c.sim_song_capture(0);

    // 9. SHIFT+D#4 empties the clipboard (0.5.4), in the grid and the song
    // view, and the song view's paste outline goes with it.
    const board = () => [info(29, 0, 11), info(29, 0, 12)];
    held(B.FX, () => key(0));                       // copy track 2's clip
    o.board = { copied: board()[0] };
    held(B.FX, () => key(10));                      // SHIFT+D#4
    o.board.gridCleared = board();
    held(B.FX, () => key(4));                       // paste: nothing to paste
    o.board.pasteless = [info(39, 0, cell(1, 0)), info(40, 0, cell(1, 0))];
    tap(B.SEQ);                                     // the song view, stopped
    render(4);
    const bare = r.pixels();
    held(B.FX, () => key(0));                       // copy the selection: the outline shows
    render(4);
    const outlined = r.pixels();
    held(B.FX, () => key(10));
    render(4);
    o.board.view = { page: pageNow(), outline: r.differ(outlined, 0, 40, 240, 178, bare) > 0, cleared: board(),
      left: r.differ(r.pixels(), 0, 40, 240, 178, bare) };
    return o;
  });
  for (const [name, key] of [['grid-empty', 'emptyPng'], ['step', 'stepPng'], ['clip', 'clipPng'], ['grid', 'gridPng'],
    ['named', 'namedPng']]) savePng(root, 'phase7-' + name, out, key);
  await page.close();
  const t = checker('7'), q = out;
  t.eq('SEQ, SEQ opens the grid', q.gridPage, 27);
  t.eq('SEL opens the cell in the song view\'s editor', q.clipPage, 19);
  t.eq('the clip open is track 1, row 1', q.opened, 8);
  t.eq('an empty cell gets a new clip a bar long', q.newLength, 1920);
  t.eq('SHIFT+REC: step entry in the clip', q.stepPage, 20);
  t.eq('SEQ closes the clip and goes back to the grid', q.backToGrid, 27);
  t.eq('no clip open after', q.closed, 0);
  t.eq('the four quarters are in the clip', q.clip1.map(n => n.clock), [0, 480, 960, 1440]);
  t.eq('SHIFT+REC leaves step entry, still in the clip', q.inClipPage, 19);
  t.eq('CLIP END (K4) made the second clip two bars long', q.clip2.length, 3840);
  t.eq('the second clip\'s half notes', q.clip2.notes.map(n => n.clock), [0, 960, 1920, 2880]);
  t.over('the song\'s own track 2 untouched', q.song2, 0);
  t.eq('F3 launches while stopped: playing starts', q.playing, 1);
  t.eq('track 1 plays its row 1 clip', q.from0, 0);
  t.yes('track 1\'s key lit, track 2\'s not', q.ledLit);
  t.yes('the clip plays from its start, bar after bar, on exact blocks', q.clipTiming.ok, q.clipTiming);
  t.eq('in the grid the other tracks, with nothing launched, are silent', q.othersSilent, 0);
  t.eq('G4: tracks 1 and 2 wait for row 1, the rest stay stopped', q.waiting, [0, 0, 0xfd, 0xfd]);
  t.yes('a waiting key blinks', q.blinked);
  t.eq('after the bar line: clips on tracks 1 and 2, the rest stopped', q.sources, [0, 0, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe]);
  t.yes('the scene launch waits for a bar line', q.launchBar);
  t.yes('track 2\'s clip starts exactly on the bar line', q.track2OnTheBar);
  t.eq('no notes from the stopped tracks at all', q.stoppedAfter, 0);
  t.atLeast('track 2\'s clip notes after the launch', q.clip2Repeats, 3);
  t.yes('track 1\'s clip launched again restarts on the bar line', q.relaunched);
  t.eq('the SONG row\'s F3 stops track 1 alone', q.afterSongRow, [0xfe, 0]);
  t.eq('SEQ goes to the song view', q.seqPage, 19);
  t.eq('there every track waits to go back to the song', q.allWaiting, [0xff, 0xff, 0xff]);
  t.eq('after the bar line, every track on the song', q.afterSeq, [0xff, 0xff, 0xff, 0xff]);
  t.yes('the song plays again on a stopped track', q.songAgain);
  t.eq('PLAY stops', q.stopped, 0);
  t.eq('SHIFT+F3, then SHIFT+A3 on row 2: the clip copied there', q.pasted, [1920, 4]);
  t.eq('SHIFT+E4 deletes it', q.deleted, 0);
  t.eq('undo brings it back', q.undone, [1920, 4]);
  t.eq('undo stays on the grid', q.afterUndoPage, 27);
  t.eq('naming: the row keeps its old name until YES', q.whileNaming, 0);
  t.eq('naming: K1 the letter, K2 the name', q.namingKnobs, ['LETTER 2', 'NAME AB']);
  t.eq('naming: ENV and SAVE don\'t end it', q.stillNaming, 'LETTER 2');
  t.eq('ENV names row 1 "AB", YES keeps it', q.rowName & 0xffff, 0x4241);
  t.eq('naming again and NO: the old name stays', q.cancelled, q.rowName);
  t.eq('K4 sets the launch quantize to a beat', q.launchQ, 1);
  t.eq('the clip deleted before loading', q.beforeLoad, 0);
  t.eq('SHIFT+SAVE opens the song list too', q.listPage, 25);
  t.eq('YES with unsaved changes asks before loading', q.askedFirst, 0);
  t.eq('loaded back: both clips, the row name and the launch quantize', q.loaded, [1920, 4, 3840, q.rowName, 1]);
  const k3 = q.k3;
  t.eq('K3: on the grid, the clip at the cursor is two bars', [k3.page, k3.before], [27, 'CLIP 2 BARS']);
  t.eq('K3 left: a bar shorter, its notes all kept', k3.shorter, [1920, 4, 'CLIP 1 BAR']);
  t.eq('K3 left again: a bar is the least without SHIFT', k3.floor, 1920);
  t.eq('SHIFT+K3: a beat shorter', k3.beat, [1440, 'CLIP 3 BEATS']);
  t.eq('K3 right from 3 beats steps onto the bar', k3.snapped, 1920);
  t.eq('K3 right: two bars, the hidden notes back', k3.grown, [3840, [0, 960, 1920, 2880]]);
  t.eq('K3 stops at 16 bars', k3.most, [30720, 'CLIP 16 BARS']);
  t.eq('and comes back down a bar a detent', k3.back, 3840);
  t.eq('K3 on an empty cell makes no clip', k3.empty, [0, 'CLIP EMPTY']);
  t.eq('shortened while playing, it plays its first bar only', k3.playing, { notes: true, firstBarOnly: true });
  t.eq('every note it started ends', k3.ended, true);
  const b = q.board;
  t.eq('SHIFT+F3 in the grid copies the clip', b.copied, 1920);
  t.eq('SHIFT+D#4 in the grid clears the clipboard', b.gridCleared, [0, 0]);
  t.eq('pasting after leaves the cell as it was', b.pasteless, [1920, 4]);
  t.yes('in the song view a copy shows the paste outline', b.view.outline && b.view.page === 19, b.view);
  t.eq('SHIFT+D#4 there clears it, outline and all', [b.view.cleared, b.view.left], [[0, 0], 0]);
  return { phase: '7', checks: t.list, out };
}
