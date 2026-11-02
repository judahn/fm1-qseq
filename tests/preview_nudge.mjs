// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
// The nudge phase of the preview check (0.6.7, 0.6.8): step entry's MOVE and
// NUDGE, and PRESETS on the step grid. First the owner's case on 0.6.7,
// past the test song's end on track 8 at 1/16: a lower pad on steps 1, 3, 7
// and 9, a higher one on 5 and 11 to 13, the first beat swept. SHIFT+K1
// was refused there at the second click. Three clicks and a NUDGE go now,
// the same three notes selected all the way, the footer saying MOVE 1/16
// and NUDGE +5, and it is one undo step. Then on track 1 a C4 and E4 chord,
// a G4 a quarter on and another E4 a half on, then rests into an empty bar.
// SEL picks E4 at the chord. SHIFT+K2 nudges it 15 clocks, the pointer with
// it, SHIFT+K1 moves it past the G4, which doesn't join it, and SHIFT+K2
// back onto the grid line. A quarter more lands on the other E4, which is
// replaced, a quarter more puts it back, and a quarter back replaces it
// again, where the run stops. All of it is one undo step: HOME+SAVE puts
// both E4s back and SHIFT+HOME+SAVE replaces the one again. Playing, NUDGE
// still moves it, and it plays where it went. Then in the empty bar, a walk
// at 1/64 and 1/2: the pointer stays, NUDGE +90, PRESETS lands on the half
// bar, then the bar line, then the half bar before. Saves docs/step-nudge.png.
import { simPage, savePng, checker } from './preview_lib.mjs';

