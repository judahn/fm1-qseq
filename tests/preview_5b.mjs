// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
// Phase 5b of the headless preview check, run from preview_check.mjs: the
// workbench's Songs and Kits sections driven through their own controls
// against the page's simulator. Songs go down and back up byte for byte,
// through MIDI and back, and from a factory template. A kit built from
// generated WAVs is written, shared samples are stored once, and a pad
// plays. The stock sample loader and clear go through the pool, and the
// protocol's errors come back as they should. Saves docs/phase5b-*.png.
import { createRequire } from 'node:module';
import { pathToFileURL } from 'node:url';
import fs from 'node:fs';
import path from 'node:path';

const require = createRequire(import.meta.url);
export async function phase5b(browser, root) {
  for (const name of ['bytes', 'song_file', 'kit_file', 'midi_file', 'song_midi', 'wav'])
    require(path.join(root, 'web/workbench', name + '.js'));
  const { bytes: B, songFile: S, kitFile: K, midiFile: M, wav: W } = globalThis.FM1SongWorkbench;
  const context = await browser.newContext({ acceptDownloads: true, viewport: { width: 1180, height: 1000 } });
  const page = await context.newPage();
  page.on('dialog', dialog => dialog.accept());
  await page.goto(pathToFileURL(path.join(root, 'build/preview/index.html')).href);
  await page.waitForFunction(() => globalThis.FM1SimProtocol && document.getElementById('sw-section'));
  await page.click('#mode-library');
  const out = {};
  const status = () => page.textContent('#sw-status');
  const settle = async () => {
    await page.waitForFunction(() => document.getElementById('sw-status').textContent !== '');
    const text = await status();
    if (process.env.FM1_TRACE) console.log('5b:', text);
    return text;
  };
  const clear = () => page.evaluate(() => { document.getElementById('sw-status').textContent = ''; });
  const act = async selector => { await clear(); await page.click(selector); return settle(); };
  const download = async selector => {
    await clear();
    const [file] = await Promise.all([page.waitForEvent('download'), page.click(selector)]);
    await settle();
    return new Uint8Array(fs.readFileSync(await file.path()));
  };
  const open = async (selector, name, bytes) => {
    await clear();
    await page.setInputFiles(selector, { name, mimeType: 'application/octet-stream', buffer: Buffer.from(bytes) });
    return settle();
  };
  const device = () => page.evaluate(async () => {
    const link = FM1SongWorkbench.link.simulator(FM1SimProtocol);
    return { info: await link.info(), songs: await link.songs(), kits: await link.kits(), samples: (await link.samples()).length };
  });
  for (let i = 0; i < 40 && !/Up to date/.test(await act('#sw-refresh')); i++) await page.waitForTimeout(250);

  // 1. Save the working song, download it, upload it as a new song and over
  // the first one: every download is the same file, byte for byte.
  out.saved = await act('#sw-song-save');
  await page.selectOption('#sw-songs', '1');
  const first = await download('#sw-song-download');
  out.opened = await open('#sw-song-file', 'first.fm1song', first);
  out.uploaded = await act('#sw-current-upload');
  await page.selectOption('#sw-songs', '2');
  const second = await download('#sw-song-download');
  await page.selectOption('#sw-songs', '1');
  out.replaced = await act('#sw-current-replace');
  const third = await download('#sw-song-download');
  out.roundTrip = { bytes: first.length, uploadedSame: B.same(first, second), replacedSame: B.same(first, third) };

  // 2. A richer song made here (controllers, program changes, three meters
  // and tempos, a glitch, humanize, an FX stutter) goes up and comes back
  // byte for byte. Then MIDI out and back in through the import panel.
  const original = S.decode(S.unwrap(first));
  original.meters = [{ bar: 0, beats: 4, unit: 4 }, { bar: 2, beats: 7, unit: 8 }, { bar: 4, beats: 3, unit: 4 }];
  original.tempos = [{ clock: 0, bpm100: 12000 }, { clock: 3840, bpm100: 9050 }, { clock: 6000, bpm100: 17333 }];
  const lane = (t, clock, kind, a, b) => {
    const list = original.tracks[t], e = S.event(clock, kind, a, b, 0);
    list.splice(list.findIndex(x => S.clockOf(x) > clock) >>> 0, 0, e);
  };
  for (let i = 0; i < 8; i++) { lane(0, i * 480, S.KIND.CONTROL, 7, 60 + i * 8); lane(1, i * 240, S.KIND.CONTROL, 74, 20 + i * 12); }
  lane(0, 3840, S.KIND.PROGRAM, 9, 0); lane(1, 1920, S.KIND.PROGRAM, 1, 0); lane(2, 960, S.KIND.CONTROL, 72, 100);
  original.glitch = [{ index: 1, word: 1 }];
  original.tracks[0][1].at = (original.tracks[0][1].at | 1 << 26) >>> 0;
  original.humanizeTime[2] = 6;
  lane(0, 1920, S.KIND.FX, 2, 64);
  original.tracks[0].find(e => S.kindOf(e) === S.KIND.FX).length = 480;
  const rich = S.encode(original);
  S.decode(rich);
  out.richOpened = await open('#sw-song-file', 'rich.fm1song', S.wrap(rich));
  out.richUploaded = await act('#sw-current-upload');
  await page.selectOption('#sw-songs', '3');
  out.richSame = B.same(await download('#sw-song-download'), S.wrap(rich));
  const mid = await download('#sw-song-midi');
  out.midiStatus = await status();
  const parsed = M.read(mid), metas = kind => parsed.tracks[0].filter(e => e.meta === kind).length;
  out.midi = { format: parsed.format, division: parsed.division, tracks: parsed.tracks.length, tempos: metas(M.META.TEMPO),
    meters: metas(M.META.METER), markers: metas(M.META.MARKER),
    controllers: parsed.tracks.flat().filter(e => e.status === 0xb0).length, programs: parsed.tracks.flat().filter(e => e.status === 0xc0).length };
  out.midiRead = await open('#sw-song-file', 'rich.mid', mid);
  out.report = await page.textContent('#sw-import-report');
  await page.locator('#sw-import').screenshot({ path: path.join(root, 'docs/phase5b-midi-import.png') });
  out.imported = await act('#sw-import-go');
  const back = S.decode(S.unwrap(await download('#sw-current-save')));
  const key = e => [S.clockOf(e), S.kindOf(e), e.a, e.b, e.length].join(':');
  const content = song => JSON.stringify(song.tracks.map(list => list.filter(e => S.kindOf(e) !== S.KIND.FX).map(key).sort()));
  const markers = song => JSON.stringify(song.markers.map(m => [m.clock, S.markerName(m)]));
  out.midiRoundTrip = { events: S.eventCount(back), notesAndLanes: content(back) === content(original),
    tempos: JSON.stringify(back.tempos) === JSON.stringify(original.tempos), meters: JSON.stringify(back.meters) === JSON.stringify(original.meters),
    markers: markers(back) === markers(original), sounds: B.same(back.sounds, original.sounds), name: S.name(back) === S.name(original) };
  out.importUploaded = await act('#sw-current-upload');
  await page.selectOption('#sw-songs', '4');
  out.importLoaded = await act('#sw-song-load');

  // 3. A factory template: built in a private simulator, uploaded, loaded.
  await page.focus('#sw-templates');
  await page.waitForFunction(() => document.getElementById('sw-templates').options.length > 1);
  await clear();
  await page.selectOption('#sw-templates', '1');
  out.template = await settle();
  out.templateUploaded = await act('#sw-current-upload');
  await page.selectOption('#sw-songs', '5');
  const template = S.decode(S.unwrap(await download('#sw-song-download')));
  out.templateLoaded = await act('#sw-song-load');
  const factory = await page.evaluate(async () => {
    const core = new FM1SimulatorCore(await FM1Web.assetBytes(FM1_SIMULATOR, 2097152, ''), null, null);
    return Array.from(core.command({ type: 'factory', index: 1 }).subarray(2048, 2048 + 8 * 223));
  });
  const types = Array.from({ length: 8 }, (_, t) => template.sounds[1 + t * 223]);
  out.templateSong = { name: S.name(template), kit: template.kit, events: S.eventCount(template),
    sameTypes: types.join() === Array.from({ length: 8 }, (_, t) => factory[t * 223]).join(), types };
  out.afterTemplate = (await device()).info;
  await page.locator('#sw-songs-part').screenshot({ path: path.join(root, 'docs/phase5b-songs.png') });

  // 4. A kit from generated WAVs, prepared per pad, written twice: the
  // second time every sample is already there.
  const wave = (rate, seconds, fn) => W.encode(Float32Array.from({ length: Math.round(rate * seconds) }, (_, i) => fn(i / rate)), rate);
  let noise = 1;
  const random = () => ((noise = (noise * 1103515245 + 12345) >>> 0) / 2 ** 32) * 2 - 1;
  const wavs = [wave(44100, 0.4, t => Math.sin(2 * Math.PI * (60 + 90 * Math.exp(-t * 30)) * t) * Math.exp(-t * 8)),
    wave(22050, 0.25, t => random() * Math.exp(-t * 18) * 0.7), wave(48000, 0.08, t => random() * (1 - t / 0.08) * 0.5),
    wave(32000, 0.3, t => 0.6 * Math.sin(2 * Math.PI * 880 * t))];
  const pad = (n, k) => `#sw-pads tr:nth-child(${n}) [data-k="${k}"]`;
  for (let n = 1; n <= 4; n++) {
    await page.setInputFiles(pad(n, 'wav'), { name: ['KICK', 'SNARE', 'HAT', 'TONE'][n - 1] + '.wav', mimeType: 'audio/wav', buffer: Buffer.from(wavs[n - 1]) });
    await page.waitForFunction(selector => document.querySelector(selector).value !== '', pad(n, 'name'));
  }
  await page.fill(pad(1, 'end'), '0.25'); await page.fill(pad(1, 'fadeOut'), '50'); await page.check(pad(1, 'normalize'));
  await page.check(pad(2, 'reverse')); await page.fill(pad(2, 'gain'), '-3');
  await page.fill(pad(3, 'bits'), '8'); await page.fill(pad(3, 'hold'), '8000');
  await page.fill(pad(4, 'start'), '0.05'); await page.fill(pad(4, 'fadeIn'), '10');
  await page.fill('#sw-kit-name', 'TEST KIT');
  const before = await device();
  out.kitWritten = await act('#sw-kit-write');
  const afterOne = await device();
  await page.fill('#sw-kit-name', 'TEST KIT 2');
  out.kitAgain = await act('#sw-kit-write');
  const afterTwo = await device();
  const kitFile = await download('#sw-kit-save');
  const kit = K.readKit(kitFile);
  out.kitFile = { name: kit.name, bytes: kitFile.length, pads: kit.pads.map(p => [p.name, p.frames]) };
  out.kitOpened = await open('#sw-kit-file', 'kit.fm1kit', kitFile);
  await page.fill('#sw-kit-name', 'TEST KIT 3');
  out.kitThird = await act('#sw-kit-write');
  const afterThree = await device();
  await page.selectOption('#sw-kits', String(afterThree.kits.at(-1).id));
  out.kitDeleted = await act('#sw-kit-delete');
  const afterDelete = await device();
  await page.selectOption('#sw-kits', String(afterOne.kits.at(-1).id));
  out.kitUsed = await act('#sw-kit-use');
  out.kits = { samplesBefore: before.samples, afterOne: afterOne.samples, afterTwo: afterTwo.samples, afterThree: afterThree.samples,
    afterDelete: afterDelete.samples, kits: afterDelete.kits.map(k => k.name), inUse: (await device()).info.kit, newKit: afterOne.kits.at(-1).id };
  out.gauge = await page.textContent('#sw-pool-text');
  await page.evaluate(() => { document.querySelector('.sw-wide').scrollLeft = 0; });
  await page.locator('#sw-kits-part').screenshot({ path: path.join(root, 'docs/phase5b-kits.png') });

  // 5. A pad plays: the page's flash in a fresh simulator, its sampler track
  // (track 5, channel 5) hit with the stock kit and then the new one.
  out.pad = await page.evaluate(async newKit => {
    const text = localStorage.getItem('fm1.simulator.flash.v4'), flash = Uint8Array.from(atob(text), ch => ch.charCodeAt(0));
    const core = new FM1SimulatorCore(await FM1Web.assetBytes(FM1_SIMULATOR, 2097152, ''), flash), c = core.c;
    const link = FM1SongWorkbench.link.simulator(async frames => core.command({ type: 'protocol', frames }));
    const hit = note => {
      const midi = bytes => { new Uint8Array(c.memory.buffer, c.sim_file(), 3).set(bytes); c.sim_midi(3); };
      const block = () => Array.from(new Float32Array(c.memory.buffer, c.sim_render(), 256));
      for (let i = 0; i < 40; i++) block();
      midi([0x94, note, 120]);
      const sound = []; for (let i = 0; i < 24; i++) sound.push(...block());
      midi([0x84, note, 0]);
      return { rms: Math.sqrt(sound.reduce((s, v) => s + v * v, 0) / sound.length), sound };
    };
    const pads = () => [8, 9, 10, 11, 12, 13, 14, 15].map(i => c.sim_song_info(38, 0, i) >>> 0);
    await link.useKit(1);
    const stock = { pads: pads(), kick: hit(36) };
    await link.useKit(newKit);
    const mine = { pads: pads(), kick: hit(36) };
    return { stockPads: stock.pads, newPads: mine.pads, stockRms: stock.kick.rms, newRms: mine.kick.rms,
      differs: stock.kick.sound.some((v, i) => Math.abs(v - mine.kick.sound[i]) > 1e-4) };
  }, out.kits.newKit);

  // 6. The stock sample loader (its "send to simulator") makes a kit in the
  // pool, and leaves the songs as they were. Clear drops songs, not kits.
  out.loader = await page.evaluate(async () => {
    const ns = FM1SongWorkbench, link = ns.link.simulator(FM1SimProtocol);
    const songs = async () => { /* one read at a time: the device keeps one read open */
      const out = [];
      for (const s of await link.songs()) out.push(s.id + ':' + (await link.readSong(s.id)).crc);
      return out;
    };
    const before = { songs: (await songs()).join(), kits: (await link.kits()).length, samples: (await link.samples()).length };
    const bank = (await link.readKit(1)).bank.slice(), pads = ns.kitFile.readBank(bank);
    const at = bank.length - pads[7].data.length + 9;
    bank[at] ^= 0x55;
    bank.set(Uint8Array.of(...[0, 8, 16, 24].map(s => ns.kitFile.bankCrc(bank) >>> s & 255)), 12);
    await FM1SamplePreview(bank);
    const info = await link.info();
    return { before, after: { songs: (await songs()).join(), kits: (await link.kits()).length, samples: (await link.samples()).length, kit: info.kit } };
  });
  await page.click('#mode-sim');
  await page.evaluate(() => document.getElementById('sim-clear').click());
  await page.waitForTimeout(500);
  out.cleared = (await device()).info;

  // 7. Errors: playing, a full pool and bad data are refused with their codes,
  // and the stock commands still answer.
  out.errors = await page.evaluate(async () => {
    const ns = FM1SongWorkbench, core = new FM1SimulatorCore(await FM1Web.assetBytes(FM1_SIMULATOR, 2097152, ''), null, null), c = core.c;
    const link = ns.link.simulator(async frames => core.command({ type: 'protocol', frames }));
    const code = async (command, payload) => { try { const r = await link.one(command, payload); return r.status; } catch (e) { return e.status; } };
    const u32 = v => [v & 255, v >>> 8 & 255, v >>> 16 & 255, v >>> 24];
    const out = { stock11: (await link.one(11)).data[0], stock0: (await link.one(0)).data.length };
    const id = await link.saveWorking(true), payload = (await link.readSong(id)).bytes;
    const tap = b => { c.sim_control(0, b, 1); c.sim_control(0, b, 0); };
    tap(11); tap(12); for (let i = 0; i < 8; i++) c.sim_render();
    out.playing = await code(36, [0, 0, ...u32(payload.length)]);
    tap(12); for (let i = 0; i < 8; i++) c.sim_render();
    const bad = payload.slice(); bad[1 + 2 + 1999 + 12 + 2 + 8 + 2 + 1 + 4] = 0; bad[1 + 2 + 1999 + 12 + 2 + 8 + 2 + 1 + 5] = 0; /* tempo 0 */
    await link.one(36, [0, 0, ...u32(bad.length)]);
    for (let at = 0; at < bad.length; at += 128) await link.one(37, [...u32(at), ...bad.subarray(at, at + 128)]);
    out.invalid = await code(38, u32(ns.bytes.crc32(bad)));
    let copies = 0;
    try { for (;;) { await link.writeSong(0, payload); copies++; } } catch (e) { out.preCheck = e.message; }
    out.copies = copies;
    out.fullSong = await code(36, [0, 0, ...u32(payload.length)]);
    const bank = (await link.readKit(1)).bank;
    const kit = ns.kitFile.readBank(bank).map((p, i) => ({ ...p, data: p.data.map((v, j) => (j % 228 > 8 ? v ^ (i + 1) : v)) }));
    const fresh = ns.kitFile.buildBank(kit);
    out.samplesBefore = (await link.samples()).length;
    await link.one(12, u32(fresh.length));
    let full = 0;
    for (let at = 0; at < fresh.length && !full; at += 128) full = await code(13, [...u32(at), ...fresh.subarray(at, at + 128)]);
    out.fullKit = full;
    out.samplesAfter = (await link.samples()).length;
    return out;
  });
  await context.close();
  const q = out, k = q.kits, p = q.pad, l = q.loader, e = q.errors;
  const ok = /saved as song 1/.test(q.saved) && /Uploaded as song 2/.test(q.uploaded) && /replaced/.test(q.replaced) &&
    q.roundTrip.uploadedSame && q.roundTrip.replacedSame && q.midi.format === 1 && q.midi.division === 480 && q.midi.tracks === 9 &&
    q.midi.tempos === original.tempos.length && q.midi.meters === original.meters.length && q.midi.markers === original.markers.length &&
    q.midi.controllers === 17 && q.midi.programs >= 2 && /Uploaded as song 3/.test(q.richUploaded) && q.richSame &&
    /1 glitch attributes, 1 FX lane events, humanize/.test(q.midiStatus) &&
    /Imported/.test(q.imported) && Object.values(q.midiRoundTrip).every(v => v === true || v > 0) &&
    /Uploaded as song 4/.test(q.importUploaded) && /Loaded/.test(q.importLoaded) && /Template ready/.test(q.template) &&
    /Uploaded as song 5/.test(q.templateUploaded) && q.templateSong.name === 'ACID ALLEY' && q.templateSong.kit === 0 &&
    q.templateSong.events === 0 && q.templateSong.sameTypes && q.afterTemplate.song === 5 && q.afterTemplate.kit === 1 &&
    /written: 4 new samples, 0 already/.test(q.kitWritten) && /written: 0 new samples, 4 already/.test(q.kitAgain) &&
    /written: 0 new samples, 4 already/.test(q.kitThird) && k.afterOne === k.samplesBefore + 4 && k.afterTwo === k.afterOne &&
    k.afterThree === k.afterOne && k.afterDelete === k.afterOne && k.inUse === k.newKit && q.kitFile.name === 'TEST KIT 2' &&
    q.kitFile.pads.filter(x => x[1]).length === 4 && q.kitFile.pads[0][1] === 8000 && p.newRms > 1e-3 && p.differs &&
    p.newPads.slice(0, 4).every((a, i) => a && a !== p.stockPads[i]) && p.newPads.slice(4).every(a => !a) &&
    l.after.songs === l.before.songs && l.after.kits === l.before.kits + 1 && l.after.samples === l.before.samples + 1 &&
    q.cleared.songs === 0 && q.cleared.kits === l.after.kits && e.stock11 === 2 && e.stock0 === 14 && e.playing === 85 &&
    e.invalid === 71 && e.copies > 10 && /Not enough room/.test(e.preCheck) && e.fullSong === 84 && e.fullKit === 84 && e.samplesAfter === e.samplesBefore;
  return { ok, out };
}
