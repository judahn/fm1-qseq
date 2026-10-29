// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
// The panel in the preview check. Every button light is on, all the time,
// on stock and song pages alike, so the printed labels can be read. The
// wake (delivery method B) runs while the song plays and stops with it.
// And a page change while the song view's playhead moves every frame (2-bar
// zoom) paints the whole new page with the LCD at the device's pace, one
// band per control poll: the footer comes out as it does at full speed.
// This fork's release (the probe, and CHANGELOG.md's latest) is on the boot
// screen between the bars and M-VAVE, the same line heads the song list, and
// the page's version badge carries it. Under it on the list, the battery:
// the simulator's 900 as BAT 900, and its bar, and right of it the free
// heap, a dash in the browser, which has no heap (0.4.0). A held REC opens
// the record settings, and HOME there goes back. The mix page has a tile a
// track, with its MUTE and SOLO rows. Saves docs/panel-boot.png,
// docs/panel-song-list.png and docs/panel-mix.png.
import fs from 'node:fs';
import path from 'node:path';
import { simPage, savePng, checker } from './preview_lib.mjs';

export async function phasePanel(browser, root) {
  const page = await simPage(browser, root);
  const out = await page.evaluate(() => {
    const r = previewRig(), { B } = r, o = { lit: {}, pages: {} };
    const look = name => { r.render(8); o.lit[name] = r.lights().every(v => v === 1); o.pages[name] = r.page(); };
    const word = i => { const w = r.info(53, 0, i); return [w & 255, w >>> 8 & 255, w >>> 16 & 255, w >>> 24]; };
    const bytes = [0, 1, 2, 3].flatMap(word), line = 'QY ' + String.fromCharCode(...bytes.slice(0, bytes.indexOf(0)));
    o.version = line.slice(3);
    const drawn = (lcd, y0, y1) => { /* the box around what is drawn in these rows */
      const box = { left: 240, right: -1, top: 240, bottom: -1 };
      for (let y = y0; y < y1; y++)
        for (let x = 0; x < 240; x++)
          if (lcd[y * 240 + x]) {
            box.left = Math.min(box.left, x); box.right = Math.max(box.right, x);
            box.top = Math.min(box.top, y); box.bottom = y;
          }
      return box;
    };
    const mask = (lcd, x, y) => Array.from({ length: 7 }, (_, row) =>
      Array.from({ length: 6 * line.length }, (_, col) => (lcd[(y + row) * 240 + x + col] ? 1 : 0)).join('')).join('/');
    const b = previewRig();
    b.c.sim_boot(2);                                 // the boot screen, as the device starts
    b.render(32);
    const boot = b.pixels(), left = (240 - 6 * line.length) >> 1;
    const shown = drawn(boot, 188, 207);             // the rows between the bars and M-VAVE
    const last = left + 6 * (line.length - 1);       // the last letter's cell, which a narrow one (a 1) doesn't fill
    o.bootBox = { left: shown.left, endsInLastCell: shown.right >= last && shown.right <= last + 4, top: shown.top, bottom: shown.bottom };
    o.bootWant = { left, endsInLastCell: true, top: 194, bottom: 200 };
    o.bootPng = b.png();
    r.render(400);
    const mix = r.state();                           // with no project to restore, nothing starts muted or soloed (0.3.2)
    o.bootMix = [4, 5].map(k => Array.from({ length: 8 }, (_, t) => mix[8 + t * 20 + k]).join(''));
    look('overview');
    r.tap(B.EDIT); look('sound editing'); r.tap(B.EDIT); r.render(8);
    r.held(B.HOME, () => r.tap(B.GLO)); look('device settings'); r.tap(B.HOME); r.render(8);
    r.tap(B.GLO); r.render(8);                       // the mix page, one tile a track (0.3.3)
    r.key(2); r.key(18); look('mix');                // white keys 2 and 11: mute track 2, solo track 3
    const tiles = r.pixels();
    o.mixTiles = Array.from({ length: 8 }, (_, t) => {
      const x = 8 + (t % 4) * 57 + 48, y = 72 + (t >> 2) * 56, panel = tiles[(y + 3) * 240 + x];
      return [18, 34].map(dy => (tiles[(y + dy) * 240 + x] !== panel ? 1 : 0)).join('');
    });
    o.mixPng = r.png();
    r.key(2); r.key(18); r.tap(B.GLO); r.render(8);
    r.tap(B.SEQ); look('song view');
    r.tap(B.PLAY); r.render(40); look('song view, playing');
    o.wakePlaying = r.info(51);
    r.tap(B.SEQ); look('clip grid');
    r.tap(B.SEQ); r.render(8);
    r.tap(B.GLO); look('song settings'); r.tap(B.GLO); r.render(8);
    r.tap(B.PLAY); r.render(8);
    o.wakeStopped = r.info(51);
    r.long(B.REC); look('record settings'); r.tap(B.HOME); r.render(8);  // REC held (0.4.0)
    o.recBack = r.page();
    r.held(B.FX, () => r.tap(B.SAVE)); look('song list');
    const list = r.pixels();
    /* Right of the battery, the free heap: the browser has none, so a dash. */
    o.heapBox = drawn(Array.from(list, (v, i) => (i % 240 >= 150 && (i / 240 | 0) >= 13 && (i / 240 | 0) < 25 ? v : 0)), 13, 25);
    o.heapDash = [15, 16, 17, 18, 19, 20, 21].map(y => [226, 227, 228, 229, 230, 231].map(x => (list[y * 240 + x] ? 1 : 0)).join('')).join('/');
    o.sameLine = mask(list, 76, 5) === mask(boot, left, 194);
    o.battery = r.info(55);
    const box = { left: 240, right: -1, top: 240, bottom: -1 };   /* what is drawn left of the bar, under the release */
    for (let y = 13; y < 25; y++)
      for (let x = 70; x < 122; x++)
        if (list[y * 240 + x]) { box.left = Math.min(box.left, x); box.right = Math.max(box.right, x); box.top = Math.min(box.top, y); box.bottom = Math.max(box.bottom, y); }
    o.batteryBox = box;
    o.batteryBar = [124, 125, 135, 146, 147].map(x => list[18 * 240 + x] !== 0);
    o.listPng = r.png();
    r.tap(B.HOME); r.render(8);
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
  out.badge = await page.textContent('#release-version');
  savePng(root, 'panel-boot', out, 'bootPng');
  savePng(root, 'panel-song-list', out, 'listPng');
  savePng(root, 'panel-mix', out, 'mixPng');
  await page.close();
  const latest = (fs.readFileSync(path.join(root, 'CHANGELOG.md'), 'utf8').match(/^## (\S+)/m) || [])[1];
  const t = checker('panel'), q = out;
  for (const [name, lit] of Object.entries(q.lit)) t.yes(`every button lit: ${name} (page ${q.pages[name]})`, lit);
  t.eq('at boot no track is muted or soloed (the engine\'s default mutes 6 and 7)', q.bootMix, ['00000000', '00000000']);
  t.eq('the mix page: a tile a track, its MUTE row lit on track 2 and its SOLO row on track 3', q.mixTiles,
    ['00', '10', '01', '00', '00', '00', '00', '00']);
  t.eq('the song pages were reached', [q.pages['song view'], q.pages['clip grid'], q.pages['song settings'],
    q.pages['record settings'], q.pages['song list']], [19, 27, 21, 23, 25]);
  t.eq('HOME on the record settings goes back to the song view', q.recBack, 19);
  t.eq('the song list: HEAP and a dash right of the battery, 6 letters to the right edge, since the browser has no heap',
    q.heapBox, { left: 232 - 6 * 6, right: 232 - 2, top: 15, bottom: 21 });
  t.eq('the dash', q.heapDash, '000000/000000/000000/111110/000000/000000/000000');
  t.eq('the wake runs while the song plays', q.wakePlaying, 1);
  t.eq('and stops with it', q.wakeStopped, 0);
  t.yes('the song played through the page changes', q.playingThrough);
  t.eq('device-paced LCD: the song view\'s footer painted after the page change, as at full speed', q.footer, 0);
  t.eq('the firmware\'s release is CHANGELOG.md\'s latest', q.version, latest);
  t.eq('the boot screen shows QY and the release, centered between the bars and M-VAVE', q.bootBox, q.bootWant);
  t.yes('the song list\'s header shows the same line', q.sameLine);
  t.eq('the battery: the simulator\'s reading, smoothed', q.battery, 900);
  t.eq('BAT 900 under the release, 7 letters wide', q.batteryBox, { left: 76, right: 76 + 6 * 7 - 2, top: 15, bottom: 21 });
  t.eq('its bar: the frame and all 22 lit, 900 being past full (622)', q.batteryBar, [true, true, true, true, true]);
  t.eq('the page\'s version badge: FM1 Open\'s release, then this one', q.badge, 'V14 · QY ' + latest);
  return { phase: 'panel', checks: t.list, out };
}