export async function phaseNudge(browser, root) {
  const page = await simPage(browser, root);
  const out = await page.evaluate(() => {
    const r = previewRig(), { B, c } = r, o = {};
    const { tap, key, held, turn, render, info } = r;
    const E4 = 64;
    const press = k => c.sim_control(1, k, 1), lift = k => c.sim_control(1, k, 0);
    const shift = down => c.sim_control(0, B.FX, down ? 1 : 0);
    render(32);
    tap(B.SEQ);
    turn(6, 12);                                    // the playhead to bar 13, past the song's end
    const p0 = info(8);
    const notes = t => Array.from({ length: info(0, t) }, (_, i) => i).filter(i => !info(30, t, i))
      .map(i => [info(1, t, i) - p0, info(2, t, i)]).filter(n => n[0] >= 0).sort((a, b) => a[0] - b[0] || a[1] - b[1]);
    const pointer = () => info(17) - p0, count = () => info(58, 0, 5), undos = () => info(29, 0, 9);
    const moved = () => info(65) & 0xffff, covered = () => info(65) >>> 16;
    const picks = () => [0, 1, 2, 3].map(i => info(64, 0, i));

    // 1. The owner's case on track 8, at 1/16.
    turn(0, 7);
    r.step();
    o.stepPage = r.page();
    held(B.FX, () => key(7));                       // SHIFT+C: 1/16
    const rest = () => held(B.FX, () => key(8));    // SHIFT+C#
    key(0); rest(); key(0); rest(); key(2); rest(); key(0); rest(); key(0); rest(); key(2); key(2); key(2);
    const pads = notes(7);
    o.pads = pads.map(n => n[0]);
    const low = pads[0][1], high = pads[2][1];
    o.layout = [low < high, pads.filter(n => n[1] === low).map(n => n[0]), pads.filter(n => n[1] === high).map(n => n[0])];
    while (pointer() > 0) turn(6, -1);
    c.sim_control(0, B.SCL, 1);
    turn(6, 4);                                     // the sweep, 1:1:000 to 1:2:000
    c.sim_control(0, B.SCL, 0);
    o.swept = count();
    const h0 = undos();
    shift(1);
    o.clicks = [];
    for (let k = 0; k < 3; k++) { turn(2, 1); render(2); o.clicks.push([count(), moved()]); }
    turn(3, 1); render(16);
    o.his = { notes: notes(7), pointer: pointer(), anchor: info(58, 0, 4) - p0, count: count(), knobs: r.knobs().slice(0, 2) };
    o.nudgePng = r.png();
    shift(0);
    o.hisSteps = undos() - h0;
    held(B.HOME, () => tap(B.SAVE));
    render(4);
    o.hisUndone = { notes: notes(7), page: r.page(), moved: moved() };

    // 2. Track 1 at the bar's start: quarters, C4 and E4 together, G4, E4, then three rests.
    turn(0, -7);
    while (pointer() > 0) turn(6, -1);
    held(B.FX, () => key(4));                       // SHIFT+A: 1/4
    press(7); press(11); render(2); lift(7); lift(11); render(2);
    key(14); key(11);
    for (let i = 0; i < 3; i++) rest();             // into bar 14
    o.entered = notes(0);
    o.end = info(11) - p0;
    turn(6, -6);                                    // back to the chord
    o.back = pointer();
    held(B.SCL, () => key(11));                     // SEL+E4: E4 alone
    o.pickedFirst = count();
    const u0 = undos();
    shift(1);
    for (let i = 0; i < 3; i++) { turn(3, 1); render(2); }
    render(16);
    o.nudged = { notes: notes(0), pointer: pointer(), picks: picks(), count: count(), knobs: r.knobs() };
    turn(2, 1); render(16);                         // past the G4
    o.moved = { notes: notes(0), pointer: pointer(), count: count(), knob: r.knobs()[1] };
    turn(3, -3); render(16);                        // back onto the line
    o.onLine = { pointer: pointer(), count: count(), knob: r.knobs()[1] };
    turn(2, 1); render(2);                          // onto the other E4: replaced
    o.landed = { notes: notes(0), count: count(), covered: covered() };
    turn(2, 1); render(2);                          // on: it comes back
    o.passed = { notes: notes(0), covered: covered() };
    turn(2, -1); render(2);                         // back onto it, where the run stops
    o.stopped = { notes: notes(0), pointer: pointer(), count: count() };
    shift(0);
    render(16);
    o.unshifted = r.knobs().slice(0, 2);
    o.steps = undos() - u0;
    held(B.HOME, () => tap(B.SAVE));
    render(4);
    o.undone = { notes: notes(0), page: r.page(), moved: moved() };
    held(B.FX, () => held(B.HOME, () => tap(B.SAVE)));
    render(4);
    o.redone = { notes: notes(0), page: r.page() };

    // 3. Playing from the bar: NUDGE two detents, early in it, and E4 plays at 970.
    c.sim_song_capture(1);
    tap(B.PLAY);
    render(10);
    o.playing = info(7);
    held(B.FX, () => { turn(3, 2); render(2); });
    o.playedMove = { notes: notes(0).filter(n => n[1] === E4), pointer: pointer() };
    render(700);
    tap(B.PLAY);
    render(8);
    const tm = r.tempo(), heard = r.heard().filter(h => !h.kind && h.track === 0);
    const e4 = heard.filter(h => h.velocity && h.pitch === E4);
    o.played = { blocks: e4.map(h => h.block), want: tm.block(info(9), info(10), p0 + 970), balanced: r.balanced(heard) };
    held(B.HOME, () => tap(B.SAVE));                // the playing nudge undone
    o.afterPlayUndo = notes(0).filter(n => n[1] === E4);

    // 4. The empty bar 14: its bar line at 1/4, a 1/64 walk, then 1/2.
    turn(6, 2);                                     // from 970, two quarters on: bar 14's line
    o.barLine = pointer();
    held(B.FX, () => key(11));                      // SHIFT+E: 1/64
    turn(6, 3);
    o.walk = pointer();
    shift(1);
    key(2);                                         // SHIFT+G: 1/2
    render(16);
    o.half = { pointer: pointer(), knobs: r.knobs().slice(0, 2) };
    shift(0);
    turn(6, 1); o.next = pointer();
    turn(6, -1); o.line = pointer();
    turn(6, -1); o.before = pointer();
    r.step();
    return o;
  });
  savePng(root, 'step-nudge', out, 'nudgePng');
  await page.close();
  const t = checker('nudge'), q = out;
  t.eq('SHIFT+REC on track 8: step entry', q.stepPage, 20);
  t.eq('his layout: steps 1, 3, 5, 7, 9, 11 to 13', q.pads, [0, 240, 480, 720, 960, 1200, 1320, 1440]);
  t.eq('the lower pad on 1, 3, 7, 9 and the higher on 5, 11 to 13', q.layout, [true, [0, 240, 720, 960], [480, 1200, 1320, 1440]]);
  t.eq('the sweep holds three', q.swept, 3);
  t.eq('three clicks of SHIFT+K1, none refused, the same three selected', q.clicks, [[3, 3], [3, 3], [3, 3]]);
  t.eq('they moved three 16ths and a NUDGE, past the lower pad on step 7', q.his.notes.map(n => n[0]),
    [365, 605, 720, 845, 960, 1200, 1320, 1440]);
  t.eq('the band with them', [q.his.anchor, q.his.pointer], [365, 845]);
  t.eq('still the three', q.his.count, 3);
  t.eq('the footer: MOVE 1/16, NUDGE +5', q.his.knobs, ['MOVE 1/16', 'NUDGE +5']);
  t.eq('his run is one undo step', q.hisSteps, 1);
  t.eq('HOME+SAVE: his notes back', q.hisUndone.notes.map(n => n[0]), [0, 240, 480, 720, 960, 1200, 1320, 1440]);
  t.eq('and the moved selection gone with it', [q.hisUndone.page, q.hisUndone.moved], [20, 0]);
  t.eq('the notes: C4 and E4, G4 a quarter on, E4 a half on', q.entered, [[0, 60], [0, 64], [480, 67], [960, 64]]);
  t.eq('the rests took the song a bar on', q.end, 3840);
  t.eq('PRESETS back to the chord', q.back, 0);
  t.eq('SEL+E4 picks it alone', q.pickedFirst, 1);
  t.eq('SHIFT+K2 three detents: E4 15 clocks on, the rest stay', q.nudged.notes, [[0, 60], [15, 64], [480, 67], [960, 64]]);
  t.eq('the pointer with it', q.nudged.pointer, 15);
  t.eq('the pick is the user\'s, E4', q.nudged.picks, [0, 0, 1, 0]);
  t.eq('the selection is E4', q.nudged.count, 1);
  t.eq('the footer: MOVE by the length, NUDGE +15 from the grid', q.nudged.knobs.slice(0, 2), ['MOVE 1/4', 'NUDGE +15']);
  t.eq('SHIFT+K1: a quarter on, past the G4', q.moved.notes, [[0, 60], [480, 67], [495, 64], [960, 64]]);
  t.eq('the pointer with it', q.moved.pointer, 495);
  t.eq('the G4 doesn\'t join', q.moved.count, 1);
  t.eq('still NUDGE +15', q.moved.knob, 'NUDGE +15');
  t.eq('SHIFT+K2 back three: on the line by the G4', q.onLine.pointer, 480);
  t.eq('still E4 alone there', q.onLine.count, 1);
  t.eq('NUDGE 0 on the line', q.onLine.knob, 'NUDGE 0');
  t.eq('onto the other E4: it is replaced', q.landed.notes, [[0, 60], [480, 67], [960, 64]]);
  t.eq('one E4 selected, one waiting', [q.landed.count, q.landed.covered], [1, 1]);
  t.eq('a quarter on: the other E4 back', q.passed.notes, [[0, 60], [480, 67], [960, 64], [1440, 64]]);
  t.eq('none waiting', q.passed.covered, 0);
  t.eq('back onto it, where the run stops: replaced', q.stopped.notes, [[0, 60], [480, 67], [960, 64]]);
  t.eq('the pointer there, E4 alone', [q.stopped.pointer, q.stopped.count], [960, 1]);
  t.eq('SHIFT up: LENGTH and VELOCITY again', q.unshifted.map(k => k.split(' ')[0]), ['LENGTH', 'VELOCITY']);
  t.eq('the whole run is one undo step', q.steps, 1);
  t.eq('HOME+SAVE: E4 back in the chord, and the other E4 back', q.undone.notes, [[0, 60], [0, 64], [480, 67], [960, 64]]);
  t.eq('in step entry still, the moved selection gone', [q.undone.page, q.undone.moved], [20, 0]);
  t.eq('SHIFT+HOME+SAVE: where the run left it, the other E4 replaced', q.redone.notes, [[0, 60], [480, 67], [960, 64]]);
  t.eq('playing', q.playing, 1);
  t.eq('NUDGE while playing moves it', q.playedMove.notes, [[970, 64]]);
  t.eq('and the pointer', q.playedMove.pointer, 970);
  t.eq('E4 played once', q.played.blocks.length, 1);
  t.eq('at 970 exactly', q.played.blocks[0], q.played.want);
  t.yes('every note ended', q.played.balanced);
  t.eq('the playing nudge is a step of its own', q.afterPlayUndo, [[960, 64]]);
  t.eq('PRESETS at 1/4 to bar 14', q.barLine, 1920);
  t.eq('a 1/64 walk: three 64ths', q.walk, 1920 + 90);
  t.eq('1/2 leaves the pointer', q.half.pointer, 1920 + 90);
  t.eq('the footer with SHIFT: MOVE 1/2, NUDGE +90', q.half.knobs, ['MOVE 1/2', 'NUDGE +90']);
  t.eq('PRESETS on: the half bar', q.next, 1920 + 960);
  t.eq('back: the bar line', q.line, 1920);
  t.eq('back again: the half bar before', q.before, 960);
  return { phase: 'nudge', checks: t.list, out };
}
