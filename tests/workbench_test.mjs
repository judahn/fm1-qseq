// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
// Node unit tests for the song workbench (web/workbench, phase 5b): the MIDI
// writer and reader, the song and kit file codecs (with phase 7's clips),
// songs to MIDI and back, sample prep, and the built-in kit and the factory
// DX7 voices against a fake device. No dependencies:
//   node tests/workbench_test.mjs
import { createRequire } from 'node:module';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const require = createRequire(import.meta.url);
const dir = path.join(path.dirname(fileURLToPath(import.meta.url)), '../web/workbench');
for (const name of ['bytes', 'song_file', 'song_clips', 'kit_file', 'midi_file', 'song_midi', 'wav', 'link', 'voices']) require(path.join(dir, name + '.js'));
const { bytes: B, songFile: S, songClips: SC, kitFile: K, midiFile: M, songMidi: SM, wav: W, link: L, voices: V } = globalThis.FM1SongWorkbench;

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

// Version 2 (phase 7): the clip grid. A song with clips and row names round trips, the file
// header carries the payload's version, and version 1 payloads and files still read and write
// back byte for byte, with an empty grid.
const withClips = song();
withClips.version = 2;
withClips.launchQ = 4;
withClips.rows = [{ row: 0, name: S.markerField('VERSE') }, { row: 15, name: S.markerField('OUTRO') }];
withClips.clips = [{ cell: 3, length: 1920, events: [S.event(0, S.KIND.NOTE, 60, 100, 240), S.event(960, S.KIND.CONTROL, 74, 9, 0)] },
  { cell: 127, length: 960, events: [] }];
const payload2 = S.encode(withClips), back2 = S.decode(payload2);
check(payload2[0] === 2 && B.same(S.encode(back2), payload2) && back2.launchQ === 4 && back2.clips.length === 2 &&
  back2.clips[0].events.length === 2 && back2.rows[1].row === 15 && S.markerName(back2.rows[0]) === 'VERSE', 'clips and row names round trip');
check(S.eventCount(back2) === S.eventCount(song()) + 2, 'clip events count toward the song');
const file2 = S.wrap(payload2);
check(file2[4] === 2 && B.same(S.unwrap(file2), payload2), 'a version 2 file');
const v1 = song(); v1.version = 1;
const payload1 = S.encode(v1);
check(payload1[0] === 1 && payload1.length === payload.length - 3 - 24 && B.same(S.encode(S.decode(payload1)), payload1), 'version 1 writes back as version 1');
const back1 = S.decode(payload1);
check(back1.clips.length === 0 && back1.rows.length === 0 && back1.launchQ === 2, 'version 1 reads with an empty grid');
const file1 = S.wrap(payload1);
check(file1[4] === 1 && B.same(S.unwrap(file1), payload1), 'a version 1 file still opens');
const outOfOrder = song(); outOfOrder.clips = [{ cell: 9, length: 480, events: [] }, { cell: 2, length: 480, events: [] }];
throws(() => S.decode(S.encode(outOfOrder)), 'clips out of cell order are refused');
const noLength = song(); noLength.clips = [{ cell: 9, length: 0, events: [] }];
throws(() => S.decode(S.encode(noLength)), 'a clip with no length is refused');
const tooMany = song(); tooMany.clips = [{ cell: 0, length: 480, events: Array.from({ length: 4096 }, (_, i) => S.event(i, S.KIND.NOTE, 60, 1, 1)) }];
throws(() => S.decode(S.encode(tooMany)), 'clips over the event pool are refused');
const future = payload2.slice(); future[0] = S.VERSION + 1;
throws(() => S.decode(future), 'a newer version is refused');
check(SM.exportMidi(withClips).skipped.clips === 2, 'MIDI export reports the clips it leaves out');

// Version 3 (phase 7b): clip blocks on the tracks round trip, and are refused in a version 2
// payload or inside a clip. Expand (song_clips.js) makes what the device's Expand job does: the
// clip pass after pass from the block's start, cut off at its end, the track's own events inside
// gone, and the controllers set at its edges. MIDI export expands every block.
const byClock = (a, b) => S.clockOf(a) - S.clockOf(b);
const withBlocks = song();
withBlocks.clips = [{ cell: 2, length: 960, events: [S.event(0, S.KIND.NOTE, 72, 100, 100), S.event(240, S.KIND.CONTROL, 74, 50, 0),
  S.event(480, S.KIND.NOTE, 74, 100, 700), S.event(1200, S.KIND.NOTE, 99, 100, 10)] }];
