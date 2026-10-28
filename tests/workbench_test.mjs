// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
// Node unit tests for the song workbench (web/workbench, phase 5b): the MIDI
// writer and reader, the song and kit file codecs, songs to MIDI and back,
// and sample prep. No dependencies: node tests/workbench_test.mjs
import { createRequire } from 'node:module';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const require = createRequire(import.meta.url);
const dir = path.join(path.dirname(fileURLToPath(import.meta.url)), '../web/workbench');
for (const name of ['bytes', 'song_file', 'kit_file', 'midi_file', 'song_midi', 'wav']) require(path.join(dir, name + '.js'));
const { bytes: B, songFile: S, kitFile: K, midiFile: M, songMidi: SM, wav: W } = globalThis.FM1SongWorkbench;

let checks = 0, failed = 0;
const check = (ok, what) => { checks++; if (!ok) { failed++; console.log('FAIL', what); } };
const throws = (fn, what) => { try { fn(); check(false, what + ' (no error)'); } catch { check(true, what); } };
const same = (a, b) => JSON.stringify(a) === JSON.stringify(b);

// CRC-32 check value.
check(B.crc32(Uint8Array.from('123456789', c => c.charCodeAt(0))) === 0xcbf43926, 'crc32 check value');

// A song: DX7, 303, 808, PSG, pluck, sampler, DX7, 303, as the test song has.
const engines = [0, 1, 3, 4, 5, 0, 1, 2];
function song() {
  const sounds = new Uint8Array(1999);
  sounds[0] = 1;
  engines.forEach((e, t) => { sounds[1 + t * 223] = e; sounds[1 + t * 223 + 2] = t; });
  const tracks = Array.from({ length: 8 }, () => []);
  const note = (t, clock, pitch, velocity, length, glitch = 0) => tracks[t].push({ at: (clock | glitch << 26) >>> 0, a: pitch, b: velocity, length });
  const cc = (t, clock, number, value) => tracks[t].push(S.event(clock, S.KIND.CONTROL, number, value, 0));
  const pc = (t, clock, program) => tracks[t].push(S.event(clock, S.KIND.PROGRAM, program, 0, 0));
  cc(0, 0, 7, 100); note(0, 0, 60, 100, 480); note(0, 0, 64, 90, 480); note(0, 0, 67, 80, 480, 2);
  note(0, 240, 60, 70, 960); pc(0, 1920, 5); note(0, 1920, 72, 127, 1);
  note(1, 0, 36, 100, 120); cc(1, 120, 74, 30); cc(1, 240, 74, 90); note(1, 240, 36, 100, 120);
  for (let i = 0; i < 16; i++) note(2, i * 240, 36 + (i % 3), 100, 60);
  cc(5, 0, 91, 40); note(5, 3840, 48, 50, 4000);
  tracks[0].sort((a, b) => (a.at & 0xffffff) - (b.at & 0xffffff));
  tracks[0].push({ at: (5760 | 3 << 24) >>> 0, a: 1, b: 2, length: 480 }); // an FX lane stutter
  return { sounds, name: S.nameField('TEST SONG'), kit: 2, programs: [1, 2, 0, 0, 0, 0, 7, 0], bars: 6,
    tempos: [{ clock: 0, bpm100: 12000 }, { clock: 3840, bpm100: 9050 }, { clock: 5000, bpm100: 17333 }],
    meters: [{ bar: 0, beats: 4, unit: 4 }, { bar: 2, beats: 7, unit: 8 }, { bar: 4, beats: 3, unit: 4 }],
    markers: [{ clock: 0, name: S.markerField('INTRO') }, { clock: 3840, name: S.markerField('BREAK') }],
    loopStart: 0, loopEnd: 3840, loopOn: 1, settings: Uint8Array.of(25, 1, 0xfd, 9, 2, 1, 1, 1, 7, 80),
    knobs: new Uint8Array(32), glitch: [{ index: 2, word: 0x12345 }], humanizeTime: Uint8Array.of(0, 0, 5, 0, 0, 0, 0, 0),
    humanizeVelocity: new Uint8Array(8), seed: 42, tracks };
}

// Song payload and file: encode, decode, encode is byte for byte.
const payload = S.encode(song());
check(B.same(S.encode(S.decode(payload)), payload), 'song payload decodes and encodes to the same bytes');
const file = S.wrap(payload);
check(file.length === payload.length + 16 && B.same(S.unwrap(file), payload), 'song file wraps and unwraps');
const damaged = file.slice(); damaged[40] ^= 1;
throws(() => S.unwrap(damaged), 'a damaged song file is refused');
const badTempo = song(); badTempo.tempos[1].bpm100 = 40000;
throws(() => S.decode(S.encode(badTempo)), 'a tempo out of range is refused');
const badMeter = song(); badMeter.meters[0].bar = 1;
throws(() => S.decode(S.encode(badMeter)), 'a first time signature off bar 1 is refused');
check(S.barClock(song(), 2) === 3840 && S.barClock(song(), 4) === 3840 + 2 * 1680 && S.barClock(song(), 6) === 7200 + 2 * 1440, 'bar clocks follow the meters');

