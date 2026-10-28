// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
// The hardware diagnostics page in the preview check: SHIFT+GLO, then ALGO
// past the sync and MIDI pages, opens it. A run of each delivery method.
// The browser's control loop runs every 10 ms, so method A should show late
// notes and B and C none. Saves docs/hw-diag.png.
import { simPage, savePng, checker } from './preview_lib.mjs';

export async function phaseDiag(browser, root) {
  const page = await simPage(browser, root);
  const out = await page.evaluate(() => {
    const r = previewRig(), { B } = r, o = {};
    const method = () => r.info(22);
    const toDiag = () => { r.held(B.FX, () => r.tap(B.GLO)); for (let i = 0; i < 5; i++) r.turn(1, 1); };
    r.render(32);
    r.tap(B.SEQ);
    toDiag();
    o.page = r.page();
    o.defaultMethod = method();
    const rewind = () => {                         // HOME, SHIFT+PLAY to the top, and back
      r.tap(B.HOME);
      r.held(B.FX, () => r.tap(B.PLAY));
      toDiag();
    };
    const run = m => {
      rewind();
      while (method() > m) r.turn(2, -1);
      if (method() < m) r.held(B.FX, () => { while (method() < m) r.turn(2, 1); });
      r.tap(B.PLAY); r.render(2500); r.tap(B.PLAY);
      return { method: 'ABC'[method()], late: [0, 1, 2, 3].map(i => r.info(23, 0, i)), notes: r.info(23, 0, 4), polls: r.info(24) };
    };
    o.runs = [run(0), run(1), run(2)];
    o.cNeedsShift = (() => { r.turn(2, -1); r.turn(2, 1); return method(); })(); // B, then K1 up without SHIFT stays at B
    r.tap(B.ENV);                                  // the factory memory sweep
    o.heaviest = r.info(26) | 0;
    o.largestFree = r.info(25);
    run(0); rewind(); r.tap(B.PLAY); r.render(900); // left playing on A for the picture
    o.playingInPicture = r.info(7);
    o.png = r.png();
    return o;
  });
  savePng(root, 'hw-diag', out, 'png');
  await page.close();
  const t = checker('diagnostics'), q = out, [a, b, c] = q.runs;
  t.eq('SHIFT+GLO, ALGO five times opens the diagnostics page', q.page, 22);
  t.eq('the preview delivers by method C', q.defaultMethod, 2);
  t.over('method A: notes played', a.notes, 100);
  t.over('method A: some notes late (10 ms control loop)', a.late[1] + a.late[2] + a.late[3], 0);
  t.over('method B: notes played', b.notes, 100);
  t.eq('method B: every note on time', b.late[0], b.notes);
  t.over('method C: notes played', c.notes, 100);
  t.eq('method C: every note on time', c.late[0], c.notes);
  t.eq('K1 reaches C only with SHIFT', q.cNeedsShift, 1);
  t.atLeast('the memory sweep found the heaviest engine', q.heaviest, 0);
  t.eq('playing in the picture', q.playingInPicture, 1);
  return { phase: 'diagnostics', checks: t.list, out };
}