withBlocks.tracks[0] = withBlocks.tracks[0].concat([S.block(1920, 2400, 2)]).sort(byClock);
withBlocks.tracks[3] = [S.event(0, S.KIND.NOTE, 40, 100, 3000), S.block(960, 960, 15), S.block(2880, 100000, 16)];
const payload3 = S.encode(withBlocks), back3 = S.decode(payload3);
check(payload3[0] === S.VERSION && B.same(S.encode(back3), payload3) && back3.tracks[0].filter(S.isBlock).length === 1 &&
  S.blockLength(back3.tracks[3][2]) === 100000 && back3.tracks[3][2].a === 0xff, 'the blocks round trip, long ones and stops');
const asThree = S.encode({ ...withBlocks, version: 3 });
check(asThree[0] === 3 && asThree.length === payload3.length - 24 && B.same(S.encode(S.decode(asThree)), asThree), 'version 3 still reads and writes back as itself');
// Version 4 (phase 8): each track's chord settings and type, at the end. Version 3 reads with
// chord mode off. Settings out of range are refused.
const withChords = song();
withChords.chords = Uint8Array.of(1, 1 | 1 << 1 | 4 << 3 | 2 << 6, 0, 2 << 1 | 3 << 6, 0, 0, 0, 1 | 3 << 3);
withChords.chordTypes = Uint8Array.of(0, 11, 0, 5, 0, 0, 0, 2);
const payload4 = S.encode(withChords), back4 = S.decode(payload4);
check(payload4[0] === S.VERSION && B.same(back4.chords, withChords.chords) && B.same(back4.chordTypes, withChords.chordTypes) &&
  B.same(S.encode(back4), payload4), 'chord settings round trip');
const chordsOff = S.decode(S.encode({ ...withChords, version: 3 }));
check(chordsOff.chords.every(c => !c) && chordsOff.chordTypes.every(t => !t), 'version 3 reads with chord mode off');
for (const [what, chords, types] of [['a voicing past smooth', Uint8Array.of(3 << 1, 0, 0, 0, 0, 0, 0, 0), new Uint8Array(8)],
  ['a strum past an 8th', Uint8Array.of(5 << 3, 0, 0, 0, 0, 0, 0, 0), new Uint8Array(8)], ['a type past add9', new Uint8Array(8), Uint8Array.of(0, 12, 0, 0, 0, 0, 0, 0)]])
  throws(() => S.decode(S.encode({ ...withChords, chords, chordTypes: types })), 'chord settings refused: ' + what);
throws(() => S.decode(Uint8Array.from([...payload4.slice(0, payload4.length - 16), 0])), 'a payload cut short is refused');
const asFour = S.encode({ ...withChords, version: 4 }), backFour = S.decode(asFour);
check(asFour[0] === 4 && asFour.length === payload4.length - 8 && B.same(S.encode(backFour), asFour) && B.same(backFour.chords, withChords.chords),
  'version 4 still reads and writes back as itself');
// Version 5 (0.6.6): each track's chromatic setting, after the chords: on in bit 3, the pad in bits 0 to 2.
// Version 4 reads with it off, a value past 15 is refused, and a note's TUNE (glitch word bits 20 to 25)
// travels with the glitch table as any attribute does. MIDI export counts a tuned note among the glitched.
check(backFour.chromatic.every(c => !c), 'version 4 reads with chromatic mode off');
const withChromatic = song();
withChromatic.chromatic = Uint8Array.of(0, 0, 0, 0, 8 | 3, 0, 5, 0);
withChromatic.glitch = [{ index: 2, word: 0x12345 }, { index: 3, word: (-5 & 63) << 20 }];
withChromatic.tracks[5] = [{ at: (0 | 3 << 26) >>> 0, a: 36, b: 100, length: 120 }, ...withChromatic.tracks[5]];
const payload5 = S.encode(withChromatic), back5 = S.decode(payload5);
check(payload5[0] === 5 && payload5.length === payload4.length + 5 + 8 && B.same(back5.chromatic, withChromatic.chromatic) &&
  B.same(S.encode(back5), payload5), 'chromatic settings round trip');