// MIDI file writer and reader: delta times, running status, note-off as velocity 0.
const lengths = [0, 127, 128, 16383, 16384, 0x0fffffff];
const written = M.write({ division: 96, tracks: [lengths.map((d, i) => ({ tick: lengths.slice(0, i + 1).reduce((a, b) => a + b, 0), data: [0x90, 60, 100] }))] });
const back = M.read(written);
check(back.division === 96 && back.tracks[0].filter(e => e.status === 0x90).map(e => e.tick).join() ===
  lengths.map((d, i) => lengths.slice(0, i + 1).reduce((a, b) => a + b, 0)).join(), 'delta times up to 28 bits');
const raw = Uint8Array.of(...B.field('MThd', 4), 0, 0, 0, 6, 0, 0, 0, 1, 1, 224, ...B.field('MTrk', 4), 0, 0, 0, 17,
  0, 0x91, 60, 100, 10, 62, 90, 20, 60, 0, 0, 62, 0, 0, 0xff, 0x2f, 0);
const runs = M.read(raw).tracks[0];
check(runs.length === 5 && runs[1].channel === 1 && runs[1].tick === 10 && runs[2].tick === 30 && runs[2].data[1] === 0, 'running status');
throws(() => M.read(raw.slice(0, 20)), 'a cut-short MIDI file is refused');

// Song to MIDI and back: notes, controllers, programs, tempo, meters, markers.
const original = song();
const exported = SM.exportMidi(original);
check(exported.skipped.fx === 1 && exported.skipped.glitched === 1 && exported.skipped.humanize, 'export reports what it leaves out');
const parsed = M.read(exported.bytes);
check(parsed.format === 1 && parsed.division === 480 && parsed.tracks.length === 9, 'format 1, 480 PPQ, conductor and 8 tracks');
const list = SM.sources(parsed);
const mapping = SM.defaultMapping(list);
check(same(mapping.map(id => (id === null ? null : list[id].track)), [1, 2, 3, null, null, 6, null, null]), 'an exported song maps back track for track');
const imported = SM.importMidi(parsed, S.decode(payload), mapping);
check(imported.report.ok && imported.song, 'the export imports');
const key = e => [S.clockOf(e), S.kindOf(e), e.a, e.b, e.length].join(':');
const content = sg => sg.tracks.map(list => list.filter(e => S.kindOf(e) !== S.KIND.FX).map(key).sort());
check(same(content(imported.song), content(original)), 'same notes, controllers and program changes');
check(same(imported.song.tempos, original.tempos) && same(imported.song.meters, original.meters), 'same tempo and meters');
check(same(imported.song.markers.map(m => [m.clock, S.markerName(m)]), original.markers.map(m => [m.clock, S.markerName(m)])), 'same markers');
check(imported.song.bars === original.bars && S.name(imported.song) === 'TEST SONG', 'same length and name');
check(B.same(S.decode(S.encode(imported.song)).sounds, original.sounds), 'the imported song encodes and keeps its sounds');

// Import: controllers an engine doesn't take are left out, and a file over capacity is reported, not cut.
const busy = [[{ tick: 0, meta: M.META.NAME, data: M.ascii('BIG') }]];
const part = [];
for (let i = 0; i < 5000; i++) part.push({ tick: i * 10, data: [0x90, 60, 100] }, { tick: i * 10 + 5, data: [0x80, 60, 0] });
part.push({ tick: 0, data: [0xb0, 33, 10] }, { tick: 0, data: [0xb0, 7, 10] });
part.sort((a, b) => a.tick - b.tick);
busy.push(part);
const big = SM.importMidi(M.read(M.write({ division: 480, tracks: busy })), S.decode(payload), [0, null, null, null, null, null, null, null]);
check(!big.report.ok && !big.song && big.report.counts.events === 5001 && big.report.problems.some(p => p.startsWith('events')), 'capacity reported before importing');
check(big.report.dropped.controllers === 1, 'a controller the engine does not take is left out');
const odd = M.write({ division: 96, tracks: [[{ tick: 0, meta: M.META.METER, data: Uint8Array.of(3, 1, 24, 8) },
  { tick: 0, meta: M.META.TEMPO, data: M.tempo(500000) }, { tick: 0, data: [0x90, 60, 100] }, { tick: 96 * 6, data: [0x80, 60, 0] }]] });
const oddSong = SM.importMidi(M.read(odd), S.decode(payload), [0, null, null, null, null, null, null, null]).song;
check(same(oddSong.meters, [{ bar: 0, beats: 6, unit: 4 }]) && oddSong.tracks[0][0].length === 480 * 6 && oddSong.bars === 1, '3/2 becomes 6/4, ticks rescale to 480');

