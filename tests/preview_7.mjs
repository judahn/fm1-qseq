// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
// Phase 7a of the headless preview check, run from preview_check.mjs or on
// its own: the clip grid through the panel. SEQ opens it, SCL makes a clip
// and opens it in the song view's editor, step entry fills it, K3 sets a
// second clip's length, a track key launches a clip (starting playback),
// a scene key launches row 1 on every track at the next bar line, checked
// against exact block arithmetic, then the SONG row and SHIFT+SEQ hand the
// tracks back. Copy, paste, delete and undo on a cell, a row name, the
// launch quantize, and the clips surviving a save and a load. Saves
// docs/phase7-*.png.
//   PLAYWRIGHT_MODULE=/path/to/node_modules/playwright node tests/preview_7.mjs
import { createRequire } from 'node:module';
import { fileURLToPath, pathToFileURL } from 'node:url';
import fs from 'node:fs';
import path from 'node:path';

export async function phase7(browser, root) {
  const page = await browser.newPage();
  await page.goto(pathToFileURL(path.join(root, 'build/preview/index.html')).href);
  await page.waitForFunction(() => globalThis.FM1SimulatorCore && typeof FM1_SIMULATOR === 'object');
  const out = await page.evaluate(() => {
    const wasm = Uint8Array.from(atob(FM1_SIMULATOR.data), ch => ch.charCodeAt(0));
    const c = new FM1SimulatorCore(wasm, null, null).c;
    const B = { FX: 2, SCL: 3, ENV: 4, LFO: 5, GLO: 7, HOME: 8, SAVE: 9, SEQ: 11, PLAY: 12, REC: 13 };
    const SONG = 0xff, STOP = 0xfe;
    const tap = b => { c.sim_control(0, b, 1); c.sim_control(0, b, 0); };
    const key = k => { c.sim_control(1, k, 1); c.sim_control(1, k, 0); };
    const held = (b, fn) => { c.sim_control(0, b, 1); fn(); c.sim_control(0, b, 0); };
    const turn = (encoder, delta) => c.sim_control(2, encoder, delta);
    const render = n => { for (let i = 0; i < n; i++) c.sim_render(); };
    const info = (what, track = 0, index = 0) => c.sim_song_info(what, track, index) >>> 0;
    const state = () => new Uint8Array(c.memory.buffer, c.sim_state(), 225);
    const pageNow = () => state()[1];
    const keyLed = k => state()[168 + 14 + k];
    const from = t => info(41, t) & 255, waiting = t => info(41, t) >> 8;
    const cell = (t, row) => t * 16 + row;
    const part = (t, row) => 8 + cell(t, row);
    const notes = p => Array.from({ length: info(0, p) }, (_, i) => ({ clock: info(1, p, i), pitch: info(2, p, i), length: info(3, p, i) }));
    const heard = () => {
      const n = c.sim_song_heard_count(), at = c.sim_song_heard();
      const bytes = new Uint8Array(c.memory.buffer, at, n * 8), words = new Uint32Array(c.memory.buffer, at, n * 2);
      return Array.from({ length: n }, (_, i) => ({ block: words[i * 2], track: bytes[i * 8 + 4], pitch: bytes[i * 8 + 5],
        velocity: bytes[i * 8 + 6], kind: bytes[i * 8 + 7] })).filter(h => !h.kind);
    };
    const png = () => {
      const pixels = new Uint16Array(c.memory.buffer, c.sim_pixels(), 240 * 240);
      const canvas = document.createElement('canvas'); canvas.width = canvas.height = 480;
      const image = canvas.getContext('2d').createImageData(480, 480);
      for (let y = 0; y < 480; y++)
        for (let x = 0; x < 480; x++) {
          const v = pixels[(y >> 1) * 240 + (x >> 1)], at = (y * 480 + x) * 4;
          image.data[at] = (v >> 11) * 255 / 31; image.data[at + 1] = ((v >> 5) & 63) * 255 / 63;
          image.data[at + 2] = (v & 31) * 255 / 31; image.data[at + 3] = 255;
        }
      canvas.getContext('2d').putImageData(image, 0, 0);
      return canvas.toDataURL('image/png').split(',')[1];
    };
    // Exact block of a transport clock, for playing that began at block `start`
    // on clock 0 with no loop or jump since, so transport and song agree.
    const tempos = Array.from({ length: info(4) }, (_, k) => ({ clock: info(5, 0, k), bpm100: BigInt(info(6, 0, k)) }));
    const den = tempos.reduce((d, t) => { let x = d, y = t.bpm100; while (y) [x, y] = [y, x % y]; return d / x * t.bpm100; }, 1n);
    const frameNum = clock => { let num = 0n; tempos.forEach((t, k) => {
      const to = Math.min(clock, k + 1 < tempos.length ? tempos[k + 1].clock : clock);
      if (to > t.clock) num += BigInt(to - t.clock) * 551250n * (den / t.bpm100); }); return num; };
    const blockOf = (start, clock) => start + Number(frameNum(clock) / (den * 128n));
    const o = {};

    // 1. SEQ, SEQ: the grid. SCL on the empty cell of track 1, row 1: a new
    // one-bar clip in the editor. Step entry: four quarter notes. SEQ back.
    render(32);
    tap(B.SEQ); tap(B.SEQ);
    o.gridPage = pageNow();
    o.emptyPng = png();
    tap(B.SCL);
    o.clipPage = pageNow();
    o.opened = info(42) & 255;
    o.newLength = info(39, 0, cell(0, 0));
    tap(B.REC); tap(B.REC);
    o.stepPage = pageNow();
    held(B.FX, () => key(7));                       // quarters
    for (const k of [0, 4, 7, 11]) key(k);
    turn(5, -4);                                    // the pointer back over them, for the picture
    o.stepPng = png();
    tap(B.SEQ);
    o.backToGrid = pageNow();
    o.closed = info(42) & 255;
    o.clip1 = notes(part(0, 0));
    // 2. Track 2's clip, two bars long by K3, two half notes in each bar.
    turn(0, 1);
    tap(B.SCL);
    turn(4, 1);                                     // CLIP END to bar 3
    tap(B.REC); tap(B.REC);
    held(B.FX, () => key(9));                       // halves
    for (const k of [0, 7, 4, 11]) key(k);
    tap(B.REC);                                     // out of step entry, still in the clip
    o.inClipPage = pageNow();
    o.clipPng = png();
    tap(B.SEQ);
    o.clip2 = { length: info(39, 0, cell(1, 0)), notes: notes(part(1, 0)) };
    o.song2 = info(0, 1);                           // the song's own track 2, untouched

    // 3. F3 launches track 1's clip in row 1 while stopped: playing starts and
    // the clip plays from its start, bar after bar.
    turn(0, -1);
    c.sim_song_capture(1);
    key(0);
    o.playing = info(7);
    const start = info(9), at0 = info(10);
    render(1400);                                   // into bar 3
    o.from0 = from(0);
    o.ledLit = keyLed(0) === 1 && keyLed(2) === 0;
    let h = heard();
    const clipPitches = o.clip1.map(n => n.pitch);
    const want = [];
    for (let bar = 0; bar < 2; bar++) for (const n of o.clip1) want.push({ pitch: n.pitch, block: blockOf(start, at0 + bar * 1920 + n.clock) });
    const got = h.filter(x => x.track === 0 && x.velocity).slice(0, want.length);
    o.clipTiming = { want: want.length, ok: JSON.stringify(got.map(x => [x.pitch, x.block])) === JSON.stringify(want.map(x => [x.pitch, x.block])) };
    o.songOnOthers = h.filter(x => x.track !== 0 && x.velocity).length > 0;

    // 4. G4 launches row 1 everywhere at the next bar line: tracks 1 and 2
    // play their clips, the rest stop. The waiting keys blink meanwhile.
    key(14);
    const launchAt = info(45, 1);
    o.waiting = [0, 1, 2, 7].map(waiting);
    render(3);
    o.gridPng = png();
    const blinks = new Set();
    for (let i = 0; i < 40 && waiting(1) !== 0xfd; i++) { render(4); blinks.add(keyLed(2)); }
    o.blinked = blinks.size === 2;
    render(1500);
    o.sources = [0, 1, 2, 3, 4, 5, 6, 7].map(from);
    h = heard();
    const bar = blockOf(start, launchAt);
    o.launchBar = launchAt % 1920 === 0;
    const first1 = h.find(x => x.track === 1 && x.velocity && x.block >= bar - 2);
    o.track2OnTheBar = first1 && first1.block === bar && first1.pitch === o.clip2.notes[0].pitch;
    o.stoppedAfter = h.filter(x => x.track >= 2 && x.velocity && x.block > bar).length;
    o.offsAtBar = h.filter(x => x.track >= 2 && !x.velocity && x.block === bar).length;
    o.clip2Repeats = h.filter(x => x.track === 1 && x.velocity && x.block >= bar).length;
    o.relaunched = h.some(x => x.track === 0 && x.velocity && x.block === bar && x.pitch === clipPitches[0]);

    // 5. The SONG row's F3 hands track 1 back alone, then SHIFT+SEQ the rest.
    turn(2, -1);
    key(0);
    render(1500);
    o.afterSongRow = [0, 1].map(from);
    held(B.FX, () => tap(B.SEQ));
    o.allWaiting = [1, 2].map(waiting);
    render(1500);
    o.afterShiftSeq = [0, 1, 2, 7].map(from);
    o.songAgain = heard().filter(x => x.track === 5 && x.velocity && x.block > bar + 3000).length > 0;
    tap(B.PLAY);
    o.stopped = info(7);
    c.sim_song_capture(0);

    // 6. Copy row 1's clip to row 2, delete it, undo. Name row 1. Launch quantize.
    turn(2, 1);
    held(B.FX, () => key(0));                       // copy
    turn(2, 1);
    held(B.FX, () => key(4));                       // paste
    o.pasted = [info(39, 0, cell(0, 1)), info(40, 0, cell(0, 1))];
    held(B.FX, () => key(11));                      // delete
    o.deleted = info(39, 0, cell(0, 1));
    c.sim_control(0, B.HOME, 1); tap(B.SAVE); c.sim_control(0, B.HOME, 0); // undo
    o.undone = [info(39, 0, cell(0, 1)), info(40, 0, cell(0, 1))];
    o.afterUndoPage = pageNow();
    turn(2, -1);
    tap(B.ENV); turn(3, 1); turn(2, 1); turn(3, 2); tap(B.ENV);
    o.rowName = info(46, 0, 0);
    turn(5, -1);
    o.launchQ = info(42) >> 8;
    o.namedPng = png();
    // 7. SAVE keeps the grid: delete row 1's clip, load the song back.
    tap(B.SAVE);
    held(B.FX, () => key(0));                        // copy, then delete it
    held(B.FX, () => key(11));
    o.beforeLoad = info(39, 0, cell(0, 0));
    held(B.FX, () => tap(B.SAVE));
    o.listPage = pageNow();
    key(0); key(0);
    tap(B.HOME);
    o.loaded = [info(39, 0, cell(0, 0)), info(40, 0, cell(0, 0)), info(39, 0, cell(1, 0)), info(46, 0, 0), info(42) >> 8];
    return o;
  });
  for (const [name, key] of [['grid-empty', 'emptyPng'], ['step', 'stepPng'], ['clip', 'clipPng'], ['grid', 'gridPng'],
    ['named', 'namedPng']]) {
    fs.writeFileSync(path.join(root, `docs/phase7-${name}.png`), Buffer.from(out[key], 'base64'));
    delete out[key];
  }
  await page.close();
  const q = out, same = (a, b) => JSON.stringify(a) === JSON.stringify(b);
  const ok = q.gridPage === 27 && q.clipPage === 19 && q.opened === 8 && q.newLength === 1920 && q.stepPage === 20 &&
    q.backToGrid === 27 && q.closed === 0 && same(q.clip1.map(n => n.clock), [0, 480, 960, 1440]) &&
    q.inClipPage === 19 && q.clip2.length === 3840 && same(q.clip2.notes.map(n => n.clock), [0, 960, 1920, 2880]) &&
    q.song2 > 0 && q.playing === 1 && q.from0 === 0 && q.ledLit && q.clipTiming.ok && q.songOnOthers &&
    same(q.waiting, [0, 0, 0xfe, 0xfe]) && q.blinked && same(q.sources, [0, 0, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe]) &&
    q.launchBar && q.track2OnTheBar && q.stoppedAfter === 0 && q.offsAtBar > 0 && q.clip2Repeats >= 3 && q.relaunched &&
    same(q.afterSongRow, [0xff, 0]) && same(q.allWaiting, [0xff, 0xff]) && same(q.afterShiftSeq, [0xff, 0xff, 0xff, 0xff]) &&
    q.songAgain && q.stopped === 0 && same(q.pasted, [1920, 4]) && q.deleted === 0 && same(q.undone, [1920, 4]) &&
    q.afterUndoPage === 27 && (q.rowName & 0xffff) === 0x4241 && q.launchQ === 1 && q.beforeLoad === 0 && q.listPage === 25 &&
    same(q.loaded, [1920, 4, 3840, q.rowName, 1]);
  return { ok, out };
}

if (import.meta.url === pathToFileURL(process.argv[1]).href) {
  const require = createRequire(import.meta.url);
  const { chromium } = require(process.env.PLAYWRIGHT_MODULE || 'playwright');
  const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
  const browser = await chromium.launch();
  const run = await phase7(browser, root);
  await browser.close();
  console.log(JSON.stringify(run.out, null, 1));
  console.log(run.ok ? 'preview_7: passed' : 'preview_7: FAILED');
  process.exit(run.ok ? 0 : 1);
}