check(back5.glitch[1].word === (-5 & 63) << 20 && S.glitchOf(back5.tracks[5][0]) === 3, 'a tuned note keeps its TUNE word');
check(SM.exportMidi(back5).skipped.glitched === SM.exportMidi(withChords).skipped.glitched + 1, 'MIDI export leaves TUNE out, counted as glitch');
throws(() => S.decode(S.encode({ ...withChromatic, chromatic: Uint8Array.of(16, 0, 0, 0, 0, 0, 0, 0) })), 'a chromatic setting past 15 is refused');
const badCc = song(); badCc.tracks[2] = [S.event(0, S.KIND.CONTROL, 200, 5, 0)];
throws(() => S.decode(S.encode(badCc)), 'a controller number past 127 is refused');
const badVelocity = song(); badVelocity.tracks[2] = [S.event(0, S.KIND.NOTE, 60, 130, 10)];
throws(() => S.decode(S.encode(badVelocity)), 'a velocity past 127 is refused');
throws(() => S.decode(S.encode({ ...withBlocks, version: 2 })), 'a block in a version 2 payload is refused');
const blockInClip = song(); blockInClip.clips = [{ cell: 1, length: 480, events: [S.block(0, 480, 0)] }];
throws(() => S.decode(S.encode(blockInClip)), 'a block inside a clip is refused');
const badRow = song(); badRow.tracks[1] = [{ ...S.block(0, 480, 0), a: 0x90 }];
throws(() => S.decode(S.encode(badRow)), 'a block past the grid is refused');
const expanded = SC.expand(withBlocks), t0 = expanded.tracks[0], at = (list, pitch) => list.filter(e => S.kindOf(e) === S.KIND.NOTE && e.a === pitch).map(S.clockOf);
check(!expanded.tracks.flat().some(S.isBlock), 'expand leaves no blocks');
check(same(at(t0, 72), [1920, 2880, 3840]) && same(at(t0, 74), [2400, 3360]) && !at(t0, 99).length, 'expand: the clip pass by pass, cut at the end');
check(!t0.some(e => S.kindOf(e) === S.KIND.PROGRAM && S.clockOf(e) === 1920), 'expand: the track\'s own events inside go');
check(same(t0.filter(e => S.kindOf(e) === S.KIND.CONTROL && e.a === 74).map(e => [S.clockOf(e), e.b]), [[1920, 50], [2160, 50], [3120, 50], [4080, 50]]),
  'expand: the clip\'s controller from its start, then each pass');
check(t0.some(e => S.kindOf(e) === S.KIND.CONTROL && e.a === 7 && e.b === 100 && S.clockOf(e) === 4320), 'expand: the track\'s controller back at the end');
check(t0.some(e => S.kindOf(e) === S.KIND.FX), 'expand: the FX lane stays');
const t3 = expanded.tracks[3];
check(t3.length === 1 && t3[0].length === 960, 'expand: a note sounding into a block ends where it starts, a stop leaves nothing');
const blockMidi = SM.exportMidi(withBlocks), midiBack = M.read(blockMidi.bytes);
const onsAt = (track, pitch) => midiBack.tracks[track].filter(e => e.status === 0x90 && e.data[0] === pitch && e.data[1]).map(e => e.tick);
check(blockMidi.skipped.blocks === 3 && same(onsAt(1, 74), [2400, 3360]) && same(onsAt(1, 72), [1920, 2880, 3840]), 'MIDI export expands the blocks');
check(same(SC.list(withClips).map(c => [c.cell, c.track, c.row, c.length, c.events, c.name]), [[3, 0, 3, 1920, 2, ''], [127, 7, 15, 960, 0, 'OUTRO']]),
  'the clips of a song, listed');
// A clip copied into another song: replacing the cell there, glitch moved to that song's table.
const source = song();
source.glitch = [{ index: 2, word: 0x123 }];
source.clips = [{ cell: 5, length: 480, events: [S.event(0, S.KIND.NOTE, 60, 100, 100), { ...S.event(240, S.KIND.NOTE, 62, 100, 100), at: (240 | 2 << 26) >>> 0 }] }];
const target = song();
target.glitch = [{ index: 1, word: 0x77 }, { index: 5, word: 0x123 }];
target.clips = [{ cell: 9, length: 960, events: [] }];
const copied = SC.copy(source, 5, target, 9);
const clip9 = copied.song.clips.find(c => c.cell === 9);
check(copied.song.clips.length === 1 && clip9.length === 480 && clip9.events.length === 2 && S.glitchOf(clip9.events[1]) === 5 &&
  !copied.dropped, 'a clip copied into another song, its glitch found there');
