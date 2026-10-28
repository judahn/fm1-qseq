/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Song files (phase 5b, docs/file-formats.md). The payload is the song as
 * the device stores it (app/song/song_file.c), decoded field by field and
 * encoded back to the same bytes. A .fm1song file is a 16-byte header with
 * a CRC, then that payload, so device to file to device is byte for byte.
 * Version 2 (phase 7) adds the clip grid after the tracks: the launch
 * quantize, the row names and the clips. Version 3 lets the tracks hold
 * clip blocks (an FX event with bit 7 of `a` set, the row or 0xff for a
 * stop, and a 24-bit length, bits 16 to 23 in `b`). Version 4 (phase 8)
 * adds each track's chord settings and chord type at the end. Older
 * versions still read and write back as themselves. */
(function (root) {
  'use strict';
  const ns = root.FM1SongWorkbench = root.FM1SongWorkbench || {};
  const { check, crc32, Reader, Writer, le32, text, field } = ns.bytes;

  const LIMITS = { tracks: 8, events: 4096, tempos: 32, meters: 32, markers: 16, bars: 999, clock: 0xffffff,
    tempoMin: 2000, tempoMax: 30000, sounds: 1999, preset: 223, rows: 16, cells: 128 };
  const VERSION = 4;
  const KIND = { NOTE: 0, CONTROL: 1, PROGRAM: 2, FX: 3 };
  const ENGINES = ['DX7', '303', '808', 'PSG', 'PLUCK', 'SAMPLER'];
  const MAGIC = 0x47534d46; /* "FMSG" */

  const clockOf = e => e.at & 0xffffff;
  const kindOf = e => (e.at >>> 24) & 3;
  const glitchOf = e => (e.at >>> 26) & 31;
  const event = (clock, kind, a, b, length) => ({ at: (clock | kind << 24) >>> 0, a, b, length });
  const isBlock = e => kindOf(e) === KIND.FX && (e.a & 0x80) !== 0;
  const blockLength = e => e.length + e.b * 65536;
  const block = (clock, length, row) => event(clock, KIND.FX, row < 16 ? 0x80 | row : 0xff, length >>> 16, length & 0xffff);

  /* A part's events: a track's or a clip's, in clock order. Blocks only on
   * a track, from version 3. */
  function events(r, what, blocks) {
    const count = r.u16(), list = [];
    for (let i = 0, last = 0; i < count; i++) {
      const e = { at: r.u32(), a: r.u8(), b: r.u8(), length: r.u16() };
      check(clockOf(e) >= last && (kindOf(e) !== KIND.NOTE || e.length > 0) && (kindOf(e) === KIND.FX || (e.a <= 127 && e.b <= 127)),
        'Bad event on ' + what);
      check(!isBlock(e) || (blocks && blockLength(e) > 0 && (e.a === 0xff || e.a < 0x90)), 'Bad clip block on ' + what);
      last = clockOf(e);
      list.push(e);
    }
    return list;
  }
  function putEvents(w, list) {
    w.u16(list.length);
    for (const e of list) w.u32(e.at).u8(e.a).u8(e.b).u16(e.length);
  }
  /* The payload, checked as the device checks it. */
  function decode(bytes) {
    const r = new Reader(bytes, 'Song data'), version = r.u8();
    check(version >= 1 && version <= VERSION, 'Unknown song data version');
    const soundsBytes = r.u16();
    const song = { version, sounds: r.take(soundsBytes).slice() };
    check(soundsBytes === LIMITS.sounds && song.sounds[0] === 1, 'The sound setups block is not one this firmware knows');
    song.name = r.take(12).slice();
    song.kit = r.u16();
    song.programs = Array.from(r.take(8));
    song.bars = r.u16();
    check(song.bars >= 1 && song.bars <= LIMITS.bars, 'Bar count out of range');
    const tempos = r.u8();
    check(tempos >= 1 && tempos <= LIMITS.tempos, 'Tempo list out of range');
    song.tempos = [];
    for (let k = 0; k < tempos; k++) {
      const clock = r.u32(), bpm100 = r.u16();
      check(bpm100 >= LIMITS.tempoMin && bpm100 <= LIMITS.tempoMax && (k ? clock > song.tempos[k - 1].clock : clock === 0), 'Bad tempo change');
      song.tempos.push({ clock, bpm100 });
    }
    const meters = r.u8();
    check(meters >= 1 && meters <= LIMITS.meters, 'Time signature list out of range');
    song.meters = [];
    for (let m = 0; m < meters; m++) {
      const bar = r.u16(), beats = r.u8(), unit = r.u8();
      check(beats >= 1 && beats <= 16 && [4, 8, 16].includes(unit) && (m ? bar > song.meters[m - 1].bar : bar === 0), 'Bad time signature');
      song.meters.push({ bar, beats, unit });
    }
    const markers = r.u8();
    check(markers <= LIMITS.markers, 'Too many markers');
    song.markers = [];
    for (let i = 0; i < markers; i++) {
      const clock = r.u32();
      check(clock <= LIMITS.clock && (!i || clock >= song.markers[i - 1].clock), 'Bad marker');
      song.markers.push({ clock, name: r.take(8).slice() });
    }
    song.loopStart = r.u32(); song.loopEnd = r.u32(); song.loopOn = r.u8();
    song.settings = r.take(10).slice();
    song.knobs = r.take(32).slice();
    const s = song.settings;
    check(s[0] <= 50 && s[3] <= 11 && s[4] < 13 && s[5] <= 4 && s[6] <= 2 && s[7] <= 2 && s[8] <= 14 && s[9] <= 100 &&
      song.loopStart <= song.loopEnd, 'Song settings out of range');
    song.glitch = [];
    for (let n = r.u8(), i = 0; i < n; i++) {
      const index = r.u8(), word = r.u32();
      check(index >= 1 && index <= 31, 'Bad glitch entry');
      song.glitch.push({ index, word });
    }
    song.humanizeTime = r.take(8).slice();
    song.humanizeVelocity = r.take(8).slice();
    song.seed = r.u32();
    song.tracks = [];
    for (let t = 0; t < LIMITS.tracks; t++) song.tracks.push(events(r, 'track ' + (t + 1), version > 2));
    song.launchQ = 2;
    song.rows = [];
    song.clips = [];
    if (version > 1) {
      song.launchQ = r.u8();
      check(song.launchQ <= 4, 'Launch quantize out of range');
      for (let n = r.u8(), i = 0; i < n; i++) {
        const row = r.u8();
        check(row < LIMITS.rows, 'Bad row name');
        song.rows.push({ row, name: r.take(8).slice() });
      }
      for (let n = r.u8(), i = 0; i < n; i++) {
        const cell = r.u8(), length = r.u16();
        check(cell < LIMITS.cells && length > 0 && (!i || cell > song.clips[i - 1].cell), 'Bad clip');
        song.clips.push({ cell, length, events: events(r, 'track ' + ((cell >> 4) + 1) + ', row ' + ((cell & 15) + 1), false) });
      }
    }
    song.chords = new Uint8Array(8);
    song.chordTypes = new Uint8Array(8);
    if (version > 3) {
      song.chords = r.take(8).slice();
      song.chordTypes = r.take(8).slice();
      check(song.chords.every(c => (c >> 1 & 3) < 3 && (c >> 3 & 7) < 5) && song.chordTypes.every(t => t < 12), 'Chord settings out of range');
    }
    check(eventCount(song) <= LIMITS.events, 'More events than a song holds');
    check(!r.left, 'Song data has extra bytes');
    return song;
  }
  function encode(song) {
    const version = song.version || VERSION, clips = song.clips || [], rows = song.rows || [];
    const w = new Writer(4096 + 16 * rows.length + 5 * clips.length + 8 * eventCount(song));
    w.u8(version).u16(song.sounds.length).put(song.sounds).put(song.name).u16(song.kit).put(Uint8Array.from(song.programs)).u16(song.bars);
    w.u8(song.tempos.length);
    for (const t of song.tempos) w.u32(t.clock).u16(t.bpm100);
    w.u8(song.meters.length);
    for (const m of song.meters) w.u16(m.bar).u8(m.beats).u8(m.unit);
    w.u8(song.markers.length);
    for (const m of song.markers) w.u32(m.clock).put(m.name);
    w.u32(song.loopStart).u32(song.loopEnd).u8(song.loopOn).put(song.settings).put(song.knobs);
    w.u8(song.glitch.length);
    for (const g of song.glitch) w.u8(g.index).u32(g.word);
    w.put(song.humanizeTime).put(song.humanizeVelocity).u32(song.seed);
    for (const list of song.tracks) putEvents(w, list);
    if (version > 1) {
      w.u8(song.launchQ === undefined ? 2 : song.launchQ).u8(rows.length);
      for (const r of rows) w.u8(r.row).put(r.name);
      w.u8(clips.length);
      for (const c of clips) { w.u8(c.cell).u16(c.length); putEvents(w, c.events); }
    }
    if (version > 3) w.put(song.chords || new Uint8Array(8)).put(song.chordTypes || new Uint8Array(8));
    return w.done();
  }
  const eventCount = song => song.tracks.reduce((n, list) => n + list.length, 0) +
    (song.clips || []).reduce((n, c) => n + c.events.length, 0);

  /* The file around it: "FMSG", the payload's version, flags 0, payload length, payload CRC-32. */
  function wrap(payload) {
    return new Writer(16 + payload.length).u32(MAGIC).u16(payload[0]).u16(0).u32(payload.length).u32(crc32(payload)).put(payload).done();
  }
  function unwrap(file) {
    check(file instanceof Uint8Array && file.length >= 16 && le32(file, 0) === MAGIC, 'Not an FM-1 song file');
    check(file[4] >= 1 && file[4] <= VERSION && file[5] === 0, 'This song file needs a newer workbench');
    const length = le32(file, 8), payload = file.subarray(16);
    check(payload.length === length && crc32(payload) === le32(file, 12), 'The song file is damaged (CRC)');
    decode(payload);
    return payload.slice();
  }

  /* Bars to clocks: each bar is beats * (1920 / unit) clocks. */
  function barClock(song, bar) {
    let clock = 0;
    for (let i = 0; i < song.meters.length; i++) {
      const m = song.meters[i], to = i + 1 < song.meters.length ? song.meters[i + 1].bar : Infinity;
      if (bar <= to) return clock + (bar - m.bar) * m.beats * (1920 / m.unit);
      clock += (to - m.bar) * m.beats * (1920 / m.unit);
    }
    return clock;
  }
  const engineOf = (song, track) => song.sounds[1 + track * LIMITS.preset];
  const programOf = (song, track) => song.sounds[1 + track * LIMITS.preset + 2];

  ns.songFile = { LIMITS, KIND, ENGINES, VERSION, decode, encode, wrap, unwrap, barClock, engineOf, programOf, eventCount,
    clockOf, kindOf, glitchOf, event, isBlock, blockLength, block, name: song => text(song.name), nameField: value => field(value, 12, 32, true),
    markerName: m => text(m.name), markerField: value => field(value, 8, 0, true) };
  if (typeof module === 'object' && module.exports) module.exports = ns.songFile;
})(typeof globalThis !== 'undefined' ? globalThis : this);
