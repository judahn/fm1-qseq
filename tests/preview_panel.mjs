// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
// The panel in the preview check. Every button light is on, all the time,
// on stock and song pages alike, so the printed labels can be read. The
// wake (delivery method B) runs while the song plays and stops with it.
// And a page change while the song view's playhead moves every frame (2-bar
// zoom) paints the whole new page with the LCD at the device's pace, one
// band per control poll: the footer comes out as it does at full speed.
import { simPage, checker } from './preview_lib.mjs';

export async function phasePanel(browser, root) {
  const page = await simPage(browser, root);
  const out = await page.evaluate(() => {
    const r = previewRig(), { B } = r, o = { lit: {}, pages: {} };
    const look = name => { r.render(8); o.lit[name] = r.lights().every(v => v === 1); o.pages[name] = r.page(); };
    r.render(400);
    look('overview');
    r.tap(B.EDIT); look('sound editing'); r.tap(B.EDIT); r.render(8);
    r.held(B.HOME, () => r.tap(B.GLO)); look('device settings'); r.tap(B.HOME); r.render(8);
    r.tap(B.SEQ); look('song view');
    r.tap(B.PLAY); r.render(40); look('song view, playing');
    o.wakePlaying = r.info(51);
    r.tap(B.SEQ); look('clip grid');
    r.tap(B.SEQ); r.render(8);
    r.tap(B.GLO); look('song settings'); r.tap(B.GLO); r.render(8);
    r.tap(B.PLAY); r.render(8);
    o.wakeStopped = r.info(51);
    r.tap(B.REC); look('recording chooser'); r.tap(B.HOME); r.render(8);
    r.held(B.FX, () => r.tap(B.SAVE)); look('song list'); r.tap(B.HOME); r.render(8);
    // The same steps at full speed and at the device's pace.
    const run = paced => {
      const q = previewRig();
      q.render(400);
      q.c.sim_lcd_paced(paced ? 1 : 0);
      q.tap(B.SEQ); q.render(20);
      for (let i = 0; i < 3; i++) q.turn(5, 1);      // K4: 2 bars across, the playhead moves every frame
      q.tap(B.PLAY); q.render(200);
      q.tap(B.SEQ); q.render(200);                   // the grid
      q.tap(B.SEQ); q.render(150);                   // the song view again, still playing
      return { lcd: q.pixels(), playing: q.info(7) };
    };
    const fast = run(false), slow = run(true);
    o.playingThrough = fast.playing && slow.playing;
    o.footer = r.differ(slow.lcd, 60, 195, 240, 240, fast.lcd); // K2 to K4 and the hint, which hold still
    return o;
  });
  await page.close();
  const t = checker('panel'), q = out;
  for (const [name, lit] of Object.entries(q.lit)) t.yes(`every button lit: ${name} (page ${q.pages[name]})`, lit);
  t.eq('the song pages were reached', [q.pages['song view'], q.pages['clip grid'], q.pages['song settings'],
    q.pages['recording chooser'], q.pages['song list']], [19, 27, 21, 23, 25]);
  t.eq('the wake runs while the song plays', q.wakePlaying, 1);
  t.eq('and stops with it', q.wakeStopped, 0);
  t.yes('the song played through the page changes', q.playingThrough);
  t.eq('device-paced LCD: the song view\'s footer painted after the page change, as at full speed', q.footer, 0);
  return { phase: 'panel', checks: t.list, out };
}