check(B.same(S.encode(S.decode(S.encode(copied.song))), S.encode(copied.song)), 'the song with the copied clip saves');
const full = song(); full.glitch = Array.from({ length: 31 }, (_, i) => ({ index: i + 1, word: 0x1000 + i }));
const crowded = SC.copy(source, 5, full, 0);
check(crowded.dropped === 1 && S.glitchOf(crowded.song.clips[0].events[1]) === 0, 'a full glitch table: that note plays plain');
const added = SC.copy(source, 5, song(), 33);
const entry = added.song.glitch.find(g => g.word === 0x123);
check(added.song.glitch.length === 2 && entry && S.glitchOf(added.song.clips[0].events[1]) === entry.index, 'a new glitch entry when it isn\'t there');
throws(() => SC.copy(source, 6, target, 0), 'no clip there, nothing copied');

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

// The built-in kit: FM1 Open's 8 sounds by name. Before sending it, the device is asked
// whether a kit already has those samples, pad for pad, and the kits' pads are read only
// when every sample is there. The kit table is kept twice: the first kit needs two sectors.
const builtinPads = K.BUILT_IN.map((name, i) => ({ name, frames: 320 * (i + 2) - 3, data: chunk(i + 2) }));
const builtinBank = K.buildBank(builtinPads), builtin = K.builtinKit(builtinBank);
check(builtin.name === 'BUILT-IN' && !builtin.params && B.same(builtin.bank, builtinBank) && builtin.pads.length === 8, 'built-in kit from the 8 sounds');
throws(() => K.builtinKit(bank), 'a bank that is not the 8 built-in sounds is refused');
const swapped = builtinPads.slice(); [swapped[0], swapped[1]] = [swapped[1], swapped[0]];
throws(() => K.builtinKit(K.buildBank(swapped)), 'the built-in sounds out of order are refused');
const le16b = v => [v & 255, v >> 8 & 255], le32b = v => [v & 255, v >>> 8 & 255, v >>> 16 & 255, v >>> 24];
function fakeDevice(samples, kits) {
  const sent = [], none = { status: 1, data: new Uint8Array(0) };
  const reply = (command, p) => {
    sent.push(command);
    const id = p[0] | p[1] << 8, kit = kits.find(k => k.id === id), s = samples[p[0]], k = kits[p[0]];
    if (command === 48) return s ? { status: 0, data: Uint8Array.from([...le16b(s.id), ...le32b(s.data.length), ...le32b(s.frames),
      ...le32b(B.crc32(s.data)), 1, ...B.field(s.name, 16)]) } : none;
    if (command === 41) return k ? { status: 0, data: Uint8Array.from([...le16b(k.id), ...B.field(k.name, 16), 255, 0, ...le32b(1000)]) } : none;
    if (command === 42) return kit ? { status: 0, data: Uint8Array.from([...le32b(1000), ...le32b(0), ...B.field(kit.name, 16), 0,
      ...kit.pads.flatMap(le16b)]) } : none;
    return { status: 0, data: new Uint8Array(0) };
  };
  return { sent, link: new L.Link(async requests => requests.map(([command, payload]) => reply(command, payload))) };
}
{
  const stored = builtinPads.map((p, i) => ({ ...p, id: 3 + i })), legacy = builtinPads.map((p, i) => ({ ...p, id: 0xf000 + i }));
  const empty = fakeDevice([], [{ id: 1, name: 'MINE', pads: [1, 0, 0, 0, 0, 0, 0, 0] }]);
  check(await empty.link.findKit(builtin.pads) === null && !empty.sent.includes(42), 'built-in kit: no samples there, no kit read');
  const most = fakeDevice(stored.slice(1), [{ id: 1, name: 'MINE', pads: stored.map(s => s.id) }]);
  check(await most.link.findKit(builtin.pads) === null && !most.sent.includes(42), 'built-in kit: 7 of 8 samples there, no kit read');
  const order = stored.map(s => s.id); [order[0], order[1]] = [order[1], order[0]];
  const both = fakeDevice([...stored, ...legacy], [{ id: 2, name: 'OTHER', pads: order }, { id: 4, name: 'STOCK BANK', pads: legacy.map(s => s.id) }]);
  const found = await both.link.findKit(builtin.pads);
  check(found && found.id === 4 && found.name === 'STOCK BANK', 'built-in kit: found as the stock bank, not as the same samples on other pads');
  check(both.sent.filter(c => c === 42).length === 2 && both.sent.filter(c => c === 49).length === 2, 'built-in kit: each kit read is ended');
  const half = fakeDevice(stored, [{ id: 5, name: 'HALF', pads: [...stored.slice(0, 4).map(s => s.id), 0, 0, 0, 0] }]);
  check(await half.link.findKit(builtin.pads) === null, 'built-in kit: a kit with only some of them is not it');
  const per = builtinPads.reduce((n, p) => n + K.sectors(p.data.length), 0);
  const cost = await empty.link.kitCost(builtin.pads, 0), again = await both.link.kitCost(builtin.pads, 2);
  check(cost.sectors === 2 + per && cost.fresh === 8 && again.sectors === 1 && again.fresh === 0, 'kit cost: two table copies for the first kit, one after');
}

