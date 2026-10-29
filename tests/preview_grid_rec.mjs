// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
// Recording into the grid (0.3.0) in the preview check. From a stop, REC on
// an empty cell plays the count-in and records a 1-bar clip from the top:
// keys play the track and go in, the take ends by itself after the bar, the
// clip plays on, launched, and the launch keys launch again. A FREE take
// started while playing (launch quantize off: the next beat) ends at the bar
// line after REC. REC on the clip now playing overdubs it from its next
// pass, and PLAY ends that with playing going on. PLAY in a count-in calls
// the take off and stops. Each take is one undo step, and undo takes a new
// clip away with it. Saves docs/grid-rec-*.png.
import { simPage, savePng, checker } from './preview_lib.mjs';

export async function phaseGridRec(browser, root) {
  const page = await simPage(browser, root);
  const out = await page.evaluate(() => {
    const r = previewRig(), { B, c } = r, info = r.info, render = n => r.render(n);
    const cell = (t, row) => t * 16 + row;
    const state = () => info(36) & 255, recCell = () => info(36) >> 8 & 255, undos = () => info(29, 0, 9);
    const from = t => info(41, t) & 255, waiting = t => info(41, t) >> 8 & 255;
    const until = (test, most = 4000) => { let n = 0; while (!test() && n < most) { render(1); n++; } return n; };
    const hold = (k, blocks) => { c.sim_control(1, k, 1); render(blocks); c.sim_control(1, k, 0); };
    const clipNotes = (t, row) => Array.from({ length: info(40, 0, cell(t, row)) }, (_, i) =>
      ({ clock: info(1, 8 + cell(t, row), i), pitch: info(2, 8 + cell(t, row), i) })).filter((n, i) => !info(30, 8 + cell(t, row), i));
    const o = {};
    render(32);
    r.tap(B.SEQ); r.tap(B.SEQ);
    o.grid = r.page();
    r.turn(3, -2);                                  // K2: REC LENGTH 1 bar
    render(2);
    o.lengthKnob = r.knobs()[1];

    // 1. From a stop, track 1 row 1, empty: the count-in, then a bar.
    const a = o.fromStop = {}, before = undos();
    r.tap(B.REC);
    a.armed = [state(), recCell(), info(7), r.page()];
    a.counting = info(28) | 0;                      // the key clock: -1 in the count-in
    render(4);
    a.armedPng = r.png();
    a.countIn = until(() => state() & 2);
    a.recording = state();
    render(100);
    hold(7, 30);                                    // C4
    render(60);
    hold(9, 30);                                    // D4
    render(20);
    a.recordingPng = r.png();
    a.ending = until(() => !state());
    a.after = { state: state(), length: info(39, 0, cell(0, 0)), notes: clipNotes(0, 0).length, playing: info(7), from: from(0),
      undo: undos() - before };
    const pitches = clipNotes(0, 0).map(n => n.pitch);
    c.sim_song_capture(1);
    render(720);                                    // a bar more: the clip plays on
    a.replayed = r.heard().filter(h => h.track === 0 && h.velocity && pitches.includes(h.pitch)).length;
    r.key(4);                                       // A3, track 3's key: a launch again (its empty cell stops it)
    a.launchKey = waiting(2);

    // 2. FREE while playing, track 2 row 1, the launch quantize off: it starts
    // on the next beat, and REC late in its second bar ends it at that bar line.
    const f = o.free = {};
    r.turn(0, 1);                                   // track 2
    r.turn(3, 5);                                   // K2: FREE
    r.turn(5, -2);                                  // K4: launch quantize off
    render(2);
    f.knobs = [r.knobs()[1], r.knobs()[3]];
    r.tap(B.REC);
    f.armed = state();
    f.beat = ((info(45, 1) - (info(43) - info(8))) % 480 + 480) % 480; /* its launch waits for a beat */
    until(() => state() & 2);
    f.recording = state();
    render(50);
    hold(14, 30);                                   // G4
    until(() => info(28) > 1920 + 1500);            // late in its second bar
    r.tap(B.REC);
    f.ending = state();
    f.ended = until(() => !state());
    f.after = { length: info(39, 0, cell(1, 0)), notes: clipNotes(1, 0).length, playing: info(7), from: from(1) };

    // 3. Overdub: REC on track 1's clip, playing: from its next pass, PLAY ends it.
    const d = o.overdub = {};
    r.turn(0, -1);                                  // track 1, row 1
    const had = clipNotes(0, 0).length, steps = undos();
    r.tap(B.REC);
    d.armed = [state(), info(47) >> 8 & 255];        // armed for the next pass
    until(() => state() & 2);
    d.recording = state();
    render(30);
    hold(11, 30);                                   // E4
    render(10);
    r.tap(B.PLAY);                                  // PLAY ends it, playing goes on
    d.after = { state: state(), playing: info(7), notes: clipNotes(0, 0).length - had, length: info(39, 0, cell(0, 0)), from: from(0),
      undo: undos() - steps };

    // 4. From a stop again, track 3 row 1: PLAY in the count-in calls it off.
    const x = o.countIn = {};
    r.tap(B.PLAY);                                  // stop
    const steps2 = undos();
    r.turn(0, 2);
    r.tap(B.REC);
    x.armed = [state(), info(7)];
    render(100);
    r.tap(B.PLAY);
    x.after = { state: state(), playing: info(7), length: info(39, 0, cell(2, 0)), undo: undos() - steps2 };

    // 5. Undo takes the overdub back, then the FREE clip and the first clip whole.
    r.turn(0, -2);
    const undo = () => { c.sim_control(0, B.HOME, 1); r.tap(B.SAVE); c.sim_control(0, B.HOME, 0); };
    undo();
    o.undoOverdub = clipNotes(0, 0).length;
    undo();
    o.undoFree = info(39, 0, cell(1, 0));
    undo();
    o.undoFirst = [info(39, 0, cell(0, 0)), info(40, 0, cell(0, 0))];
    o.page = r.page();
    return o;
  });
  savePng(root, 'grid-rec-armed', out.fromStop, 'armedPng');
  savePng(root, 'grid-rec-recording', out.fromStop, 'recordingPng');
  await page.close();
  const t = checker('grid rec'), a = out.fromStop, f = out.free, d = out.overdub, x = out.countIn;
  t.eq('the grid', out.grid, 27);
  t.eq('K2 is the REC length, here 1 bar', out.lengthKnob, 'REC LEN 1 BAR');
  t.eq('REC from a stop: armed (a new clip), cell 1, playing, the grid stays', a.armed, [1 | 8, 0, 1, 27]);
  t.eq('the count-in: keys land nowhere yet', a.counting, -1);
  t.over('the count-in lasts', a.countIn, 600);
  t.eq('then recording', a.recording, 2 | 8);
  t.under('it ended by itself within the bar', a.ending, 700);
  t.eq('after: no take, a 1-bar clip with the 2 notes, playing on, launched on track 1, one undo step', a.after,
    { state: 0, length: 1920, notes: 2, playing: 1, from: 0, undo: 1 });
  t.atLeast('the clip plays on: its notes again', a.replayed, 2);
  t.eq('and the track keys launch again', a.launchKey, 0xfe);
  t.eq('FREE with the launch quantize off', f.knobs, ['REC LEN FREE', 'LAUNCH Q OFF']);
  t.eq('armed, a new FREE clip', f.armed, 1 | 4 | 8);
  t.eq('recording, no end yet', f.recording, 2 | 4 | 8);
  t.eq('REC: its end set, still recording', f.ending, 2 | 8);
  t.under('it ends at that bar line', f.ended, 700);
  t.eq('2 bars long, its note in, playing on, launched', f.after, { length: 3840, notes: 1, playing: 1, from: 0 });
  t.eq('it starts on a beat', f.beat, 0);
  t.eq('overdub: armed for the clip\'s next pass', d.armed, [1, 1]);
  t.eq('then recording into it (no new clip)', d.recording, 2);
  t.eq('PLAY ends it: playing goes on, one note more, its length kept, still launched, one undo step', d.after,
    { state: 0, playing: 1, notes: 1, length: 1920, from: 0, undo: 1 });
  t.eq('REC from a stop (FREE): armed, playing the count-in', x.armed, [1 | 4 | 8, 1]);
  t.eq('PLAY in the count-in: called off, stopped, no clip, no undo step', x.after, { state: 0, playing: 0, length: 0, undo: 0 });
  t.eq('undo: the overdub\'s note goes', out.undoOverdub, 2);
  t.eq('undo: the FREE clip goes', out.undoFree, 0);
  t.eq('undo: the first clip goes, notes and all', out.undoFirst, [0, 0]);
  t.eq('still on the grid', out.page, 27);
  return { phase: 'grid rec', checks: t.list, out };
}
