// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
// Shared by the preview check's phases (preview_check.mjs runs them): the
// page with the simulator, an in-page rig for driving the panel, named
// checks, and the report. A check records its phase, its name, and the
// actual and expected values, so a failure says exactly what went wrong.
import { pathToFileURL } from 'node:url';
import fs from 'node:fs';
import path from 'node:path';

/* Runs in the page (it is sent there as source, so it stands alone). After
 * it, previewButtons names the panel buttons, and previewRig() boots a fresh
 * simulator and returns the helpers: panel controls, the song probes, what
 * the player sent, exact block timing from the tempo map, and the screen as
 * a PNG at 2x. */
function installRig() {
  const wasm = Uint8Array.from(atob(FM1_SIMULATOR.data), ch => ch.charCodeAt(0));
  globalThis.previewButtons = { OCT_DOWN: 0, OCT_UP: 1, NO: 0, YES: 1, /* OCT- is NO and OCT+ is YES on the song list, jobs and naming */
    FX: 2, SCL: 3, ENV: 4, LFO: 5, EDIT: 6, GLO: 7, HOME: 8, SAVE: 9, ARP: 10, SEQ: 11, PLAY: 12, REC: 13 };
  globalThis.previewRig = () => {
    const r = { B: previewButtons, wasm, meter: false, rms: 0, blocks: 0 };
    r.boot = () => { r.core = new FM1SimulatorCore(wasm, null, null); r.c = r.core.c; return r; };
    r.tap = b => { r.c.sim_control(0, b, 1); r.c.sim_control(0, b, 0); };
    r.key = k => { r.c.sim_control(1, k, 1); r.c.sim_control(1, k, 0); };
    r.held = (b, fn) => { r.c.sim_control(0, b, 1); fn(); r.c.sim_control(0, b, 0); };
    r.turn = (encoder, delta) => r.c.sim_control(2, encoder, delta);
    r.render = n => { /* with `meter` on, the output's level is summed too */
      for (let i = 0; i < n; i++) {
        const at = r.c.sim_render();
        if (!r.meter) continue;
        let sum = 0;
        for (const v of new Float32Array(r.c.memory.buffer, at, 256)) sum += v * v;
        r.rms += sum; r.blocks++;
      }
    };
    r.level = () => Math.sqrt(r.rms / (r.blocks * 256));
    r.info = (what, track = 0, index = 0) => r.c.sim_song_info(what, track, index) >>> 0;
    r.state = () => new Uint8Array(r.c.memory.buffer, r.c.sim_state(), 225);
    r.page = () => r.state()[1];
    r.lights = () => Array.from(r.state().subarray(168, 168 + 14)); /* the 14 button lights, 0 or 1 */
    r.pixels = () => new Uint16Array(r.c.memory.buffer, r.c.sim_pixels(), 240 * 240).slice(); /* the LCD */
    /* Pixels in [x0, x1) by [y0, y1) that differ from `color`, or from the
     * same pixels of `other` when it is an LCD from r.pixels(). */
    r.differ = (lcd, x0, y0, x1, y1, other = 0) => {
      let n = 0;
      for (let y = y0; y < y1; y++)
        for (let x = x0; x < x1; x++) n += lcd[y * 240 + x] !== (typeof other === 'number' ? other : other[y * 240 + x]);
      return n;
    };
    r.same = (a, b) => JSON.stringify(a) === JSON.stringify(b);
    r.knobs = () => {
      const b = new Uint8Array(r.c.memory.buffer, r.c.sim_knobs(), 128);
      const s = o => { let t = ''; for (let i = 0; i < 16 && b[o + i]; i++) t += String.fromCharCode(b[o + i]); return t; };
      return [0, 1, 2, 3].map(k => s(k * 32) + ' ' + s(k * 32 + 16));
    };
    /* Every message the player sent: a note (kind 0, pitch and velocity, also
     * as a and b), or a controller or program change (kind is its status). */
    r.heard = () => {
      const n = r.c.sim_song_heard_count(), at = r.c.sim_song_heard();
      const bytes = new Uint8Array(r.c.memory.buffer, at, n * 8), words = new Uint32Array(r.c.memory.buffer, at, n * 2);
      return Array.from({ length: n }, (_, i) => ({ block: words[i * 2], track: bytes[i * 8 + 4], pitch: bytes[i * 8 + 5],
        velocity: bytes[i * 8 + 6], a: bytes[i * 8 + 5], b: bytes[i * 8 + 6], kind: bytes[i * 8 + 7] }));
    };
    r.balanced = list => {
      const on = new Map();
      for (const n of list) { const k = n.track * 128 + n.pitch; if (!!on.get(k) === !!n.velocity) return false; on.set(k, !!n.velocity); }
      return [...on.values()].every(v => !v);
    };
    r.compare = (expected, got) => {
      const key = n => n.block + ':' + n.track + ':' + n.pitch, want = new Map();
      for (const n of expected) want.set(key(n), (want.get(key(n)) || 0) + 1);
      const wrong = [];
      for (const n of got) { const k = key(n); if (want.get(k)) want.set(k, want.get(k) - 1); else wrong.push(n); }
      const missing = [...want].filter(([, v]) => v > 0).map(([k]) => k);
      return { expected: expected.length, got: got.length, wrong: wrong.slice(0, 5), missing: missing.slice(0, 5), ok: !wrong.length && !missing.length };
    };
    /* Exact timing from the tempo map as it is now: frames * den for a clock,
     * and the block a clock lands in, for playing that began at block
     * `start` on clock `from`. */
    r.tempo = () => {
      const tempos = Array.from({ length: r.info(4) }, (_, k) => ({ clock: r.info(5, 0, k), bpm100: BigInt(r.info(6, 0, k)) }));
      const den = tempos.reduce((d, t) => { let x = d, y = t.bpm100; while (y) [x, y] = [y, x % y]; return d / x * t.bpm100; }, 1n);
      const frameNum = clock => { let num = 0n; tempos.forEach((t, k) => {
        const to = Math.min(clock, k + 1 < tempos.length ? tempos[k + 1].clock : clock);
        if (to > t.clock) num += BigInt(to - t.clock) * 551250n * (den / t.bpm100); }); return num; };
      return { tempos, den, frameNum, block: (start, from, clock) => start + Number((frameNum(clock) - frameNum(from)) / (den * 128n)) };
    };
    r.png = () => {
      const pixels = new Uint16Array(r.c.memory.buffer, r.c.sim_pixels(), 240 * 240);
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
    return r.boot();
  };
}

/* A page with the preview loaded and the rig installed. `file` picks the
 * hardware test build's preview (build/preview-test) instead. */
export async function simPage(browser, root, file = 'build/preview/index.html') {
  const page = await browser.newPage();
  await page.goto(pathToFileURL(path.join(root, file)).href);
  await page.waitForFunction(() => globalThis.FM1SimulatorCore && typeof FM1_SIMULATOR === 'object');
  await page.evaluate(installRig);
  return page;
}
/* Writes docs/<name>.png from the page's base64, and takes it out of `from`. */
export function savePng(root, name, from, key) {
  fs.writeFileSync(path.join(root, `docs/${name}.png`), Buffer.from(from[key], 'base64'));
  delete from[key];
}

/* A screenshot of a page element that comes out the same every run: the
 * element in view, the pointer parked where nothing hovers, no focus, fonts
 * loaded, and transitions finished (a button's hover fade lasts 0.12 s, and
 * every screenshot follows a click). Chromium still shades the rounded edges
 * of a few controls one level differently on some runs, so a shot within one
 * level of the file already there, pixel for pixel, leaves the file alone. */
export async function shot(locator, file) {
  const page = locator.page();
  await locator.scrollIntoViewIfNeeded();
  await page.mouse.move(0, 0);
  await page.evaluate(() => { document.activeElement?.blur?.(); return document.fonts.ready.then(() => undefined); });
  const png = await locator.screenshot({ animations: 'disabled' });
  if (!(await samePicture(page, file, png))) fs.writeFileSync(file, png);
}

async function samePicture(page, file, png) {
  if (!fs.existsSync(file)) return false;
  const old = fs.readFileSync(file);
  if (old.equals(png)) return true;
  return page.evaluate(async ([a, b]) => {
    const pixels = async (data) => {
      const img = new Image();
      img.src = 'data:image/png;base64,' + data;
      await img.decode();
      const c = document.createElement('canvas');
      c.width = img.width;
      c.height = img.height;
      const g = c.getContext('2d');
      g.drawImage(img, 0, 0);
      return g.getImageData(0, 0, c.width, c.height);
    };
    const p = await pixels(a), q = await pixels(b);
    if (p.width !== q.width || p.height !== q.height) return false;
    for (let i = 0; i < p.data.length; ++i) if (Math.abs(p.data[i] - q.data[i]) > 1) return false;
    return true;
  }, [old.toString('base64'), png.toString('base64')]);
}

/* Named checks for one phase. Each records its name, the actual value, what
 * was expected, and how they compare when it isn't equality. `yes` and `no`
 * take a detail to print on failure. */
export function checker(phase) {
  const list = [];
  const same = (a, b) => JSON.stringify(a) === JSON.stringify(b);
  const add = (name, ok, actual, expected, rule = '') => list.push({ phase, name, ok: !!ok, actual, expected, rule });
  const shown = (value, detail) => (detail === undefined ? value : { value, detail });
  return {
    list,
    eq: (name, actual, expected) => add(name, same(actual, expected), actual, expected),
    yes: (name, actual, detail) => add(name, !!actual, shown(actual, detail), 'true'),
    no: (name, actual, detail) => add(name, !actual, shown(actual, detail), 'false'),
    over: (name, actual, limit) => add(name, actual > limit, actual, limit, '>'),
    atLeast: (name, actual, limit) => add(name, actual >= limit, actual, limit, '>='),
    under: (name, actual, limit) => add(name, actual < limit, actual, limit, '<'),
    match: (name, actual, pattern) => add(name, pattern.test(actual), actual, String(pattern), 'matching'),
  };
}

const brief = value => {
  const text = typeof value === 'string' ? value : JSON.stringify(value);
  return text === undefined ? String(value) : text.length > 600 ? text.slice(0, 600) + '...' : text;
};
/* Prints every failure with its phase, name, actual and expected, then the
 * totals. With PREVIEW_DUMP set, the whole record of every phase first. */
export function report(runs) {
  if (process.env.PREVIEW_DUMP) console.log(JSON.stringify(Object.fromEntries(runs.map(r => [r.phase, r.out])), null, 1));
  const all = runs.flatMap(r => r.checks), failed = all.filter(c => !c.ok);
  for (const c of failed)
    console.log(`FAIL phase ${c.phase}: ${c.name}\n  actual:   ${brief(c.actual)}\n  expected: ${c.rule ? c.rule + ' ' : ''}${brief(c.expected)}`);
  for (const r of runs)
    console.log(`phase ${r.phase}: ${r.checks.length} checks, ${r.checks.filter(c => !c.ok).length} failed`);
  console.log(`preview_check: ${all.length} checks, ${failed.length} failed`);
  return !failed.length;
}
