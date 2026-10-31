// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
// The panel in the preview check. Every button light is on, all the time,
// on stock and song pages alike, so the printed labels can be read, but
// PLAY's blinks while the song plays, lit the first half of each beat, the
// count-in's too, and is lit again once stopped (0.5.1). The
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
// track, with its MUTE and SOLO rows. The knobs on the song pages (0.5.2):
// PRESETS moves the playhead and ALGO zooms, side by side, with a notice of
// the span and snap. K1 is the tempo the header shows, whole BPM or half
// with SHIFT, a turn from an imported 119.88 landing on that grid, heard at
// once while playing, K2 input quantize, requantizing the last take, also
// in the clip editor, and K3 and K4 the loop. The automation view has the
// lane on K1 and a change at the playhead on K2, stopped only, with SHIFT+
// ALGO only zooming. Step entry's PRESETS moves the pointer, jumps and
// sweeps, and K4 is the pitch knob. During a take PRESETS does nothing.
// Saves docs/panel-boot.png, docs/panel-song-list.png and docs/panel-mix.png.
import fs from 'node:fs';
import path from 'node:path';
import { simPage, savePng, checker } from './preview_lib.mjs';

export async function phasePanel(browser, root) {
  const page = await simPage(browser, root);
  const out = await page.evaluate(() => {
    const r = previewRig(), { B } = r, o = { lit: {}, pages: {} };
    const look = name => {
      r.render(8);
      const playing = r.info(7);
      o.lit[name] = r.lights().every((v, i) => v === 1 || (playing && i === B.PLAY));
      o.pages[name] = r.page();
    };
    /* PLAY's light block by block: its whole lit and dark runs, and whether
     * they are as long as each other, within a tenth (0.5.1). */
    const blink = n => {
      const runs = [[], []], mean = a => a.reduce((x, y) => x + y, 0) / (a.length || 1);
      let v = r.lights()[B.PLAY], length = 0, first = true;
      for (let i = 0; i < n; i++) {
        r.render(1);
        const w = r.lights()[B.PLAY];
        if (w === v) { length++; continue; }
        if (!first) runs[v].push(length);
        first = false; v = w; length = 1;
      }
      return { on: runs[1].length, off: runs[0].length, even: Math.abs(mean(runs[1]) - mean(runs[0])) <= 0.1 * mean(runs[0]) };
    };
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
    o.blinkPlaying = blink(1400);
    r.tap(B.SEQ); look('clip grid');
    r.tap(B.SEQ); r.render(8);
    r.tap(B.GLO); look('song settings'); r.tap(B.GLO); r.render(8);
    r.tap(B.PLAY); r.render(8);
    o.wakeStopped = r.info(51);
    o.stoppedLit = r.lights()[B.PLAY];
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
    // REC from a stop: through the count-in PLAY blinks, the song's clock
    // still. REC again calls the take off, and PLAY is lit.
    const clock = r.info(8);
    r.tap(B.REC);
    o.blinkCountIn = { ...blink(600), still: r.info(8) === clock, playing: r.info(7) };
    r.tap(B.REC); r.render(8);
    o.calledOff = { playing: r.info(7), lit: r.lights()[B.PLAY] };
    // The same steps at full speed and at the device's pace.
    const run = paced => {
      const q = previewRig();
      q.render(400);
      q.c.sim_lcd_paced(paced ? 1 : 0);
      q.tap(B.SEQ); q.render(20);
      for (let i = 0; i < 3; i++) q.turn(1, 1);      // ALGO: 2 bars across, the playhead moves every frame
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
  out.knobs = await page.evaluate(knobRun);
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
  t.yes('PLAY blinks while the song plays, lit and dark in turn, each half a beat', q.blinkPlaying.on >= 2 && q.blinkPlaying.off >= 2 && q.blinkPlaying.even, q.blinkPlaying);
  t.eq('stopped, PLAY is lit', q.stoppedLit, 1);
  t.yes('PLAY blinks through a take\'s count-in, the song\'s clock still', q.blinkCountIn.on >= 2 && q.blinkCountIn.off >= 2 && q.blinkCountIn.even &&
    q.blinkCountIn.still && q.blinkCountIn.playing === 1, q.blinkCountIn);
  t.eq('REC calls the count-in off: stopped, PLAY lit', q.calledOff, { playing: 0, lit: 1 });
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
  knobChecks(t, q.knobs);
  return { phase: 'panel', checks: t.list, out };
}

/* The knobs on the song pages (0.5.2), on a fresh simulator, in the page. */
function knobRun() {
  const n = previewRig(), { B, c, info } = n, o = {}, DIM = 0x738e, TEXT = 0xef5d;
  const knobs = (blocks = 16) => { n.render(blocks); return n.knobs(); }; /* a frame drawn first */
  const clock = () => info(8), zoom = () => info(58, 0, 6), tempo = (k = 0) => info(6, 0, k);
  const right = (y0, y1, x0, color) => { /* the rightmost pixel in `color` in those rows, from x0 */
    const lcd = n.pixels(); let x = -1;
    for (let y = y0; y < y1; y++) for (let i = x0; i < 240; i++) if (lcd[y * 240 + i] === color) x = Math.max(x, i);
    return x;
  };
  const hintEnd = () => { n.render(16); return right(231, 238, 0, DIM); };
  const headerEnd = () => { n.render(16); return right(8, 15, 196, TEXT); };
  /* SHIFT held past 600 ms first, so letting go alone is no tap to the effects page. */
  const shifted = fn => { c.sim_control(0, B.FX, 1); n.render(250); const v = fn(); c.sim_control(0, B.FX, 0); return v; };
  const notes = t => Array.from({ length: info(0, t) }, (_, i) => i).filter(i => !info(30, t, i)).map(i => [info(1, t, i), info(2, t, i)]);
  const added = (before, t) => notes(t).filter(m => !before.some(b => b[0] === m[0] && b[1] === m[1]));
  /* A take from a stop: the count-in, two keys off the 1/16 grid, REC ends it and PLAY stops. `during` runs in it. */
  const take = (during = () => {}) => {
    n.tap(B.REC);
    let k = 0; while (info(28) === 0xffffffff && k++ < 3000) n.render(1);
    n.render(37); c.sim_control(1, 26, 1); n.render(20); c.sim_control(1, 26, 0); n.render(53);
    during();
    c.sim_control(1, 24, 1); n.render(15); c.sim_control(1, 24, 0); n.render(10);
    n.tap(B.REC); n.tap(B.PLAY);
  };
  n.render(32);
  n.tap(B.SEQ);

  // 1. The song view's footer, PRESETS and ALGO, and K1 and K4 off the playhead and the zoom.
  const v = o.view = {};
  v.footer = knobs();
  n.turn(2, 1); v.k1 = [clock(), tempo()]; n.turn(2, -1);
  n.turn(6, 1); v.presets = clock();
  const end = info(15); n.turn(5, 1); v.k4 = [zoom(), info(15) - end]; n.turn(5, -1);
  v.hint = hintEnd();
  n.turn(1, 1); v.algo = [zoom(), hintEnd()];       /* ZOOM 8 BARS  SNAP 1/2 BAR: 25 letters */
  n.render(600); v.noticeGone = hintEnd();
  n.held(B.FX, () => n.turn(6, 1)); v.marker = clock();
  n.turn(1, -1); n.render(600);
  c.sim_control(0, B.HOME, 1); n.turn(6, 2); c.sim_control(0, B.HOME, 0); /* the stock HOME+PRESETS */
  v.home = [clock(), n.page()];
  n.tap(B.HOME); n.tap(B.SEQ); n.render(4);
  v.back = n.page();

  // 2. K1, the tempo: whole and half steps, the snap from an imported value, the display forms, the range.
  const tp = o.tempo = {};
  n.held(B.FX, () => n.tap(B.PLAY));               // the top, stopped
  n.turn(2, 1); tp.whole = [tempo(), knobs()[0]];
  tp.shiftShown = shifted(() => knobs()[0]);
  shifted(() => n.turn(2, 1)); tp.half = [tempo(), knobs()[0], headerEnd()];
  n.turn(2, 1); tp.halfUp = tempo();
  n.turn(2, -2); tp.back = tempo();
  const odd = turn => { c.sim_song_tempo(0, 11988); turn(); return tempo(); };
  c.sim_song_tempo(0, 11988); tp.odd = [knobs()[0], headerEnd()];
  tp.snaps = [odd(() => n.turn(2, 1)), odd(() => n.turn(2, -1)), odd(() => shifted(() => n.turn(2, 1))), odd(() => shifted(() => n.turn(2, -1)))];
  tp.snapShown = knobs()[0];
  n.turn(2, 1); tp.stepsOn = tempo();
  n.turn(2, 127); n.turn(2, 127); tp.top = tempo();
  n.turn(2, -127); n.turn(2, -127); n.turn(2, -127); tp.bottom = tempo();
  n.turn(2, 100);
  const marks = info(12);
  n.tap(B.SCL);                                     // SEL: a marker step before the run of turns
  n.turn(2, 1); n.turn(2, 1); n.held(B.HOME, () => n.tap(B.SAVE));
  tp.undone = [tempo(), info(12) !== marks];        /* the run undone in one step, the marker left */
  n.held(B.HOME, () => n.tap(B.SAVE)); tp.undone.push(info(12) - marks);
  n.turn(6, 9);                                     // bar 10, past the change to 133 at bar 9
  n.turn(2, 1); tp.past = [tempo(0), tempo(1), knobs()[0]]; n.turn(2, -1);
  n.held(B.FX, () => n.tap(B.PLAY));
  n.tap(B.PLAY); n.render(100);
  let at = clock(); n.render(300); const slow = clock() - at;
  n.turn(2, 60); at = clock(); n.render(300); const fast = clock() - at;
  tp.playing = { ratio: Math.round(fast / slow * 100) / 100, engine: info(54) & 0xffff, tempo: tempo() };
  n.tap(B.PLAY); n.turn(2, -60);
  // The settings page's K1: whole BPM, half with SHIFT (hundredths before).
  n.tap(B.GLO);
  tp.settings = [knobs()[0], (shifted(() => n.turn(2, 1)), tempo()), knobs()[0], (n.turn(2, 1), tempo())];
  n.turn(2, -1); n.tap(B.GLO);

  // 3. K2, input quantize, and the last take requantized. During the take PRESETS moves nothing.
  const qz = o.quantize = {};
  n.render(4);
  qz.footer = [knobs()[1], (n.turn(3, 5), info(29, 0, 5)), knobs()[1]];
  n.turn(3, -5);
  n.held(B.FX, () => n.tap(B.PLAY));
  const before = notes(0), start = info(59);
  take(() => {
    n.turn(6, 4);
    qz.duringTake = [info(16) >> 1 & 1, info(59) - start, tempo()];
    n.turn(2, 3);
    qz.duringTake.push(tempo());
  });
  qz.taken = added(before, 0).map(m => m[0]);
  qz.offGrid = qz.taken.some(at => at % 120);
  n.turn(3, 5);
  qz.requantized = [added(before, 0).map(m => m[0]), info(29, 0, 5), hintEnd()]; /* REQUANTIZED THE LAST TAKE: 25 letters */
  n.held(B.HOME, () => n.tap(B.SAVE));
  qz.undone = added(before, 0).map(m => m[0]);
  n.held(B.HOME, () => n.tap(B.SAVE));
  qz.gone = added(before, 0).length;                /* a second undo takes the take away */
  n.turn(3, -5);

  // 4. The clip editor: the same row, CLIP END on K4, and a take there requantized by K2.
  const cl = o.clip = {};
  n.tap(B.SEQ); n.tap(B.SCL);
  cl.page = [n.page(), info(42) & 255];
  cl.footer = knobs();
  n.turn(5, 1); cl.end = info(39, 0, 0); n.turn(5, -1);
  const inClip = notes(0);                          /* the open clip is its track's part */
  take();
  cl.taken = added(inClip, 0).map(m => m[0]);
  n.turn(3, 5);
  cl.requantized = added(inClip, 0).map(m => m[0]);
  n.turn(3, -5);
  /* K1 there is the tempo heard: where the song is, not the clip's own place. */
  n.tap(B.SEQ); n.tap(B.SEQ);
  n.turn(6, 9);                                     // bar 10, past the change to 133 at bar 9
  n.tap(B.SEQ); n.tap(B.SCL);                       // the same clip again, the song's place kept
  cl.songTempo = [knobs()[0], (n.turn(2, 1), tempo(1)), tempo(0)];
  n.turn(2, -1);
  n.tap(B.SEQ); n.tap(B.SEQ);

  // 5. The automation view: K1 the lane, K2 a change, stopped only, SHIFT+ALGO only zooming.
  const au = o.auto = {};
  n.held(B.FX, () => n.tap(B.LFO));
  n.held(B.FX, () => n.tap(B.PLAY));
  au.page = n.page();
  au.footer = knobs();
  n.turn(2, 20); au.program = knobs().slice(0, 2);
  n.turn(3, 3);
  au.set = [knobs()[1], Array.from({ length: info(0, 0) }, (_, i) => i).filter(i => info(30, 0, i) === 2).map(i => [info(1, 0, i), info(2, 0, i)])];
  n.turn(0, -1); au.conductor = knobs().slice(0, 2);
  n.turn(3, 1); shifted(() => n.turn(3, 1)); au.tempo = [tempo(), knobs()[1]];
  n.turn(2, 1); au.meter = knobs().slice(0, 2);
  n.turn(2, -1);
  n.tap(B.PLAY); n.render(50); n.turn(3, 1); au.playing = tempo(); n.tap(B.PLAY);
  const z0 = zoom(); shifted(() => n.turn(1, 1)); au.shiftAlgo = [tempo(), zoom() - z0]; n.turn(1, -1);
  n.render(600);
  au.hint = hintEnd();
  au.shiftHint = shifted(() => hintEnd());          /* PRESETS CHANGE TO CHANGE  SEL CLEAR: 35 letters */
  n.turn(6, 1); au.presets = clock();
  n.held(B.FX, () => n.turn(6, 1)); au.changeToChange = clock();
  const loopEnd = info(15); n.turn(5, 1); au.k4 = info(15) - loopEnd; n.turn(5, -1);
  n.held(B.FX, () => n.tap(B.LFO));

  // 6. Step entry at the song's end: PRESETS the pointer, K4 the pitch.
  const st = o.step = {};
  n.held(B.FX, () => n.tap(B.PLAY));
  n.turn(6, 12);
  n.step();
  st.page = n.page();
  st.footer = [knobs()[3], shifted(() => knobs()[3])];
  const p0 = info(17);
  n.key(7); n.key(11);                              // C4 and E4, a 16th apart
  n.turn(6, -2); st.back = info(17) - p0;
  const pitchAt = clock => { const m = notes(0).find(m => m[0] === clock); return m ? m[1] : -1; };
  n.turn(5, 1); st.k4 = pitchAt(p0);
  shifted(() => n.turn(5, 1)); st.octave = pitchAt(p0);
  shifted(() => n.turn(6, 1)); st.jumped = info(17) - p0;
  c.sim_control(0, B.LFO, 1); n.turn(6, -1); st.sweep = [info(58, 0, 3) & 1, info(58, 0, 5)];
  n.turn(5, 1); c.sim_control(0, B.LFO, 0);
  st.swept = [pitchAt(p0), pitchAt(p0 + 120)];
  n.turn(0, 7); st.drums = knobs()[3];
  n.step();
  return o;
}
function knobChecks(t, k) {
  const v = k.view, tp = k.tempo, qz = k.quantize, cl = k.clip, au = k.auto, st = k.step;
  t.eq('song view footer: TEMPO, QUANTIZE, LOOP IN, LOOP OUT', v.footer, ['TEMPO 120', 'QUANTIZE OFF', 'LOOP IN 3.1', 'LOOP OUT 7.1']);
  t.eq('K1 changes the tempo, not the playhead', v.k1, [0, 12100]);
  t.eq('PRESETS moves the playhead a snap step', v.presets, 1920);
  t.eq('K4 moves the loop\'s end, not the zoom', v.k4, [0, 1920]);
  t.eq('ALGO zooms, with ZOOM 8 BARS  SNAP 1/2 BAR on the bottom line', v.algo, [1, 6 + 24 * 6 + 4]);
  t.eq('and the notice runs out', v.noticeGone, v.hint);
  t.eq('SHIFT+PRESETS goes to the next marker', v.marker, 3840);
  t.eq('HOME+PRESETS is still the stock source page and never moves the playhead', v.home[0], 3840);
  t.no('HOME+PRESETS leaves the song view', v.home[1] === 19);
  t.eq('SEQ comes back to the song view', v.back, 19);
  t.eq('K1: whole BPM', tp.whole, [12100, 'TEMPO 121']);
  t.eq('K1 with SHIFT shows a decimal', tp.shiftShown, 'TEMPO 121.0');
  t.eq('SHIFT+K1: half BPM, one decimal in the footer, the header fitting it', [tp.half[0], tp.half[1]], [12150, 'TEMPO 121.5']);
  t.yes('the header\'s 121.5 ends on its fifth letter', tp.half[2] >= 224 && tp.half[2] < 230, tp.half[2]);
  t.eq('K1 from a half goes to the whole BPM above, and back down by whole BPM', [tp.halfUp, tp.back], [12200, 12000]);
  t.eq('an imported 119.88 shows its hundredths', tp.odd[0], 'TEMPO 119.88');
  t.yes('the header\'s 119.88 ends on its sixth letter, inside the screen', tp.odd[1] >= 230 && tp.odd[1] < 236, tp.odd[1]);
  t.eq('from 119.88: K1 to 120 or 119, SHIFT+K1 to 120 or 119.5', tp.snaps, [12000, 11900, 12000, 11950]);
  t.eq('119.5 shows one decimal', tp.snapShown, 'TEMPO 119.5');
  t.eq('then steps on: 119.5 to 120', tp.stepsOn, 12000);
  t.eq('K1 stops at 300 and 20 BPM', [tp.top, tp.bottom], [30000, 2000]);
  t.eq('a run of K1 turns is one undo step, and the step before it stays', tp.undone, [12000, true, 0]);
  t.eq('past the tempo change K1 turns that one, as the header shows it', tp.past, [12000, 13400, 'TEMPO 134']);
  t.eq('K1 while playing is heard at once, the engine following', tp.playing, { ratio: 1.5, engine: 180, tempo: 18000 });
  t.eq('settings page 1: SHIFT+K1 half BPM, K1 whole, as before', tp.settings, ['TEMPO 120', 12050, 'TEMPO 120.5', 12100]);
  t.eq('K2: input quantize, as the record settings name it', qz.footer, ['QUANTIZE OFF', 5, 'QUANTIZE 1/16']);
  t.eq('during a take PRESETS asks no jump and leaves the start point, K1 records instead of the tempo', qz.duringTake, [0, 0, 12000, 12000]);
  t.yes('the take: two notes, off the 1/16 grid', qz.taken.length === 2 && qz.offGrid, qz.taken);
  t.yes('K2 to 1/16 after it requantizes the take onto the grid', qz.requantized[0].length === 2 && qz.requantized[0].every(at => at % 120 === 0) &&
    qz.requantized[1] === 5, qz.requantized);
  t.eq('with REQUANTIZED THE LAST TAKE on the bottom line', qz.requantized[2], 6 + 24 * 6 + 4);
  t.eq('undo puts the take back as played, and again takes it away', [qz.undone, qz.gone], [qz.taken, 0]);
  t.eq('SEL in the grid opens the clip editor', cl.page, [19, 8]);
  t.eq('clip editor footer: TEMPO, QUANTIZE, LOOP IN, CLIP END', cl.footer, ['TEMPO 120', 'QUANTIZE OFF', 'LOOP IN 1.1', 'CLIP END 2.1']);
  t.eq('K4 sets the clip\'s end', cl.end, 3840);
  t.yes('a take in the clip editor, off the grid', cl.taken.length === 2 && cl.taken.some(at => at % 120), cl.taken);
  t.yes('K2 there requantizes it', cl.requantized.length === 2 && cl.requantized.every(at => at % 120 === 0), cl.requantized);
  t.eq('with the song past a tempo change, the clip editor\'s K1 shows and turns that one', cl.songTempo, ['TEMPO 133', 13400, 12000]);
  t.eq('the automation view', au.page, 24);
  t.eq('automation footer: LANE, VALUE, LOOP IN, LOOP OUT', au.footer, ['LANE VOLUME', 'VALUE -', 'LOOP IN 3.1', 'LOOP OUT 7.1']);
  t.eq('K1 picks the lane: programs last', au.program, ['LANE PROGRAM', 'VALUE -']);
  t.eq('K2 sets a program change at the playhead, its value in the footer', au.set, ['VALUE 4', [[0, 3]]]);
  t.eq('on the conductor K1 shows the tempo lane', au.conductor, ['LANE TEMPO', 'VALUE 120']);
  t.eq('K2 sets the tempo there, whole BPM, half with SHIFT', au.tempo, [12150, 'VALUE 121.5']);
  t.eq('K1 to time signatures', au.meter, ['LANE TIME SIG', 'VALUE 4/4']);
  t.eq('K2 while playing changes nothing', au.playing, 12150);
  t.eq('SHIFT+ALGO only zooms now', au.shiftAlgo, [12150, 1]);
  t.eq('the bottom line names no knob: TEMPO 121.5  SEL DEL', au.hint, 6 + 19 * 6 + 4);
  t.eq('with SHIFT: PRESETS CHANGE TO CHANGE  SEL CLEAR', au.shiftHint, 6 + 34 * 6 + 4);
  t.eq('PRESETS moves the playhead there too', au.presets, 1920);
  t.eq('SHIFT+PRESETS goes to the next tempo change', au.changeToChange, 15360);
  t.eq('K4 moves the loop\'s end', au.k4, 1920);
  t.eq('SHIFT+REC: step entry', st.page, 20);
  t.eq('step entry\'s K4 reads PITCH, OCTAVE with SHIFT', st.footer, ['PITCH < >', 'OCTAVE < >']);
  t.eq('PRESETS moves the pointer back two 16ths', st.back, 0);
  t.eq('K4 moves the note up a step', st.k4, 61);
  t.eq('SHIFT+K4 an octave', st.octave, 73);
  t.eq('SHIFT+PRESETS jumps to the next note', st.jumped, 120);
  t.eq('LFO+PRESETS sweeps both notes', st.sweep, [1, 2]);
  t.eq('K4 moves every swept note', st.swept, [74, 65]);
  t.eq('on the 808 K4 reads PAD', st.drums, 'PAD < >');
}
