// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
// The nudge phase of the preview check (0.6.7): step entry's MOVE and NUDGE,
// and PRESETS on the step grid. Past the test song's end, on track 1, a C4
// and E4 chord, a G4 a quarter on and another E4 a half on, then rests into
// an empty bar. Back at the chord, SEL picks E4. SHIFT+K2, three detents,
// nudges it 15 clocks and the pointer with it, the footer saying MOVE 1/4
// and NUDGE +15, the pick E4 alone. SHIFT+K1 moves it a quarter on, past
// the G4, which doesn't join it, and SHIFT+K2 back onto the grid line, NUDGE
// 0. Another quarter would land on the other E4: NOTE IN THE WAY, nothing
// moves. All of it is one undo step: HOME+SAVE puts E4 back in the chord and
// SHIFT+HOME+SAVE moves it again. Playing, NUDGE still moves it, and it
// plays where it went. Then in the empty bar, a walk at 1/64 and 1/2: the
// pointer stays, NUDGE +90, PRESETS lands on the half bar, then the bar
// line, then the half bar before. Saves docs/step-nudge.png.
import { simPage, savePng, checker } from './preview_lib.mjs';

export async function phaseNudge(browser, root) {
  const page = await simPage(browser, root);
  const out = await page.evaluate(() => {
    const r = previewRig(), { B, c } = r, o = {};
    const { tap, key, held, turn, render, info } = r;
    const DIM = 0x738e, E4 = 64;
    const press = k => c.sim_control(1, k, 1), lift = k => c.sim_control(1, k, 0);
    const shift = down => c.sim_control(0, B.FX, down ? 1 : 0);
    render(32);
    tap(B.SEQ);
    turn(6, 12);                                    // the playhead to bar 13, past the song's end
    const p0 = info(8);
    r.step();
    o.stepPage = r.page();
    const notes = () => Array.from({ length: info(0, 0) }, (_, i) => i).filter(i => !info(30, 0, i))
      .map(i => [info(1, 0, i) - p0, info(2, 0, i)]).filter(n => n[0] >= 0).sort((a, b) => a[0] - b[0] || a[1] - b[1]);
    const pointer = () => info(17) - p0, count = () => info(58, 0, 5), undos = () => info(29, 0, 9);
    const picks = () => [0, 1, 2, 3].map(i => info(64, 0, i));
    const hintEnd = () => { /* the bottom line's rightmost pixel */
      render(16);
      const lcd = r.pixels(); let x = -1;
      for (let y = 231; y < 238; y++) for (let i = 0; i < 240; i++) if (lcd[y * 240 + i] === DIM) x = Math.max(x, i);
      return x;
    };

    // The notes: quarters, C4 and E4 together, G4, E4, then three rests.
    held(B.FX, () => key(4));                       // SHIFT+A: 1/4
    press(7); press(11); render(2); lift(7); lift(11); render(2);
    key(14); key(11);
    for (let i = 0; i < 3; i++) held(B.FX, () => key(8)); // SHIFT+C#: rests, into bar 14
    o.entered = notes();
    o.end = info(11) - p0;
    turn(6, -6);                                    // back to the chord
    o.back = pointer();
    held(B.SCL, () => key(11));                     // SEL+E4: E4 alone
    o.pickedFirst = count();
    const u0 = undos();

    // 1. SHIFT+K2 three detents, a poll each.
    shift(1);
    for (let i = 0; i < 3; i++) { turn(3, 1); render(2); }
    render(16);
    o.nudged = { notes: notes(), pointer: pointer(), picks: picks(), count: count(), knobs: r.knobs() };
    o.nudgePng = r.png();
    // 2. SHIFT+K1: a quarter on, past the G4.
    turn(2, 1); render(16);
    o.moved = { notes: notes(), pointer: pointer(), count: count(), knob: r.knobs()[1] };
    // 3. SHIFT+K2 back three: on the grid line, by the G4, still alone.
    turn(3, -3); render(16);
    o.onLine = { pointer: pointer(), count: count(), knob: r.knobs()[1] };
    // 4. A quarter more lands on the other E4: refused.
    turn(2, 1);
    o.refused = { notes: notes(), pointer: pointer(), hint: hintEnd() };
    shift(0);
    render(16);
    o.unshifted = r.knobs().slice(0, 2);
    o.steps = undos() - u0;

    // 5. One undo step, and redone.
    held(B.HOME, () => tap(B.SAVE));
    render(4);
    o.undone = { notes: notes(), page: r.page() };
    held(B.FX, () => held(B.HOME, () => tap(B.SAVE)));
    render(4);
    o.redone = { notes: notes(), page: r.page() };

    // 6. Playing from the bar: NUDGE two detents, early in it, and E4 plays at 490.
    c.sim_song_capture(1);
    tap(B.PLAY);
    render(10);
    o.playing = info(7);
    held(B.FX, () => { turn(3, 2); render(2); });
    o.playedMove = { notes: notes(), pointer: pointer() };
    render(400);
    tap(B.PLAY);
    render(8);
    const tm = r.tempo(), heard = r.heard().filter(h => !h.kind && h.track === 0);
    const e4 = heard.filter(h => h.velocity && h.pitch === E4);
    o.played = { blocks: e4.map(h => h.block), want: tm.block(info(9), info(10), p0 + 490), balanced: r.balanced(heard) };
    held(B.HOME, () => tap(B.SAVE));                // the playing nudge undone
    o.afterPlayUndo = notes().filter(n => n[1] === E4);

    // 7. The empty bar 14: its bar line at 1/4, a 1/64 walk, then 1/2.
    turn(6, 3);                                     // from 490, three quarters on: bar 14's line
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
  t.eq('SHIFT+REC: step entry', q.stepPage, 20);
  t.eq('the notes: C4 and E4, G4 a quarter on, E4 a half on', q.entered, [[0, 60], [0, 64], [480, 67], [960, 64]]);
  t.eq('the rests took the song a bar on', q.end, 3840);
  t.eq('PRESETS back to the chord', q.back, 0);
  t.eq('SEL+E4 picks it alone', q.pickedFirst, 1);
  t.eq('SHIFT+K2 three detents: E4 15 clocks on, the rest stay', q.nudged.notes, [[0, 60], [15, 64], [480, 67], [960, 64]]);
  t.eq('the pointer with it', q.nudged.pointer, 15);
  t.eq('the pick is E4 alone', q.nudged.picks, [0, 0, 1, 0]);
  t.eq('the selection is E4', q.nudged.count, 1);
  t.eq('the footer: MOVE by the length, NUDGE +15 from the grid', q.nudged.knobs.slice(0, 2), ['MOVE 1/4', 'NUDGE +15']);
  t.eq('SHIFT+K1: a quarter on, past the G4', q.moved.notes, [[0, 60], [480, 67], [495, 64], [960, 64]]);
  t.eq('the pointer with it', q.moved.pointer, 495);
  t.eq('the G4 doesn\'t join', q.moved.count, 1);
  t.eq('still NUDGE +15', q.moved.knob, 'NUDGE +15');
  t.eq('SHIFT+K2 back three: on the line by the G4', q.onLine.pointer, 480);
  t.eq('still E4 alone there', q.onLine.count, 1);
  t.eq('NUDGE 0 on the line', q.onLine.knob, 'NUDGE 0');
  t.eq('onto the other E4: nothing moves', q.refused.notes, [[0, 60], [480, 64], [480, 67], [960, 64]]);
  t.eq('the pointer stays', q.refused.pointer, 480);
  t.eq('NOTE IN THE WAY on the bottom line', q.refused.hint, 6 + 14 * 6 + 4);
  t.eq('SHIFT up: LENGTH and VELOCITY again', q.unshifted.map(k => k.split(' ')[0]), ['LENGTH', 'VELOCITY']);
  t.eq('the whole run is one undo step', q.steps, 1);
  t.eq('HOME+SAVE: E4 back in the chord', q.undone.notes, [[0, 60], [0, 64], [480, 67], [960, 64]]);
  t.eq('in step entry still', q.undone.page, 20);
  t.eq('SHIFT+HOME+SAVE: where the run left it', q.redone.notes, [[0, 60], [480, 64], [480, 67], [960, 64]]);
  t.eq('playing', q.playing, 1);
  t.eq('NUDGE while playing moves it', q.playedMove.notes.filter(n => n[1] === 64), [[490, 64], [960, 64]]);
  t.eq('and the pointer', q.playedMove.pointer, 490);
  t.eq('E4 played at 490, then the other', q.played.blocks.length, 2);
  t.eq('at 490 exactly', q.played.blocks[0], q.played.want);
  t.yes('every note ended', q.played.balanced);
  t.eq('the playing nudge is a step of its own', q.afterPlayUndo, [[480, 64], [960, 64]]);
  t.eq('PRESETS at 1/4 to bar 14', q.barLine, 1920);
  t.eq('a 1/64 walk: three 64ths', q.walk, 1920 + 90);
  t.eq('1/2 leaves the pointer', q.half.pointer, 1920 + 90);
  t.eq('the footer with SHIFT: MOVE 1/2, NUDGE +90', q.half.knobs, ['MOVE 1/2', 'NUDGE +90']);
  t.eq('PRESETS on: the half bar', q.next, 1920 + 960);
  t.eq('back: the bar line', q.line, 1920);
  t.eq('back again: the half bar before', q.before, 960);
  return { phase: 'nudge', checks: t.list, out };
}
