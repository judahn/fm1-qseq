// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
// Phase 6 of the preview check, run from preview_check.mjs: edit
// jobs through the panel (SHIFT+EDIT, K1 the job, K2 to K4 its settings,
// YES asks and YES runs), then playback checked against exact block arithmetic.
// Quantize (with undo and redo), strum, crescendo, fit to scale and a
// cutoff ramp on one simulator. Normalize on a second, against a third
// that plays the same song unbaked: every note on the same block. Saves
// docs/phase6-*.png.
import { simPage, savePng, checker } from './preview_lib.mjs';

export async function phase6(browser, root) {
  const page = await simPage(browser, root);
  const out = await page.evaluate(() => {
    const B = previewButtons, same = (a, b) => JSON.stringify(a) === JSON.stringify(b);
    /* A simulator on the song view, with the job and playback helpers. */
    function rig() {
      const r = previewRig(), c = r.c;
      r.events = t => Array.from({ length: r.info(0, t) }, (_, i) => ({ clock: r.info(1, t, i), a: r.info(2, t, i),
        length: r.info(3, t, i), b: r.info(18, t, i), kind: r.info(30, t, i), glitch: r.info(35, t, i) }));
      r.notes = t => r.events(t).filter(e => !e.kind);
      /* Jobs: open the page, K1 to the job, settings, YES twice (it asks, then runs). */
      r.job = (index, settings = () => {}) => {
        r.held(B.FX, () => r.tap(B.EDIT));
        r.turn(2, -30); r.turn(2, index);
        settings();
        r.render(2);
        const shown = r.knobs();
        r.tap(B.YES); r.tap(B.YES);
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
        const heard = r.heard();
        c.sim_song_capture(0);
        return { start, heard };
      };
      /* Exact block of a clock, for playing that began at block `start` on clock 0. */
      r.block = () => { const tempo = r.tempo(); return (start, clock) => tempo.block(start, 0, clock); };
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
      r.tap(B.GLO); r.turn(4, 30); r.held(B.FX, () => r.turn(4, 1)); // swing 30% on 16ths (SHIFT+K3 since 0.5.8), every track
      r.turn(1, 3); r.turn(0, 1); r.turn(2, 9); r.turn(3, 12); // humanize track 2: 9 clocks, 12 steps
      r.turn(1, -3); r.tap(B.GLO);
      r.turn(0, -1);
      r.turn(6, 12);                                // the playhead to bar 13
      r.step(); r.tap(B.ARP);
      r.held(B.FX, () => r.key(4));                 // SHIFT+A3: quarters
      r.turn(2, 3); r.key(7); r.turn(2, -1); r.key(9);
      r.tap(B.ARP); r.tap(B.ARP); r.tap(B.ARP); r.step(); // ARP through the glitch pages to off, SHIFT+REC leaves
    };
    const plain = rig(), baked = rig();
    setup(plain); setup(baked);
    o.glitched = plain.notes(0).filter(n => n.glitch).length;
    o.normalizeShown = baked.job(18, () => baked.held(B.SCL, () => baked.key(14))); // all tracks
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
  for (const [name, key] of [['jobs', 'jobsPng'], ['ramp', 'rampPng'], ['song', 'songPng'], ['normalize', 'normalizePng']])
    savePng(root, 'phase6-' + name, out, key);
  await page.close();
  const t = checker('6'), q = out;
  t.eq('SHIFT+EDIT opens the job page', q.jobPage, 26);
  t.eq('quantize: the job shown', q.quantizeShown[0], 'JOB QUANTIZE');
  t.eq('quantize: grid 1/8', q.quantizeShown[2], 'GRID 1/8');
  t.over('quantize: the PSG had triplets off the 8th grid', q.quantize.before, 0);
  t.eq('quantize: none left off it', q.quantize.after, 0);
  t.yes('quantize: other tracks untouched', q.quantize.othersSame);
  t.yes('undo puts the triplets back', q.undone);
  t.yes('redo quantizes again', q.redone);
  t.eq('strum: the job shown', q.strumShown[0], 'JOB STRUM');
  t.eq('strum: a four-note chord at 4320', q.strum.chord.length, 4);
  t.eq('strum: its notes 30 clocks apart, low to high', q.strum.got, q.strum.chord.map((p, i) => [4320 + 30 * i, p]));
  t.eq('crescendo: the job shown', q.crescendoShown[0], 'JOB CRESCENDO');
  t.yes('crescendo: velocities ramp across the selection', q.crescendo);
  t.eq('the key set to D major', q.key, 2 | 1 << 8);
  t.eq('fit to scale: the job shown', q.fitShown[0], 'JOB FIT TO SCALE');
  t.eq('fit to scale: no note outside D major', q.fit.outside, 0);
  t.over('fit to scale: notes checked', q.fit.count, 0);
  t.eq('ramp: the job shown', q.rampShown[0], 'JOB RAMP');
  t.eq('ramp: a point every 16th', q.ramp.points, 64);
  t.eq('ramp: starts at 0', q.ramp.first.b, 0);
  t.eq('ramp: ends at 127', q.ramp.last.b, 127);
  t.yes('played: every note and cutoff change on its block', q.playback.ok, q.playback);
  t.over('played: events checked', q.playback.want, 600);
  t.eq('copy track: the sound shown', q.copyShown[3], 'COPY SOUND');
  t.eq('copy track: track 2 gets track 3\'s engine', q.copy.engine, q.copy.from);
  t.eq('copy track: track 3 is the PSG', q.copy.from, 3);
  t.eq('copy track: undo puts the 303 back', q.copy.undone, 1);
  t.eq('normalize: two glitched notes to bake', q.glitched, 2);
  t.eq('normalize: the job shown', q.normalizeShown[0], 'JOB NORMALIZE');
  t.yes('normalize: no glitch, swing or humanize left', q.bakedClean);
  t.atLeast('normalize: the repeats became notes', q.bakedMore, 5);
  t.yes('normalize: heard the same as unbaked, block for block', q.normalize.same, q.normalize);
  t.over('normalize: events compared', q.normalize.heard, 600);
  return { phase: '6', checks: t.list, out };
}