// Kits: a bank of fake SEA5 chunks through the bank and file codecs.
const chunk = n => { const d = new Uint8Array(n * 228); for (let i = 0; i < n; i++) d.set([1, 0x45, 20, 0x5a, i, 7], i * 228); return d; };
const padsIn = Array.from({ length: 8 }, (_, i) => (i === 5 ? { name: '', frames: 0, data: new Uint8Array(0) } : { name: 'PAD ' + i, frames: 320 * (i + 1) - 7, data: chunk(i + 1) }));
const bank = K.buildBank(padsIn);
check(same(K.readBank(bank).map(p => [p.name, p.frames, p.data.length]), padsIn.map(p => [p.name, p.frames, p.data.length])), 'bank encodes and decodes');
const params = Uint8Array.from({ length: 48 }, (_, i) => i);
const kitFile = K.writeKit({ name: 'MY KIT', params, bank });
const kitBack = K.readKit(kitFile);
check(kitBack.name === 'MY KIT' && B.same(kitBack.params, params) && B.same(kitBack.bank, bank), 'kit file round trip');
check(B.same(K.writeKit({ name: 'MY KIT', params, bank }), kitFile) && !K.readKit(K.writeKit({ name: 'X', params: null, bank })).params, 'kit file without pad settings');
const kitBad = kitFile.slice(); kitBad[200] ^= 1;
throws(() => K.readKit(kitBad), 'a damaged kit file is refused');
const bankBad = bank.slice(); bankBad[600] ^= 1;
throws(() => K.readBank(bankBad), 'a damaged bank is refused');
check(K.sectors(4032) === 1 && K.sectors(4033) === 2, 'pool sectors per sample');

// Sample prep: WAV in and out, resampling, and each step of the chain.
const tone = Float32Array.from({ length: 44100 }, (_, i) => 0.5 * Math.sin(2 * Math.PI * 441 * i / 44100));
const wav16 = W.encode(tone, 44100), decoded = W.decode(wav16);
check(decoded.rate === 44100 && decoded.data.length === 44100 && Math.abs(decoded.data[25] - tone[25]) < 1e-4, '16-bit WAV in and out');
const stereo24 = new Uint8Array(44 + 4 * 6), v = new DataView(stereo24.buffer);
stereo24.set(B.field('RIFF', 4), 0); v.setUint32(4, 36 + 24, true); stereo24.set(B.field('WAVEfmt ', 8), 8);
v.setUint32(16, 16, true); v.setUint16(20, 1, true); v.setUint16(22, 2, true); v.setUint32(24, 8000, true);
v.setUint32(28, 48000, true); v.setUint16(32, 6, true); v.setUint16(34, 24, true); stereo24.set(B.field('data', 4), 36); v.setUint32(40, 24, true);
stereo24.set([0, 0, 0x40, 0, 0, 0xc0], 44); // left +0.5, right -0.5
stereo24.set([0, 0, 0x40, 0, 0, 0x40], 50); // both +0.5
const mono = W.decode(stereo24);
check(mono.rate === 8000 && mono.data.length === 4 && Math.abs(mono.data[0]) < 1e-6 && Math.abs(mono.data[1] - 0.5) < 1e-6, '24-bit stereo mixes to mono');
const at32 = W.resample(tone, 44100);
check(at32.length === 32000 && Math.abs(Math.max(...at32) - 0.5) < 0.01, 'resampled to 32 kHz keeps the level');
const plain = W.prepare({ rate: 32000, data: Float32Array.from({ length: 3200 }, (_, i) => i / 3200 - 0.5) });
const trimmed = W.prepare({ rate: 32000, data: plain }, { start: 0.01, end: 0.05 });
check(trimmed.length === 1280 && Math.abs(trimmed[0] - plain[320]) < 1e-6, 'trim');
const reversed = W.prepare({ rate: 32000, data: plain }, { reverse: true });
check(Math.abs(reversed[0] - plain[3199]) < 1e-6, 'reverse');
const faded = W.prepare({ rate: 32000, data: Float32Array.from({ length: 3200 }, () => 0.5) }, { fadeIn: 10, fadeOut: 10 });
check(faded[0] === 0 && faded[3199] === 0 && Math.abs(faded[160] - 0.25) < 1e-6 && faded[1600] === 0.5, 'fades');
const loud = W.prepare({ rate: 32000, data: plain }, { normalize: true });
check(Math.abs(Math.max(...loud.map(Math.abs)) - 0.966) < 1e-3, 'normalize to -0.3 dBFS');
const quiet = W.prepare({ rate: 32000, data: plain }, { gain: -6 });
check(Math.abs(quiet[0] / plain[0] - 10 ** (-6 / 20)) < 1e-4, 'gain');
const crushed = W.prepare({ rate: 32000, data: plain }, { bits: 2, hold: 8000 });
check(new Set(crushed).size <= 5 && crushed[0] === crushed[3] && crushed[4] !== undefined, 'bit depth and rate reduction');

console.log(`workbench_test: ${checks} checks, ${failed} failed`);
process.exit(failed ? 1 : 0);
