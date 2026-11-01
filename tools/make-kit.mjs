// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
// A kit file from a folder of one-shots: node tools/make-kit.mjs FOLDER [--name NAME] [--out FILE] [--wavs]
//
// Pads come from FOLDER's audio files in name order, the first 8, each named by
// its file (a leading "3-" or "03 " is dropped, hyphens become spaces). Or
// FOLDER/kit.json says it all:
//   { "name": "AMEN", "source": "where the audio came from (a note, nothing reads it)",
//     "defaults": { "trim": true, "normalize": true, "fadeOut": 8 },
//     "pads": [ { "file": "break.wav", "name": "KICK", "start": 8.10, "end": 8.31 }, ... ] }
// Several pads can slice one file. Each pad takes the workbench's prep settings
// (start and end in seconds, fadeIn and fadeOut in ms, reverse, normalize,
// gain in dB, bits, hold in Hz) and trim, which starts the pad where the sound
// does. Without kit.json every pad gets trim, normalize and a 5 ms fade-out.
//
// WAV is read as is. AIFF, FLAC, MP3, OGG and M4A go through ffmpeg first, which
// must be installed. The prep and the SEA5 encoding are the workbench's own (wav.js, kit_file.js
// and the simulator), run in headless Chromium on the page ./build.sh made, so
// the file is the one the workbench would save. Open it there with "Open a kit
// file", then "Write kit to the device". It prints each pad's length and the
// flash it takes: 4 KB sectors, the device's unit (songs and kits share about
// 400 KB). With --wavs it also saves each pad as prepared, to listen to first,
// in a folder beside the kit file.
import { createRequire } from 'node:module';
import { fileURLToPath } from 'node:url';
import { execFileSync } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';

const require = createRequire(import.meta.url);
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const args = process.argv.slice(2), flag = name => { const i = args.indexOf(name); return i >= 0 ? args.splice(i, 2)[1] : null; };
const nameArg = flag('--name'), outArg = flag('--out'), wavs = args.includes('--wavs') && args.splice(args.indexOf('--wavs'), 1).length > 0;
const folder = args[0], AUDIO = /\.(wav|aiff?|flac|mp3|ogg|m4a)$/i;
if (!folder || !fs.statSync(folder, { throwIfNoEntry: false })?.isDirectory()) {
  console.error('usage: node tools/make-kit.mjs FOLDER [--name NAME] [--out FILE] [--wavs]');
  process.exit(2);
}

/* The kit as pads: a file, a name and prep settings each. */
const manifest = path.join(folder, 'kit.json');
let kit;
if (fs.existsSync(manifest)) {
  const m = JSON.parse(fs.readFileSync(manifest, 'utf8'));
  kit = { name: m.name, pads: m.pads.map(p => ({ ...(m.defaults || {}), ...p })) };
} else {
  const files = fs.readdirSync(folder).filter(f => AUDIO.test(f)).sort((a, b) => a.localeCompare(b, 'en', { numeric: true }));
  kit = { name: path.basename(path.resolve(folder)), pads: files.slice(0, 8).map(file => ({
    file, name: file.replace(AUDIO, '').replace(/^\d+[-_ ]+/, '').replace(/[-_]+/g, ' '), trim: true, normalize: true, fadeOut: 5 })) };
}
kit.name = (nameArg || kit.name || 'NEW KIT').toUpperCase().slice(0, 15);
if (!kit.pads.length || kit.pads.length > 8) throw Error('A kit has 1 to 8 pads, this one ' + kit.pads.length);
/* Another format as a 16-bit WAV at its own rate, which prep resamples. To a
 * file, since a WAV piped out has no length in its header. */
function wav(file) {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'make-kit-')), out = path.join(dir, 'pad.wav');
  try {
    execFileSync('ffmpeg', ['-v', 'error', '-i', file, '-acodec', 'pcm_s16le', out]);
    return fs.readFileSync(out);
  } finally { fs.rmSync(dir, { recursive: true, force: true }); }
}
kit.files = {}; /* each file once, as base64, however many pads slice it */
for (const p of kit.pads) {
  p.name = String(p.name || '').toUpperCase().replace(/[^ -~]/g, '').slice(0, 15);
  const file = path.join(folder, p.file);
  kit.files[p.file] ??= (/\.wav$/i.test(file) ? fs.readFileSync(file) : wav(file)).toString('base64');
}

