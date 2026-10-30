// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
// Recording into the grid (0.3.0) in the preview check. From a stop, REC on
// an empty cell plays the count-in and records a 1-bar clip from the top:
// keys play the track and go in, the take ends by itself after the bar, the
// clip plays on, launched, and the launch keys launch again. A FREE take
// started while playing (launch quantize off: the next beat) ends at the bar
// line after REC. REC on the clip now playing overdubs it from its next
// pass, and PLAY ends that with playing going on. With SHIFT+SEL on REPLACE
// (0.4.0), REC empties the clip first, keeping its length, and records into
// it. PLAY in a count-in calls the take off and stops. Each take is one undo
// step, and undo takes a new clip away with it, or gives a replaced clip
// back. The catch (0.5.1), on a fresh FM-1: a key in the count-in's last
// 94 ms lands on the first beat, as long as it was held if let go before
// it, one just before the clip's end on the next pass's first beat, and one
// just before a cell's launch on its first beat.
// Saves docs/grid-rec-*.png.
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
    r.key(0);                                       // F3, track 1's key: a launch again
    a.launchKey = waiting(0);

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

    // 3b. REPLACE: SHIFT+SEL switches to it, and REC on track 1's clip,
    // playing, empties it at once, keeping its length, then records into it
    // from its next pass. SHIFT+SEL again: OVERDUB.
    const p = o.replace = {};
    r.held(B.FX, () => r.tap(B.SCL));
    p.mode = info(29, 0, 3);
    const steps3 = undos();
    r.tap(B.REC);
    p.emptied = { notes: clipNotes(0, 0).length, length: info(39, 0, cell(0, 0)), state: state() };
    until(() => state() & 2);
    render(30);
    hold(14, 30);                                   // G4
    render(10);
    r.tap(B.PLAY);                                  // PLAY ends it, playing goes on
    p.after = { state: state(), playing: info(7), pitches: clipNotes(0, 0).map(n => n.pitch), length: info(39, 0, cell(0, 0)),
      from: from(0), undo: undos() - steps3 };
    r.held(B.FX, () => r.tap(B.SCL));
    p.back = info(29, 0, 3);

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

    // 5. Undo gives the replaced clip back, takes the overdub back, then the
    // FREE clip and the first clip whole.
    r.turn(0, -2);
    const undo = () => { c.sim_control(0, B.HOME, 1); r.tap(B.SAVE); c.sim_control(0, B.HOME, 0); };
    undo();
    o.undoReplace = clipNotes(0, 0).length;
    undo();
    o.undoOverdub = clipNotes(0, 0).length;
    undo();
    o.undoFree = info(39, 0, cell(1, 0));
    undo();
    o.undoFirst = [info(39, 0, cell(0, 0)), info(40, 0, cell(0, 0))];
    o.page = r.page();

    // 6. The catch (0.5.1), on a fresh FM-1. From a stop, REC on an empty
    // cell, 1 bar: a key as the catch opens, in the count-in, goes on the
    // first beat, and one just before the bar's end on the next pass's.
    // Then, playing, REC on track 2's empty cell waits for the next bar: a
    // key just before it, held over it, goes on that clip's first beat.
    const g = previewRig(), gi = g.info, gr = n => g.render(n), k = o.catch = {};
    const gUntil = (test, most = 4000) => { let n = 0; while (!test() && n < most) { gr(1); n++; } return n; };
    const gHold = (key, blocks) => { g.c.sim_control(1, key, 1); gr(blocks); g.c.sim_control(1, key, 0); };
    const gNotes = (t, row) => Array.from({ length: gi(40, 0, cell(t, row)) }, (_, i) =>
      ({ clock: gi(1, 8 + cell(t, row), i), pitch: gi(2, 8 + cell(t, row), i), length: gi(3, 8 + cell(t, row), i) }))
      .filter((n, i) => !gi(30, 8 + cell(t, row), i));
    gr(32);
    g.tap(B.SEQ); g.tap(B.SEQ);
    g.turn(3, -2);                                  // K2: REC LENGTH 1 bar
    g.tap(B.REC);
    gUntil(() => (gi(28) | 0) >= 0);                // the catch opens
    gHold(5, 10);                                   // A#3, let go before the beat: as long as it was held
    g.c.sim_control(1, 7, 1);                       // C4 down
    k.early = 10 + gUntil(() => gi(8) > 0);         // blocks to the first beat
    gr(30);
    g.c.sim_control(1, 7, 0);
    gUntil(() => (gi(28) | 0) >= 1880);             // 40 clocks before the clip's end
    gHold(9, 10);                                   // D4, let go before the end
    gUntil(() => !(gi(36) & 255));                  // the take ends with the bar
    k.first = gNotes(0, 0).map(n => ({ clock: n.clock, pitch: n.pitch }));
    k.tapped = (gNotes(0, 0).find(n => n.pitch === 58) || {}).length;
    g.turn(0, 1);                                   // track 2
    g.tap(B.REC);                                   // its launch waits for the next bar
    k.waiting = gi(36) & 3;                         // armed
    gUntil(() => gi(45, 1) > gi(43) && gi(45, 1) - gi(43) <= 40);
    gHold(14, 30);                                  // G4, held over the launch
    gUntil(() => !(gi(36) & 255));                  // its take ends with the bar
    k.launched = gNotes(1, 0).map(n => ({ clock: n.clock, pitch: n.pitch }));
    return o;
  });
  savePng(root, 'grid-rec-armed', out.fromStop, 'armedPng');
  savePng(root, 'grid-rec-recording', out.fromStop, 'recordingPng');
  await page.close();
  const t = checker('grid rec'), a = out.fromStop, f = out.free, d = out.overdub, p = out.replace, x = out.countIn;
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
  t.eq('and the track keys launch again', a.launchKey, 0);
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
  t.eq('SHIFT+SEL: REPLACE', p.mode, 1);
  t.eq('REC on the clip in REPLACE: emptied at once, its length kept, armed for its next pass', p.emptied,
    { notes: 0, length: 1920, state: 1 });
  t.eq('PLAY ends it: playing on, only the new note in it, its length kept, still launched, one undo step', p.after,
    { state: 0, playing: 1, pitches: [67], length: 1920, from: 0, undo: 1 });
  t.eq('SHIFT+SEL again: OVERDUB', p.back, 2);
  t.eq('REC from a stop (FREE): armed, playing the count-in', x.armed, [1 | 4 | 8, 1]);
  t.eq('PLAY in the count-in: called off, stopped, no clip, no undo step', x.after, { state: 0, playing: 0, length: 0, undo: 0 });
  t.eq('undo: the replaced clip comes back whole', out.undoReplace, 3);
  t.eq('undo: the overdub\'s note goes', out.undoOverdub, 2);
  t.eq('undo: the FREE clip goes', out.undoFree, 0);
  t.eq('undo: the first clip goes, notes and all', out.undoFirst, [0, 0]);
  t.eq('still on the grid', out.page, 27);
  const k = out.catch;
  t.yes('the catch: a key about 94 ms before the first beat, 30 to 35 blocks', k.early >= 30 && k.early <= 35, k.early);
  t.eq('the catch: those keys and one just before the clip\'s end, all on its first beat', k.first,
    [{ clock: 0, pitch: 58 }, { clock: 0, pitch: 60 }, { clock: 0, pitch: 62 }]);
  t.yes('the catch: the key let go before the beat keeps its length, 10 blocks, about 28 clocks', k.tapped >= 25 && k.tapped <= 31, k.tapped);
  t.eq('the catch: REC while playing waits for the launch', k.waiting, 1);
  t.eq('the catch: a key just before the launch, held over it, on the new clip\'s first beat', k.launched, [{ clock: 0, pitch: 67 }]);
  return { phase: 'grid rec', checks: t.list, out };
}
