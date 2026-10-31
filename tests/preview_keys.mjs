// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
// Step entry's keys in the preview check. With SHIFT a key works by its note
// name in any octave: F to E pick 1/1 down to 1/64, F# triplet, G# dots,
// C# rest, D# tie, A# repeat, and no key enters a note. K1 runs the same
// way, 1/1 at the left and halving turning right (0.5.1). Without SHIFT every
// key enters notes, F5 to G5 too, and the key lights show the scale on all
// 27. Saves docs/keys-step-shift.png.
import { simPage, savePng, checker } from './preview_lib.mjs';

export async function phaseKeys(browser, root) {
  const page = await simPage(browser, root);
  const out = await page.evaluate(() => {
    const r = previewRig(), { B, c, info } = r, o = {};
    const shiftKey = k => r.held(B.FX, () => r.key(k));
    const length = () => { r.render(2); return r.knobs()[0].replace('LENGTH ', ''); };
    const notes = () => Array.from({ length: info(0, 0) }, (_, i) => ({ clock: info(1, 0, i), pitch: info(2, 0, i), length: info(3, 0, i),
      kind: info(30, 0, i) })).filter(n => !n.kind);
    r.render(32);
    r.tap(B.SEQ);
    r.turn(6, 12);                                 // PRESETS: to the song end, bar 13, where track 1 is empty
    r.step();                                      // SHIFT+REC: step entry on track 1
    o.stepPage = r.page();

    // 0. K1 from 1/16: left to 1/1, then right a step at a time to 1/64.
    r.turn(2, -9);
    o.k1 = [length(), ...[1, 2, 3, 4, 5, 6].map(() => { r.turn(2, 1); return length(); })];
    // 1. Lengths by note name. K1 to 1/64 first, so each F shows.
    o.octaves = [0, 12, 24].map(k => { r.turn(2, 6); shiftKey(k); return length(); });
    o.whites = [12, 14, 16, 18, 19, 21, 23].map(k => { shiftKey(k); return length(); }); // F4 to E5
    shiftKey(7); o.c = length();                   // C4
    shiftKey(11); o.e = length();                  // E4
    shiftKey(19);                                  // C5: 1/16 again
    // 2. Triplet on F#4, twice. Dots on G#4, three times.
    o.triplet = [13, 13].map(k => { shiftKey(k); return length(); });
    o.dots = [15, 15, 15].map(k => { shiftKey(k); return length(); });

    // 3. F5, F#5 and G5 enter notes, a 16th apart.
    let before = notes().length, at = info(17);
    r.key(24); r.key(25); r.key(26);
    o.top = notes().slice(before).map(n => [n.clock - at, n.pitch]);
    // 4. SHIFT+C#4 rests. C4, then SHIFT+D#4 ties it on by a 16th.
    // SHIFT+A#4 enters it again.
    before = notes().length; at = info(17);
    shiftKey(8);
    o.rest = { added: notes().length - before, moved: info(17) - at };
    at = info(17);
    r.key(7);
    shiftKey(10);
    const tied = notes().find(n => n.clock === at && n.pitch === 60);
    o.tie = { length: tied ? tied.length : 0, moved: info(17) - at };
    before = notes().length; at = info(17);
    shiftKey(17);
    o.repeat = { added: notes().slice(before).map(n => [n.clock - at, n.pitch]), moved: info(17) - at };
    // The page with SHIFT held: the hint names the length keys.
    c.sim_control(0, B.FX, 1);
    r.render(4);
    o.shiftPng = r.png();
    c.sim_control(0, B.FX, 0);
    // 5. SHIFT with every key but the A# ones enters nothing.
    before = notes().length;
    r.held(B.FX, () => { for (let k = 0; k < 27; k++) if (k % 12 !== 5) r.key(k); });
    o.shiftNotes = notes().length - before;

    // 6. A minor on the settings page: in step entry all 27 key lights show it.
    r.step();                                      // SHIFT+REC leaves (0.5.0)
    r.tap(B.GLO); r.turn(1, 1); r.turn(2, 9); r.turn(3, 2); r.turn(1, -1); r.tap(B.GLO);
    r.step();
    r.render(4);
    const minor = [0, 2, 4, 5, 7, 9, 11], lit = Array.from(r.state().subarray(168 + 14, 168 + 41));
    o.scaleLeds = { lit: lit.map(v => (v ? 1 : 0)).join(''), want: lit.map((_, k) => (minor.includes((53 + k) % 12) ? 1 : 0)).join('') };
    return o;
  });
  savePng(root, 'keys-step-shift', out, 'shiftPng');
  await page.close();
  const t = checker('keys'), q = out;
  t.eq('SHIFT+REC opens step entry', q.stepPage, 20);
  t.eq('K1: 1/1 at the left, each step right halves it, down to 1/64', q.k1, ['1/1', '1/2', '1/4', '1/8', '1/16', '1/32', '1/64']);
  t.eq('SHIFT+F3, SHIFT+F4 and SHIFT+F5 each pick 1/1', q.octaves, ['1/1', '1/1', '1/1']);
  t.eq('SHIFT+F4 to E5: 1/1 down to 1/64', q.whites, ['1/1', '1/2', '1/4', '1/8', '1/16', '1/32', '1/64']);
  t.eq('SHIFT+C4 picks 1/16', q.c, '1/16');
  t.eq('SHIFT+E4 picks 1/64', q.e, '1/64');
  t.eq('SHIFT+F#4 turns triplet on and off', q.triplet, ['1/16T', '1/16']);
  t.eq('SHIFT+G#4 cycles dot, double dot, off', q.dots, ['1/16.', '1/16..', '1/16']);
  t.eq('F5, F#5 and G5 enter their notes, a 16th apart', q.top, [[0, 77], [120, 78], [240, 79]]);
  t.eq('SHIFT+C#4 rests: no note, one 16th on', q.rest, { added: 0, moved: 120 });
  t.eq('SHIFT+D#4 ties C4 on by a 16th (two steps, the second at 90%)', q.tie, { length: 228, moved: 240 });
  t.eq('SHIFT+A#4 repeats C4 at the pointer', q.repeat, { added: [[0, 60]], moved: 120 });
  t.eq('SHIFT with every other key enters no note', q.shiftNotes, 0);
  t.eq('in step entry the key lights show the scale on all 27 keys', q.scaleLeds.lit, q.scaleLeds.want);
  return { phase: 'keys', checks: t.list, out };
}