// The factory DX7 voices: kind 4 transfers (commands 1 to 6) as app/transfer.c
// runs them, over 4 banks: blank (never saved), voices of its own, and the
// factory's already. One question for the one bank of its own, every bank
// written, each transfer ended. Declined, nothing is written.
function dx7Device(banks) {
  const sent = [];
  let open = null, at = 0, writing = false, index = 0;
  const ok = (data = []) => ({ status: 0, data: Uint8Array.from(data) });
  const reply = (command, p) => {
    sent.push(command);
    if (command === 1) { open = banks[p[1]].slice(); writing = false; return ok([0, 16]); }
    if (command === 2) return ok(open.subarray(p[0] | p[1] << 8, (p[0] | p[1] << 8) + p[2]));
    if (command === 3) { open = new Uint8Array(4096); at = 0; writing = true; index = p[1]; return ok(); }
    if (command === 4) { open.set(p.slice(2), p[0] | p[1] << 8); at += p.length - 2; return ok(); }
    if (command === 5) {
      const crc = (p[0] | p[1] << 8 | p[2] << 16 | p[3] << 24) >>> 0;
      if (!writing || at !== 4096 || crc !== B.crc32(open)) return { status: 71, data: new Uint8Array(0) };
      banks[index] = open; open = null; return ok();
    }
    if (command === 6) { open = null; return ok(); }
    return { status: 71, data: new Uint8Array(0) };
  };
  return { sent, banks, link: new L.Link(async requests => requests.map(([command, payload]) => reply(command, payload))) };
}
{
  const factory = [0, 1, 2, 3].map(b => Uint8Array.from({ length: 4096 }, (_, i) => (i * 3 + b * 17) % 100));
  const own = factory[2].map(v => (v + 1) % 100), asked = [];
  const device = dx7Device([new Uint8Array(4096), factory[1].slice(), own.slice(), factory[3].slice()]);
  const r = await V.sendFactory(device.link, factory, n => (asked.push(n), true));
  check(same(asked, [1]), 'factory voices: one question, for the bank of its own');
  check(device.banks.every((b, i) => same(Array.from(b), Array.from(factory[i]))), 'factory voices: every bank holds them');
  check(r.saved === 1 && r.fresh === 1, 'factory voices: one bank of its own, one blank');
  check(device.sent.filter(c => c === 3).length === 4 && device.sent.filter(c => c === 5).length === 4, 'factory voices: 4 banks written');
  check(device.sent.filter(c => c === 1).length === device.sent.filter(c => c === 6).length - 4, 'factory voices: every transfer ended');
  const declined = dx7Device([own.slice(), own.slice(), factory[2].slice(), new Uint8Array(4096)]), counts = [];
  const no = await V.sendFactory(declined.link, factory, n => (counts.push(n), false));
  check(no.cancelled && same(counts, [2]) && !declined.sent.includes(3), 'factory voices: declined, nothing written');
}

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
