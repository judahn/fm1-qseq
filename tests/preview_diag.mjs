// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
// The hardware test build's diagnostics page, in the preview built the same
// way (./build.sh --test-preview: SONG_UI 0 and SONG_HW_TEST 1, as the
// default device build). SEQ opens it from the stock pages and SEQ again
// goes to the stock step page. A whole run of the test song by each delivery
// method: the browser's control loop runs every 10 ms, so A shows late notes
// and B none. HOME goes back to the stock overview. With the LCD at the
// device's pace (one band per control poll) the page paints whole over the
// stock page it replaces, which it didn't on the FM-1. The memory probe
// stops at the top of its search, which the page reads as "or more". Every
// button light is on. Saves docs/hw-diag.png.
import { simPage, savePng, checker } from './preview_lib.mjs';

export async function phaseDiag(browser, root) {
  const page = await simPage(browser, root, 'build/preview-test/index.html');
  const out = await page.evaluate(() => {
    const r = previewRig(), { B } = r, o = {};
    const method = () => r.info(22), all = list => list.every(v => v === 1);
    r.render(400);                                     // the boot animation
    o.bootPage = r.page();
    o.overviewLit = all(r.lights());
    r.tap(B.SEQ); r.render(8);
    o.page = r.page();
    o.defaultMethod = method();
    o.diagLit = all(r.lights());
    // A whole run of the test song (12 bars, it stops by itself) by method m.
    const run = m => {
      r.turn(2, m ? 1 : -1);
      r.tap(B.SCL); r.tap(B.PLAY); r.render(4);
      const wake = r.info(51), lit = all(r.lights());
      let blocks = 4;
      while (r.info(7) && blocks < 12000) { r.render(100); blocks += 100; }
      return { method: 'AB'[method()], late: [0, 1, 2, 3].map(i => r.info(23, 0, i)), notes: r.info(23, 0, 4),
        wake, stopped: !r.info(7), wakeAfter: r.info(51), lit };
    };
    o.runs = [run(0), run(1)];
    r.turn(2, 1);
    o.pastB = method();                                // K1 right at B stays on B
    r.tap(B.HOME); r.render(8);
    o.homePage = r.page();
    // The LCD at the device's pace, from the overview and from the step page.
    const regions = lcd => ({ header: r.differ(lcd, 180, 0, 240, 27), rows: r.differ(lcd, 170, 74, 240, 104),
      low: r.differ(lcd, 0, 168, 240, 190) });
    r.c.sim_lcd_paced(1);
    r.render(200);
    r.tap(B.SEQ); r.render(150);
    o.fromOverview = regions(r.pixels());
    r.tap(B.SEQ); r.render(200);
    o.stepPage = r.page();
    o.stepLit = all(r.lights());
    r.tap(B.SEQ); r.render(150);
    o.fromSteps = regions(r.pixels());
    r.c.sim_lcd_paced(0);
    r.tap(B.ENV); r.render(8);                         // the memory probe, every engine on track 6
    o.largestFree = r.info(25);
    o.heaviest = r.info(26) | 0;
    o.engines = [0, 1, 2, 3, 4, 5].map(i => r.info(27, 0, i));
    r.tap(B.PLAY); r.render(900);                      // playing on B for the picture
    o.playingInPicture = r.info(7);
    o.png = r.png();
    return o;
  });
  savePng(root, 'hw-diag', out, 'png');
  await page.close();
  const t = checker('diagnostics'), q = out, [a, b] = q.runs, top = 1 << 19;
  t.eq('the test build boots on the stock overview', q.bootPage, 0);
  t.eq('SEQ opens the diagnostics page', q.page, 22);
  t.eq('the test build starts on method B', q.defaultMethod, 1);
  t.eq('method A: the whole song, 1,362 notes on and off', a.notes, 1362);
  t.over('method A: some notes late (10 ms control loop)', a.late[1] + a.late[2] + a.late[3], 0);
  t.eq('method A: no wake', a.wake, 0);
  t.eq('method B: the whole song', b.notes, 1362);
  t.eq('method B: every note on time', b.late[0], b.notes);
  t.eq('method B: the interrupt wakes the control loop while playing', b.wake, 1);
  t.yes('method B: the song stopped by itself', b.stopped);
  t.eq('method B: no wake once stopped', b.wakeAfter, 0);
  t.eq('K1 right at B stays on B (C is gone)', q.pastB, 1);
  t.eq('HOME goes back to the stock overview', q.homePage, 0);
  t.eq('SEQ on the diagnostics page goes to the stock step page', q.stepPage, 1);
  for (const [name, at] of [['from the overview', q.fromOverview], ['from the step page', q.fromSteps]]) {
    t.eq(`device-paced LCD, ${name}: nothing left in the header`, at.header, 0);
    t.eq(`device-paced LCD, ${name}: nothing left beside the rows`, at.rows, 0);
    t.eq(`device-paced LCD, ${name}: nothing left under the rows`, at.low, 0);
  }
  t.atLeast('memory probe: the browser\'s malloc reaches the top of the search', q.largestFree, top - 16);
  t.yes('memory probe: every engine reaches the top too ("or more")', q.engines.every(v => v >= top - 16));
  t.atLeast('memory probe: a heaviest engine is named', q.heaviest, 0);
  t.yes('every button lit on the overview', q.overviewLit);
  t.yes('every button lit on the diagnostics page', q.diagLit);
  t.yes('every button lit while playing', a.lit && b.lit);
  t.yes('every button lit on the stock step page', q.stepLit);
  t.eq('playing in the picture', q.playingInPicture, 1);
  return { phase: 'diagnostics', checks: t.list, out };
}
