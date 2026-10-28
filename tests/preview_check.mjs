// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
// Headless check of the browser preview (build/preview/index.html). Plays the
// test song through the simulator's exported panel controls, then checks
// every note the player sent against exact timing worked out here in BigInt
// arithmetic, independent of the C player. Phase 2: step-enters a phrase,
// plays it back the same way, and drives the settings page. Saves
// docs/phase1-song-view.png, docs/phase2-step-view.png and docs/phase2-settings.png.
// Needs Playwright with a cached Chromium:
//   PLAYWRIGHT_MODULE=/path/to/node_modules/playwright node tests/preview_check.mjs
import { createRequire } from 'node:module';
import { fileURLToPath, pathToFileURL } from 'node:url';
import fs from 'node:fs';
import path from 'node:path';

const require = createRequire(import.meta.url);
const { chromium } = require(process.env.PLAYWRIGHT_MODULE || 'playwright');
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const browser = await chromium.launch();
const page = await browser.newPage();
await page.goto(pathToFileURL(path.join(root, 'build/preview/index.html')).href);
await page.waitForFunction(() => globalThis.FM1SimulatorCore && typeof FM1_SIMULATOR === 'object');

const result = await page.evaluate(() => {
  const wasm = Uint8Array.from(atob(FM1_SIMULATOR.data), ch => ch.charCodeAt(0));
  const c = new FM1SimulatorCore(wasm, null, null).c;
  const BUTTON = { FX: 2, ENV: 4, SEQ: 11, PLAY: 12 }, KEY_G4 = 14;
  const tap = b => { c.sim_control(0, b, 1); c.sim_control(0, b, 0); };
  let rms = 0, blocks = 0;
  const render = n => {
    for (let i = 0; i < n; i++) {
      const out = new Float32Array(c.memory.buffer, c.sim_render(), 256);
      let sum = 0; for (const v of out) sum += v * v;
      rms += sum; blocks++;
    }
  };
  const info = (what, track = 0, index = 0) => c.sim_song_info(what, track, index) >>> 0;
  const heard = () => {
    const n = c.sim_song_heard_count(), at = c.sim_song_heard();
    const bytes = new Uint8Array(c.memory.buffer, at, n * 8), words = new Uint32Array(c.memory.buffer, at, n * 2);
    return Array.from({ length: n }, (_, i) => ({ block: words[i * 2], track: bytes[i * 8 + 4], pitch: bytes[i * 8 + 5], velocity: bytes[i * 8 + 6] }));
  };
  // Exact frame of a clock: sum of clocks * 44100 * 60 / (480 * bpm), as BigInt num/den.
  const tempos = Array.from({ length: info(4) }, (_, k) => ({ clock: info(5, 0, k), bpm100: BigInt(info(6, 0, k)) }));
  const den = tempos.reduce((d, t) => { let a = d, b = t.bpm100; while (b) [a, b] = [b, a % b]; return d / a * t.bpm100; }, 1n);
  const frameNum = clock => {   // frame * den
    let num = 0n;
    tempos.forEach((t, k) => {
      const to = Math.min(clock, k + 1 < tempos.length ? tempos[k + 1].clock : clock);
      if (to > t.clock) num += BigInt(to - t.clock) * 551250n * (den / t.bpm100);
    });
    return num;
  };
  const notes = [];
  for (let t = 0; t < 8; t++)
    for (let i = 0; i < info(0, t); i++) notes.push({ track: t, clock: info(1, t, i), pitch: info(2, t, i) });
  const compare = (expected, got) => {
    const key = n => n.block + ':' + n.track + ':' + n.pitch, want = new Map();
    for (const n of expected) want.set(key(n), (want.get(key(n)) || 0) + 1);
    const wrong = [];
    for (const n of got) { const k = key(n); if (want.get(k)) want.set(k, want.get(k) - 1); else wrong.push(n); }
    const missing = [...want].filter(([, v]) => v > 0).map(([k]) => k);
    return { expected: expected.length, got: got.length, wrong: wrong.slice(0, 5), missing: missing.slice(0, 5), ok: !wrong.length && !missing.length };
  };
  const balanced = list => {
    const on = new Map();
    for (const n of list) { const k = n.track * 128 + n.pitch; if (!!on.get(k) === !!n.velocity) return false; on.set(k, !!n.velocity); }
    return [...on.values()].every(v => !v);
  };

  // 1. The whole song from the top: SEQ opens the song view, PLAY starts it.
  render(32);
  c.sim_song_capture(1);
  tap(BUTTON.SEQ);
  const page1 = new Uint8Array(c.memory.buffer, c.sim_state(), 8)[1];
  tap(BUTTON.PLAY);
  render(9000);
  const startBlock = info(9), list = heard(), ons = list.filter(n => n.velocity);
  const expected = notes.map(n => ({ track: n.track, pitch: n.pitch,
    block: startBlock + Number(frameNum(n.clock) / (den * 128n)) }));
  const tempoBlock = startBlock + Number(frameNum(tempos[1].clock) / (den * 128n));
  const whole = { songPage: page1, startBlock, tempoChangeBlock: tempoBlock, stoppedAtEnd: info(7) === 0,
    notes: compare(expected, ons), offs: list.length - ons.length, balanced: balanced(list),
    afterTempoChange: ons.filter(n => n.block >= tempoBlock).length,
    perTrack: Array.from({ length: 8 }, (_, t) => ons.filter(n => n.track === t).length),
    rms: Math.sqrt(rms / (blocks * 256)) };
  // A few concrete notes, for the report: the 64th run and the bar 11 triplets.
  const sample = (track, clock) => {
    const n = notes.find(m => m.track === track && m.clock === clock);
    const exact = Number(frameNum(clock)) / Number(den);
    return { clock, exactFrame: +exact.toFixed(3), expectedBlock: startBlock + Math.floor(exact / 128),
      heardBlock: ons.find(h => h.track === track && h.pitch === n.pitch && h.block >= startBlock + Math.floor(exact / 128) - 1)?.block };
  };
  whole.samples = [sample(0, 7 * 1920 + 1440), sample(0, 7 * 1920 + 1440 + 30), sample(0, 7 * 1920 + 1440 + 450),
    sample(0, 10 * 1920 + 80), sample(0, 10 * 1920 + 160)];

  // 2. SHIFT + G4 jumps to marker 2 while stopped, ENV turns the loop on
  // (bars 3 to 6), PLAY plays it. Every pass must keep exact time.
  rms = blocks = 0;
  c.sim_song_capture(1);
  c.sim_control(0, BUTTON.FX, 1); c.sim_control(1, KEY_G4, 1); c.sim_control(1, KEY_G4, 0); c.sim_control(0, BUTTON.FX, 0);
  const jumped = info(8);
  tap(BUTTON.ENV);
  tap(BUTTON.PLAY);
  render(7000);
  const loopStart = 2 * 1920, loopEnd = 6 * 1920, start2 = info(9), list2 = heard(), ons2 = list2.filter(n => n.velocity);
  const passFrames = frameNum(loopEnd) - frameNum(loopStart), lastBlock = start2 + 7000;
  const expected2 = [];
  for (let pass = 0; pass < 4; pass++)
    for (const n of notes) {
      if (n.clock < loopStart || n.clock >= loopEnd) continue;
      const block = start2 + Number((frameNum(n.clock) - frameNum(loopStart) + BigInt(pass) * passFrames) / (den * 128n));
      if (block < lastBlock) expected2.push({ track: n.track, pitch: n.pitch, block });
    }
  const loop = { jumpedTo: jumped, startBlock: start2, passFrames: Number(passFrames) / Number(den),
    notes: compare(expected2, ons2), stillPlaying: info(7) === 1, rms: Math.sqrt(rms / (blocks * 256)) };

  // Screenshot of the song view while the loop plays, at 2x.
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
  tap(BUTTON.PLAY);
  loop.balancedAfterStop = balanced(heard());
  // Once stopped the engine must fall silent: no stuck notes.
  render(2000); rms = blocks = 0; render(1000);
  loop.rmsAfterStop = Math.sqrt(rms / (blocks * 256));

  // 3. Panel controls. Encoders: 2..5 are K1..K4. SHIFT is FX.
  const turn = (encoder, delta) => c.sim_control(2, encoder, delta);
  const knobs = () => { const b = new Uint8Array(c.memory.buffer, c.sim_knobs(), 128); const s = (i, f) => String.fromCharCode(...b.subarray(i * 32 + f * 16, i * 32 + f * 16 + 16)).replace(/\0.*$/, ''); return [0, 1, 2, 3].map(i => s(i, 0) + ' ' + s(i, 1)); };
  const shifted = fn => { c.sim_control(0, BUTTON.FX, 1); fn(); c.sim_control(0, BUTTON.FX, 0); };
  const controls = {};
  shifted(() => tap(BUTTON.PLAY));               // SHIFT+PLAY: to the loop start
  controls.rewindToLoopStart = info(8) === loopStart;
  tap(BUTTON.ENV);                                // loop off
  controls.loopOff = (info(16) & 1) === 0;
  turn(5, 3);                                     // K4: zoom in to 2 bars, snap 1/8
  controls.zoomKnobs = knobs();
  turn(2, 1);                                     // K1: one 8th
  controls.k1MovesBySnap = info(8) === loopStart + 240;
  shifted(() => turn(2, 1));                      // SHIFT+K1: next marker
  controls.shiftK1NextMarker = info(8) === 6 * 1920;
  turn(3, -1); turn(4, 2);                        // K2 / K3: loop edges by snap
  controls.loopEdges = [info(14), info(15)];
  controls.loopEdgesOk = info(14) === loopStart - 240 && info(15) === loopEnd + 480;
  turn(5, -3);
  // Marker jump while playing waits for the bar line: start at bar 1, ask
  // for marker 3 (BREAK, bar 7) in beat 2, then check the bar 2 line.
  shifted(() => { c.sim_control(1, 12, 1); c.sim_control(1, 12, 0); }); // F4: marker 1
  c.sim_song_capture(1);
  tap(BUTTON.PLAY);
  render(300);
  const start3 = info(9);
  shifted(() => { c.sim_control(1, 16, 1); c.sim_control(1, 16, 0); }); // A4: marker 3
  controls.pendingAfterRequest = (info(16) & 2) !== 0;
  controls.clockWhilePending = info(8);
  render(600);
  const barLine = start3 + Number(frameNum(1920) / (den * 128n)), list3 = heard().filter(n => n.velocity);
  const atBreak = notes.filter(n => n.clock === 6 * 1920).map(n => n.track + ':' + n.pitch).sort();
  const inBlock = list3.filter(n => n.block === barLine).map(n => n.track + ':' + n.pitch).sort();
  // After the bar line only bar 7 onward plays, in time from where bar 2 would have started.
  const lastBlock3 = start3 + 900, shift3 = frameNum(1920) - frameNum(6 * 1920);
  const afterJump = notes.map(n => ({ track: n.track, pitch: n.pitch, clock: n.clock,
    block: start3 + Number((frameNum(n.clock) + shift3) / (den * 128n)) }))
    .filter(n => n.clock >= 6 * 1920 && n.block < lastBlock3);
  controls.afterJump = compare(afterJump, list3.filter(n => n.block >= barLine));
  controls.jumpBlock = barLine;
  controls.jumpLandsOnBarLine = atBreak.length > 0 && JSON.stringify(inBlock) === JSON.stringify(atBreak);
  controls.bar1PlayedOut = list3.filter(n => n.block > start3 + 300 && n.block < barLine).length;
  controls.clockAfterJump = info(8);
  tap(BUTTON.PLAY);
  controls.balancedAfterStop = balanced(heard());
  // SEQ: song view to the stock step page and back, and the stock long
  // press on the step page still opens its actions page.
  const pageNow = () => new Uint8Array(c.memory.buffer, c.sim_state(), 8)[1];
  const pages = [pageNow()];
  tap(BUTTON.SEQ); pages.push(pageNow());
  tap(BUTTON.SEQ); render(4); pages.push(pageNow());
  tap(BUTTON.SEQ); c.sim_control(0, BUTTON.SEQ, 1); render(300); c.sim_control(0, BUTTON.SEQ, 0); pages.push(pageNow());
  controls.seqPages = pages;
  controls.seqOk = pages.join() === '19,1,19,12';
  return { whole, loop, controls, png: canvas.toDataURL('image/png').split(',')[1] };
});
// Phase 2: step entry and the settings page, on a fresh simulator.
const phase2 = await page.evaluate(() => {
  const wasm = Uint8Array.from(atob(FM1_SIMULATOR.data), ch => ch.charCodeAt(0));
  const c = new FM1SimulatorCore(wasm, null, null).c;
  const B = { FX: 2, ENV: 4, LFO: 5, GLO: 7, HOME: 8, SEQ: 11, PLAY: 12, REC: 13 };
  const tap = b => { c.sim_control(0, b, 1); c.sim_control(0, b, 0); };
  const key = k => { c.sim_control(1, k, 1); c.sim_control(1, k, 0); };
  const shiftKey = k => { c.sim_control(0, B.FX, 1); key(k); c.sim_control(0, B.FX, 0); };
  const turn = (encoder, delta) => c.sim_control(2, encoder, delta);
  const render = n => { for (let i = 0; i < n; i++) c.sim_render(); };
  const info = (what, track = 0, index = 0) => c.sim_song_info(what, track, index) >>> 0;
  const state = () => new Uint8Array(c.memory.buffer, c.sim_state(), 225);
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
  const out = {};
  render(32);
  tap(B.SEQ);
  turn(2, 12);                                   // K1: to the song end, bar 13
  const p0 = info(8);
  tap(B.REC);                                    // step entry on track 1
  out.stepPage = state()[1];
  out.pointerAtStart = info(17) === p0;
  // One bar: an 8th, three 8th triplets, a dotted 8th, a 16th tied to an
  // 8th (the length changes before the tie), an 8th rest, and a chord built
  // key by key.
  shiftKey(6); key(7);                           // 8th, C4
  shiftKey(1); key(9); key(11); key(12);         // 8th triplets D4 E4 F4
  shiftKey(1); shiftKey(3); key(14);             // dotted 8th G4
  shiftKey(3); shiftKey(3); shiftKey(4); key(16); // plain 16th A4
  shiftKey(6); key(26);                          // an 8th, then tie
  key(24);                                       // 8th rest
  const beforeChord = info(0, 0);
  c.sim_control(1, 7, 1); c.sim_control(1, 11, 1); c.sim_control(1, 14, 1);
  const whileHeld = info(0, 0);
  c.sim_control(1, 7, 0); c.sim_control(1, 11, 0); c.sim_control(1, 14, 0);
  out.chordWaitedForRelease = whileHeld === beforeChord && info(0, 0) === beforeChord + 3;
  const want = [[0, 60, 216], [240, 62, 144], [400, 64, 144], [560, 65, 144], [720, 67, 324], [1080, 69, 336],
    [1680, 60, 216], [1680, 64, 216], [1680, 67, 216]].map(([at, pitch, length]) => ({ clock: p0 + at, pitch, length }));
  const stored = [];
  for (let i = 0; i < info(0, 0); i++) if (info(1, 0, i) >= p0) stored.push({ clock: info(1, 0, i), pitch: info(2, 0, i), length: info(3, 0, i) });
  out.stored = stored;
  out.storedOk = JSON.stringify(stored) === JSON.stringify(want);
  out.pointerAfter = info(17) - p0;
  // Back onto the chord: K4 by an 8th. The pointer selects and previews it.
  c.sim_song_capture(1);
  turn(5, -1);
  out.previewedChord = (() => { const n = c.sim_song_heard_count(), b = new Uint8Array(c.memory.buffer, c.sim_song_heard(), n * 8);
    return Array.from({ length: n }, (_, i) => b[i * 8 + 5]).sort().join(); })();
  render(4);
  out.stepPng = png();
  render(200);                                   // the preview times out
  // REC leaves at the pointer. Back to the bar line and play the phrase.
  tap(B.REC);
  out.backInSong = state()[1];
  turn(2, -1);
  out.playheadAtPhrase = info(8) === p0;
  c.sim_song_capture(1);
  tap(B.PLAY);
  render(1600);
  const start = info(9), n = c.sim_song_heard_count(), at = c.sim_song_heard();
  const bytes = new Uint8Array(c.memory.buffer, at, n * 8), words = new Uint32Array(c.memory.buffer, at, n * 2);
  const heard = Array.from({ length: n }, (_, i) => ({ block: words[i * 2], track: bytes[i * 8 + 4], pitch: bytes[i * 8 + 5], velocity: bytes[i * 8 + 6] }));
  const tempos = Array.from({ length: info(4) }, (_, k) => ({ clock: info(5, 0, k), bpm100: BigInt(info(6, 0, k)) }));
  const den = tempos.reduce((d, t) => { let a = d, b = t.bpm100; while (b) [a, b] = [b, a % b]; return d / a * t.bpm100; }, 1n);
  const frameNum = clock => { let num = 0n; tempos.forEach((t, k) => {
    const to = Math.min(clock, k + 1 < tempos.length ? tempos[k + 1].clock : clock);
    if (to > t.clock) num += BigInt(to - t.clock) * 551250n * (den / t.bpm100); }); return num; };
  const block = clock => start + Number((frameNum(clock) - frameNum(p0)) / (den * 128n));
  const expected = [];
  for (const w of want) { expected.push(block(w.clock) + ':' + w.pitch + ':on'); expected.push(block(w.clock + w.length) + ':' + w.pitch + ':off'); }
  const got = heard.filter(h => h.track === 0).map(h => h.block + ':' + h.pitch + ':' + (h.velocity ? 'on' : 'off'));
  out.playback = { expected: expected.length, got: got.length, ok: JSON.stringify([...expected].sort()) === JSON.stringify([...got].sort()),
    first: got.slice(0, 4), stoppedAtEnd: info(7) === 0 };
  // Settings: GLO tap opens them. Key A minor on page 2, then swing 25% on
  // page 1 with track 2 left straight.
  tap(B.GLO);
  out.settingsPage = state()[1];
  turn(1, 1); turn(2, 9); turn(3, 2); turn(1, -1);
  turn(4, 25); key(2);
  out.swing = info(19) & 255; out.swingTracks = info(19) >>> 16; out.key = info(20);
  const leds = () => Array.from(state().subarray(168 + 14, 168 + 41));
  const whites = [0, 2, 4, 6, 7, 9, 11, 12];
  out.swingLeds = leds().every((on, k) => !!on === (whites.includes(k) && whites.indexOf(k) !== 1));
  render(4);
  out.settingsPng = png();
  tap(B.GLO);
  out.backFromSettings = state()[1];
  // The key lights show A minor on a pitched track: the white keys here.
  const major = [0, 2, 4, 5, 7, 9, 11];
  out.scaleLeds = leds().every((on, k) => !!on === major.includes((53 + k) % 12));
  // In step entry an out-of-scale key snaps up: C#4 enters D4. At the song
  // end, where the track is empty.
  turn(2, 20);
  tap(B.REC);
  const p1 = info(17), count = info(0, 0);
  key(8);
  out.snapped = info(0, 0) === count + 1 && info(1, 0, count) === p1 && info(2, 0, count) === 62;
  tap(B.ENV);                                    // back-delete removes it again
  out.backDeleted = info(0, 0) === count && info(17) === p1;
  tap(B.REC);
  c.sim_control(0, B.FX, 1); tap(B.GLO); c.sim_control(0, B.FX, 0);
  out.shiftGlo = state()[1];
  return out;
});
await browser.close();
fs.writeFileSync(path.join(root, 'docs/phase2-step-view.png'), Buffer.from(phase2.stepPng, 'base64'));
fs.writeFileSync(path.join(root, 'docs/phase2-settings.png'), Buffer.from(phase2.settingsPng, 'base64'));
delete phase2.stepPng; delete phase2.settingsPng;
result.phase2 = phase2;
const p2 = phase2.stepPage === 20 && phase2.pointerAtStart && phase2.chordWaitedForRelease && phase2.storedOk &&
  phase2.previewedChord === '60,64,67' && phase2.backInSong === 19 && phase2.playheadAtPhrase && phase2.playback.ok &&
  phase2.playback.stoppedAtEnd && phase2.settingsPage === 21 && phase2.swing === 25 && phase2.swingTracks === 0xfd &&
  phase2.key === (9 | 2 << 8) && phase2.swingLeds && phase2.backFromSettings === 19 && phase2.scaleLeds &&
  phase2.snapped && phase2.backDeleted && phase2.shiftGlo === 7;
fs.writeFileSync(path.join(root, 'docs/phase1-song-view.png'), Buffer.from(result.png, 'base64'));
delete result.png;
console.log(JSON.stringify(result, null, 1));
const k = result.controls;
const ok = result.whole.notes.ok && result.whole.balanced && result.whole.stoppedAtEnd && result.whole.songPage === 19 &&
  result.loop.notes.ok && result.loop.balancedAfterStop && result.whole.rms > 0.001 && result.loop.rmsAfterStop < 1e-4 &&
  k.rewindToLoopStart && k.loopOff && k.k1MovesBySnap && k.shiftK1NextMarker && k.loopEdgesOk &&
  k.pendingAfterRequest && k.jumpLandsOnBarLine && k.afterJump.ok && k.balancedAfterStop && k.seqOk && p2;
console.log(ok ? 'preview_check: passed' : 'preview_check: FAILED');
process.exit(ok ? 0 : 1);