const page = ['build/official-web-songfull/FM1-Official-Open-Source.html', 'build/official-web/FM1-Official-Open-Source.html',
  'build/preview/index.html'].map(p => path.join(root, p)).find(p => fs.existsSync(p));
if (!page) throw Error('No workbench page: run ./build.sh --package (or --preview) first');
const { chromium } = require(process.env.PLAYWRIGHT_MODULE || 'playwright');
const browser = await chromium.launch();
const tab = await browser.newPage();
await tab.goto('file://' + page);
await tab.waitForFunction(() => globalThis.FM1SongWorkbench?.kitFile && globalThis.FM1SongWorkbench?.wav && globalThis.FM1Web);
const result = await tab.evaluate(async kit => {
  const ns = FM1SongWorkbench, KF = ns.kitFile, W = ns.wav;
  const wasm = await FM1Web.assetBytes(typeof FM1_SIMULATOR !== 'undefined' ? FM1_SIMULATOR : globalThis.FM1_SIMULATOR, 2097152,
    'The simulator failed its check');
  const sim = new WebAssembly.Instance(new WebAssembly.Module(wasm), {}).exports;
  sim._initialize();
  const encode = data => { /* as ui_kits.js does */
    const pcm = W.toInt16(data), at = sim.sim_sample_alloc(pcm.length);
    if (!at) throw Error('A pad is too long for the sampler');
    new Int16Array(sim.memory.buffer, at, pcm.length).set(pcm);
    const out = sim.sim_sample_encode();
    if (!out) throw Error('Encoding failed');
    return new Uint8Array(sim.memory.buffer, out, sim.sim_sample_bytes()).slice();
  };
  const entries = [], report = [], sources = {};
  for (const p of kit.pads) {
    const source = sources[p.file] ??= W.decode(Uint8Array.from(atob(kit.files[p.file]), c => c.charCodeAt(0)));
    const o = {};
    for (const k of Object.keys(W.DEFAULTS)) if (p[k] !== undefined) o[k] = p[k];
    if (p.trim) { /* from where the sound reaches -48 dBFS, 1 ms early */
      const from = Math.round((o.start || 0) * source.rate), floor = 10 ** (-48 / 20);
      let i = from;
      while (i < source.data.length && Math.abs(source.data[i]) < floor) i++;
      o.start = Math.max(from, i - Math.round(source.rate / 1000)) / source.rate;
    }
    const prepared = W.prepare(source, o);
    if (!prepared.length) throw Error('Pad ' + p.name + ' is empty');
    const data = encode(prepared);
    entries.push({ name: p.name, frames: prepared.length, data });
    report.push({ name: p.name, seconds: prepared.length / W.RATE, sectors: KF.sectors(data.length),
      wav: kit.wavs ? btoa(String.fromCharCode(...W.encode(prepared))) : null });
  }
  while (entries.length < 8) entries.push({ name: '', frames: 0, data: new Uint8Array(0) });
  const file = KF.writeKit({ name: kit.name, params: null, bank: KF.buildBank(entries) });
  KF.readKit(file); /* what the workbench will check on opening it */
  let text = '';
  for (let i = 0; i < file.length; i += 8192) text += String.fromCharCode(...file.subarray(i, i + 8192));
  return { file: btoa(text), report };
}, { ...kit, wavs });
await browser.close();

const out = outArg || path.join(folder, kit.name.replace(/ /g, '-') + '.fm1kit');
fs.writeFileSync(out, Buffer.from(result.file, 'base64'));
let total = 0;
console.log(`${kit.name}: ${path.relative(process.cwd(), out)}`);
result.report.forEach((p, i) => {
  total += p.sectors;
  console.log(`  ${i + 1}  ${p.name.padEnd(15)} ${p.seconds.toFixed(3)} s  ${p.sectors} sector${p.sectors > 1 ? 's' : ''}`);
});
console.log(`  ${total} sectors, ${total * 4} KB on the device (samples it already has aren't sent again)`);
if (wavs) {
  const dir = out.replace(/\.fm1kit$/i, '') + '-pads';
  fs.mkdirSync(dir, { recursive: true });
  result.report.forEach((p, i) => fs.writeFileSync(path.join(dir, `${i + 1}-${p.name.replace(/ /g, '-')}.wav`), Buffer.from(p.wav, 'base64')));
  console.log(`  pads as prepared: ${path.relative(process.cwd(), dir)}`);
}
