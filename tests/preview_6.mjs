// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
// Phase 6 of the headless preview check, run from preview_check.mjs: edit
// jobs through the panel (SHIFT+EDIT, K1 the job, K2 to K4 its settings,
// SAVE twice), then playback checked against exact block arithmetic.
// Quantize (with undo and redo), strum, crescendo, fit to scale and a
// cutoff ramp on one simulator. Normalize on a second, against a third
// that plays the same song unbaked: every note on the same block. Saves
// docs/phase6-*.png.
import { pathToFileURL } from 'node:url';
import fs from 'node:fs';
import path from 'node:path';

export async function phase6(browser, root) {
  const page = await browser.newPage();
  await page.goto(pathToFileURL(path.join(root, 'build/preview/index.html')).href);
  await page.waitForFunction(() => globalThis.FM1SimulatorCore && typeof FM1_SIMULATOR === 'object');
  const out = await page.evaluate(() => {
    const wasm = Uint8Array.from(atob(FM1_SIMULATOR.data), ch => ch.charCodeAt(0));
    const B = { FX: 2, ENV: 4, LFO: 5, EDIT: 6, GLO: 7, HOME: 8, SAVE: 9, ARP: 10, SEQ: 11, PLAY: 12, REC: 13 };
    const same = (a, b) => JSON.stringify(a) === JSON.stringify(b);
    function rig() {
      const c = new FM1SimulatorCore(wasm, null, null).c;
      const r = { c };
      r.tap = b => { c.sim_control(0, b, 1); c.sim_control(0, b, 0); };
      r.key = k => { c.sim_control(1, k, 1); c.sim_control(1, k, 0); };
      r.held = (b, fn) => { c.sim_control(0, b, 1); fn(); c.sim_control(0, b, 0); };
      r.turn = (encoder, delta) => c.sim_control(2, encoder, delta);
      r.render = n => { for (let i = 0; i < n; i++) c.sim_render(); };
      r.info = (what, track = 0, index = 0) => c.sim_song_info(what, track, index) >>> 0;
      r.page = () => new Uint8Array(c.memory.buffer, c.sim_state(), 8)[1];
      r.knobs = () => {
        const b = new Uint8Array(c.memory.buffer, c.sim_knobs(), 128);
        const s = o => { let t = ''; for (let i = 0; i < 16 && b[o + i]; i++) t += String.fromCharCode(b[o + i]); return t; };
        return [0, 1, 2, 3].map(k => s(k * 32) + ' ' + s(k * 32 + 16));
      };
      r.events = t => Array.from({ length: r.info(0, t) }, (_, i) => ({ clock: r.info(1, t, i), a: r.info(2, t, i),
        length: r.info(3, t, i), b: r.info(18, t, i), kind: r.info(30, t, i), glitch: r.info(35, t, i) }));
      r.notes = t => r.events(t).filter(e => !e.kind);
      r.png = () => {
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
      /* Jobs: open the page, K1 to the job, settings, SAVE twice. */
      r.job = (index, settings = () => {}) => {
        r.held(B.FX, () => r.tap(B.EDIT));
        r.turn(2, -30); r.turn(2, index);
        settings();
        r.render(2);
        const shown = r.knobs();
        r.tap(B.SAVE); r.tap(B.SAVE);
        r.render(2);
        return shown;
      };
      /* The whole song from the top: what was heard, by block. */
      r.play = () => {
        if (r.info(16) & 1) r.tap(B.ENV);
        r.held(B.FX, () => r.tap(B.PLAY));
        c.sim_song_capture(1);
        r.tap(B.PLAY);
        const start = r.info(9);
        for (let i = 0; i < 40000 && r.info(7); i++) c.sim_render();
        const n = c.sim_song_heard_count(), at = c.sim_song_heard();
        const bytes = new Uint8Array(c.memory.buffer, at, n * 8), words = new Uint32Array(c.memory.buffer, at, n * 2);
        const heard = Array.from({ length: n }, (_, i) => ({ block: words[i * 2], track: bytes[i * 8 + 4], a: bytes[i * 8 + 5],
          b: bytes[i * 8 + 6], kind: bytes[i * 8 + 7] }));
        c.sim_song_capture(0);
        return { start, heard };
      };
      /* Exact block of a clock, for playing that began at block `start` on clock 0. */
      r.block = () => {
        const tempos = Array.from({ length: r.info(4) }, (_, k) => ({ clock: r.info(5, 0, k), bpm100: BigInt(r.info(6, 0, k)) }));
        const den = tempos.reduce((d, t) => { let x = d, y = t.bpm100; while (y) [x, y] = [y, x % y]; return d / x * t.bpm100; }, 1n);
        const frameNum = clock => { let num = 0n; tempos.forEach((t, k) => {
          const to = Math.min(clock, k + 1 < tempos.length ? tempos[k + 1].clock : clock);
          if (to > t.clock) num += BigInt(to - t.clock) * 551250n * (den / t.bpm100); }); return num; };
        return (start, clock) => start + Number(frameNum(clock) / (den * 128n));
      };
      r.render(32);
      r.tap(B.SEQ);
      return r;
    }
    const tally = list => { const m = new Map(); for (const k of list) m.set(k, (m.get(k) || 0) + 1); return m; };
    const sameTally = (a, b) => a.size === b.size && [...a].every(([k, v]) => b.get(k) === v);
    const o = {};

    // 1. Quantize the PSG's triplets (track 3) to 8ths over the whole song.
    const A = rig();
    const loop = [A.info(14), A.info(15)];
    const others = [0, 1, 3].map(t => JSON.stringify(A.events(t)));
    A.turn(0, 2);                                  // SELECT: track 3
    const psg = A.notes(2);
    o.quantizeShown = A.job(0, () => { A.turn(3, 1); A.turn(4, 2); });   // whole song, 1/16 to 1/8
    o.jobPage = A.page();
    o.jobsPng = A.png();
    const q = A.notes(2);
    o.quantize = { before: psg.filter(n => n.clock % 240).length, after: q.filter(n => n.clock % 240).length, count: q.length,
      othersSame: same([0, 1, 3].map(t => JSON.stringify(A.events(t))), others) };
    A.held(B.HOME, () => A.tap(B.SAVE));           // undo
    o.undone = same(A.notes(2).map(n => n.clock).sort((x, y) => x - y), psg.map(n => n.clock).sort((x, y) => x - y));
    A.held(B.FX, () => A.held(B.HOME, () => A.tap(B.SAVE))); // redo
    o.redone = A.notes(2).filter(n => n.clock % 240).length === 0;
    A.tap(B.HOME);

    // 2. Strum the pluck's chords (track 4) up by 30 clocks in the selection.
    A.turn(0, 1);
    const chordAt = A.notes(3).filter(n => n.clock === 4320).map(n => n.a).sort((x, y) => x - y);
    o.strumShown = A.job(8, () => A.turn(4, 10));
    const strum = A.notes(3).filter(n => n.clock >= 4320 && n.clock < 4320 + 120);
    o.strum = { chord: chordAt, got: strum.map(n => [n.clock, n.a]).sort((x, y) => x[0] - y[0]),
      outside: A.notes(3).filter(n => n.clock < loop[0]).length };
    A.tap(B.HOME);

    // 3. Crescendo on the 303 (track 2), +40 across the selection.
    A.turn(0, -2);
    const bass = A.notes(1);
    o.crescendoShown = A.job(2, () => A.turn(4, 10));  // +30 to +40
    const swell = A.notes(1);
    o.crescendo = bass.every((n, i) => {
      const inside = n.clock >= loop[0] && n.clock < loop[1];
      const want = inside ? Math.min(127, Math.max(1, n.b + Math.round(40 * (n.clock - loop[0]) / (loop[1] - loop[0])))) : n.b;
      return swell[i].b === want;
    });
    A.tap(B.HOME);

    // 4. D major in the settings, then fit the DX7 (track 1) into it.
    A.tap(B.GLO); A.turn(1, 1); A.turn(2, 2); A.turn(3, 1); A.turn(1, -1); A.tap(B.GLO);
    A.turn(0, -1);
    o.key = A.info(20);
    o.fitShown = A.job(5, () => A.turn(3, 1));
    const scale = [2, 4, 6, 7, 9, 11, 1];
    o.fit = { outside: A.notes(0).filter(n => !scale.includes(n.a % 12)).length, count: A.notes(0).length };
    A.tap(B.HOME);

    // 5. A cutoff ramp on the 303 across the selection, every 16th: the defaults.
    A.turn(0, 1);
    o.rampShown = A.job(12, () => {});
    o.rampPng = A.png();                           // the page, with what it did
    A.tap(B.HOME);
    const ramp = A.events(1).filter(e => e.kind === 1 && e.a === 74);
    o.ramp = { points: ramp.length, first: ramp[0], last: ramp[ramp.length - 1] };
    A.render(2);
    o.songPng = A.png();

    // 6. Played from the top: every note and every cutoff change on the
    // block exact arithmetic gives.
    const run = A.play(), at = A.block();
    const want = [], got = [];
    for (let t = 0; t < 8; t++)
      for (const n of A.notes(t)) want.push(at(run.start, n.clock) + ':' + t + ':' + n.a + ':' + n.b);
    for (const e of ramp) want.push(at(run.start, e.clock) + ':cc:' + e.b);
    want.push(at(run.start, 0) + ':cc:' + ramp[0].b);   /* the start chases the lane's first value */
    for (const h of run.heard) {
      if (!h.kind && h.b) got.push(h.block + ':' + h.track + ':' + h.a + ':' + h.b);
      if (h.kind === 0xb0 && h.track === 1 && h.a === 74) got.push(h.block + ':cc:' + h.b);
    }
    o.playback = { want: want.length, got: got.length, ok: sameTally(tally(want), tally(got)) };

    // Copy Track's sound: track 3's setup (the PSG, where FROM started) onto
    // track 2, then undo puts the 303 back.
    o.copyShown = A.job(15, () => A.turn(5, 1));
    o.copy = { engine: A.info(34, 1), from: A.info(34, 2) };
    A.held(B.HOME, () => A.tap(B.SAVE));
    o.copy.undone = A.info(34, 1);
    A.tap(B.HOME);

    // 7. Normalize. Two simulators get the same swing, humanize and glitch
    // (repeats from step entry): one plays it as it is, the other bakes it
    // first. They must be heard the same, block for block.
    const setup = r => {
      r.tap(B.GLO); r.turn(4, 30); r.turn(5, 1);   // swing 30% on 16ths, every track
      r.turn(1, 3); r.turn(0, 1); r.turn(2, 9); r.turn(3, 12); // humanize track 2: 9 clocks, 12 steps
      r.turn(1, -3); r.tap(B.GLO);
      r.turn(0, -1);
      r.turn(2, 12);                                // the playhead to bar 13
      r.tap(B.REC); r.tap(B.REC); r.tap(B.ARP);
      r.held(B.FX, () => r.key(7));                 // quarters
      r.turn(2, 3); r.key(7); r.turn(2, -1); r.key(9);
      r.tap(B.ARP); r.tap(B.REC);
    };
    const plain = rig(), baked = rig();
    setup(plain); setup(baked);
    o.glitched = plain.notes(0).filter(n => n.glitch).length;
    o.normalizeShown = baked.job(18, () => baked.held(B.LFO, () => baked.key(14))); // all tracks
    o.normalizePng = baked.png();
    baked.tap(B.HOME);
    o.bakedClean = [0, 1, 2, 3, 4, 5, 6, 7].every(t => baked.notes(t).every(n => !n.glitch)) &&
      (baked.info(37, 1) === 0) && (baked.info(19) >>> 16) === 0;
    o.bakedMore = baked.notes(0).length - plain.notes(0).length;
    const key = h => (h.block - h.start) + ':' + h.track + ':' + h.a + ':' + h.b;
    const p1 = plain.play(), p2 = baked.play();
    const t1 = tally(p1.heard.filter(h => !h.kind).map(h => key({ ...h, start: p1.start })));
    const t2 = tally(p2.heard.filter(h => !h.kind).map(h => key({ ...h, start: p2.start })));
    o.normalize = { heard: p1.heard.filter(h => !h.kind).length, same: sameTally(t1, t2) };
    o.loop = loop;
    return o;
  });
  for (const [name, key] of [['jobs', 'jobsPng'], ['ramp', 'rampPng'], ['song', 'songPng'], ['normalize', 'normalizePng']]) {
    fs.writeFileSync(path.join(root, `docs/phase6-${name}.png`), Buffer.from(out[key], 'base64'));
    delete out[key];
  }
  await page.close();
  const q = out;
  const ok = q.jobPage === 26 && q.quantizeShown[0] === 'JOB QUANTIZE' && q.quantizeShown[2] === 'GRID 1/8' &&
    q.quantize.before > 0 && q.quantize.after === 0 && q.quantize.othersSame && q.undone && q.redone &&
    q.strumShown[0] === 'JOB STRUM' && q.strum.chord.length === 4 &&
    JSON.stringify(q.strum.got) === JSON.stringify(q.strum.chord.map((p, i) => [4320 + 30 * i, p])) &&
    q.crescendoShown[0] === 'JOB CRESCENDO' && q.crescendo && q.key === (2 | 1 << 8) && q.fitShown[0] === 'JOB FIT TO SCALE' &&
    q.fit.outside === 0 && q.fit.count > 0 && q.rampShown[0] === 'JOB RAMP' && q.ramp.points === 64 &&
    q.ramp.first.b === 0 && q.ramp.last.b === 127 && q.playback.ok && q.playback.want > 600 &&
    q.copyShown[3] === 'COPY SOUND' && q.copy.engine === q.copy.from && q.copy.from === 3 && q.copy.undone === 1 &&
    q.glitched === 2 && q.normalizeShown[0] === 'JOB NORMALIZE' && q.bakedClean && q.bakedMore >= 5 &&
    q.normalize.same && q.normalize.heard > 600;
  return { ok, out };
}
