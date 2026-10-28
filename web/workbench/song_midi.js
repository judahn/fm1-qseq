/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Songs to and from Standard MIDI Files (phase 5b, design 8b). Export is
 * format 1 at 480 PPQ: a conductor track (name, tempo, time signatures,
 * markers) and one track per FM-1 track on channels 1 to 8, with notes,
 * controllers and program changes, every clip block expanded into what it
 * plays (song_clips.js). Glitch attributes, the FX lane, humanize and the
 * clip grid stay in the song file and are left out of MIDI. Import maps the
 * file's tracks onto FM-1 tracks and reports what doesn't fit first. */
(function (root) {
  'use strict';
  const ns = root.FM1SongWorkbench = root.FM1SongWorkbench || {};
  const S = ns.songFile, M = ns.midiFile;
  const { KIND, LIMITS, ENGINES } = S;
  /* The controllers each engine takes (app/song/lanes.c), and program counts. */
  const CONTROLLERS = [[7, 10, 1, 91, 11, 2, 4], [74, 71, 1, 75, 7, 10, 11, 91, 5], [7, 10, 91, 11, 1],
    [7, 74, 72, 10, 11, 91, 1], [7, 74, 75, 72, 10, 11, 91, 1], [7, 10, 91, 11, 1]];
  const PROGRAMS = [128, 2, 0, 4, 0, 0];
  const engine = (song, t) => Math.min(S.engineOf(song, t), 5);
  const byTick = (a, b) => a.tick - b.tick || a.order - b.order || a.seq - b.seq;

  function exportMidi(song) {
    const skipped = { fx: 0, glitched: 0, humanize: song.humanizeTime.some(v => v) || song.humanizeVelocity.some(v => v),
      clips: (song.clips || []).length, blocks: ns.songClips.blocks(song) };
    song = ns.songClips.expand(song);
    let end = S.barClock(song, song.bars), seq = 0;
    const conductor = [{ tick: 0, order: 0, seq: seq++, meta: M.META.NAME, data: M.ascii(S.name(song)) }];
    for (const m of song.meters) conductor.push({ tick: S.barClock(song, m.bar), order: 1, seq: seq++, meta: M.META.METER, data: M.meter(m.beats, m.unit) });
    for (const t of song.tempos) conductor.push({ tick: t.clock, order: 2, seq: seq++, meta: M.META.TEMPO, data: M.tempo(Math.round(6e9 / t.bpm100)) });
    for (const m of song.markers) conductor.push({ tick: m.clock, order: 3, seq: seq++, meta: M.META.MARKER, data: M.ascii(S.markerName(m)) });
    const tracks = [conductor];
    for (let t = 0; t < LIMITS.tracks; t++) {
      const list = [{ tick: 0, order: 0, seq: seq++, meta: M.META.NAME, data: M.ascii('T' + (t + 1) + ' ' + (ENGINES[S.engineOf(song, t)] || '?')) }];
      const events = song.tracks[t], base = song.programs[t] - 1, count = PROGRAMS[engine(song, t)];
      if (base >= 0 && base < count && !events.some(e => S.kindOf(e) === KIND.PROGRAM && !S.clockOf(e)))
        list.push({ tick: 0, order: 1, seq: seq++, data: [0xc0 | t, base] });
      for (const e of events) {
        const clock = S.clockOf(e), kind = S.kindOf(e);
        if (kind === KIND.NOTE) {
          skipped.glitched += S.glitchOf(e) ? 1 : 0;
          list.push({ tick: clock, order: 2, seq: seq++, data: [0x90 | t, e.a, Math.max(1, e.b)] });
          list.push({ tick: clock + e.length, order: 0, seq: seq++, data: [0x80 | t, e.a, 0] });
          end = Math.max(end, clock + e.length);
        } else if (kind === KIND.CONTROL) list.push({ tick: clock, order: 1, seq: seq++, data: [0xb0 | t, e.a, e.b] });
        else if (kind === KIND.PROGRAM) list.push({ tick: clock, order: 1, seq: seq++, data: [0xc0 | t, e.a] });
        else skipped.fx++;
      }
      tracks.push(list);
    }
    for (const list of tracks) {
      list.sort(byTick);
      list.push({ tick: Math.max(end, list.length ? list[list.length - 1].tick : 0), meta: M.META.END, data: new Uint8Array(0) });
    }
    return { bytes: M.write({ division: 480, tracks }), skipped };
  }

  /* The file's playable parts: one per track and channel. */
  function sources(midi) {
    const out = [];
    midi.tracks.forEach((events, track) => {
      const name = events.find(e => e.meta === M.META.NAME);
      for (let channel = 0; channel < 16; channel++) {
        const mine = events.filter(e => e.status !== undefined && e.channel === channel);
        if (!mine.length) continue;
        out.push({ id: out.length, track, channel, name: name ? String.fromCharCode(...name.data).trim() : '',
          notes: mine.filter(e => e.status === 0x90 && e.data[1]).length, events: mine });
      }
    });
    return out;
  }

  /* A song from `base` (its sounds, kit and settings) with the file's timing
   * and the mapped parts. `mapping[t]` is a source id or null. */
  function importMidi(midi, base, mapping, name = '') {
    const problems = [], dropped = { controllers: 0, programs: 0, clamped: 0, moved: 0 };
    const clock = tick => Math.round(tick * 480 / midi.division);
    const all = midi.tracks.flat();
    const sizes = { [M.META.TEMPO]: 3, [M.META.METER]: 2 };
    const metas = kind => all.filter(e => e.meta === kind && e.data.length >= (sizes[kind] || 0)).sort((a, b) => a.tick - b.tick);
    const tempos = [];
    for (const e of metas(M.META.TEMPO)) {
      const us = (e.data[0] << 16) | (e.data[1] << 8) | e.data[2], at = clock(e.tick);
      let bpm100 = Math.round(6e9 / Math.max(us, 1));
      if (bpm100 < LIMITS.tempoMin || bpm100 > LIMITS.tempoMax) { dropped.clamped++; bpm100 = Math.min(LIMITS.tempoMax, Math.max(LIMITS.tempoMin, bpm100)); }
      if (tempos.length && tempos[tempos.length - 1].clock === at) tempos.pop();
      tempos.push({ clock: at, bpm100 });
    }
    if (!tempos.length || tempos[0].clock) tempos.unshift({ clock: 0, bpm100: 12000 });
    const song = { ...base, tempos, meters: [{ bar: 0, beats: 4, unit: 4 }], markers: [], glitch: [],
      humanizeTime: new Uint8Array(8), humanizeVelocity: new Uint8Array(8), tracks: [], rows: [], clips: [] };
    for (const e of metas(M.META.METER)) {
      let beats = e.data[0], unit = 2 ** e.data[1];
      while (unit < 4) { beats *= 2; unit *= 2; }
      while (unit > 16 && !(beats & 1)) { beats /= 2; unit /= 2; }
      if (beats < 1 || beats > 16 || unit > 16) { problems.push('Time signature ' + e.data[0] + '/' + 2 ** e.data[1] + ' left out'); continue; }
      const at = clock(e.tick);
      let bar = 0;
      while (S.barClock(song, bar) < at) bar++;
      if (S.barClock(song, bar) !== at) dropped.moved++;
      const last = song.meters[song.meters.length - 1];
      if (last.bar === bar) song.meters.pop();
      song.meters.push({ bar, beats, unit });
    }
    for (const e of metas(M.META.MARKER))
      song.markers.push({ clock: clock(e.tick), name: S.markerField(String.fromCharCode(...e.data).trim()) });
    const list = sources(midi);
    let end = Math.max(0, ...all.filter(e => e.meta === M.META.END).map(e => clock(e.tick)));
    for (let t = 0; t < LIMITS.tracks; t++) {
      const source = mapping[t] === null || mapping[t] === undefined ? null : list[mapping[t]];
      const events = [], open = new Map(), engineIndex = engine(song, t), last = new Map();
      for (const e of source ? source.events : []) {
        const at = clock(e.tick);
        if (e.status === 0x90 && e.data[1]) {
          if (!open.has(e.data[0])) open.set(e.data[0], []);
          open.get(e.data[0]).push({ at, velocity: e.data[1] });
        } else if (e.status === 0x80 || e.status === 0x90) {
          const on = open.get(e.data[0])?.shift();
          if (!on) continue;
          let length = at - on.at;
          if (length < 1 || length > 65535) { dropped.clamped++; length = Math.min(65535, Math.max(1, length)); }
          events.push({ ...S.event(on.at, KIND.NOTE, e.data[0], on.velocity, length), order: 1 });
          end = Math.max(end, on.at + length);
        } else if (e.status === 0xb0) {
          if (!CONTROLLERS[engineIndex].includes(e.data[0])) { dropped.controllers++; continue; }
          const same = events.findIndex(x => x.order === 0 && S.kindOf(x) === KIND.CONTROL && x.a === e.data[0] && S.clockOf(x) === at);
          if (same >= 0) events.splice(same, 1);
          else if (last.get(e.data[0]) === e.data[1]) continue; /* no change */
          last.set(e.data[0], e.data[1]);
          events.push({ ...S.event(at, KIND.CONTROL, e.data[0], e.data[1], 0), order: 0 });
        } else if (e.status === 0xc0) {
          if (e.data[0] >= PROGRAMS[engineIndex] || (!at && e.data[0] === song.programs[t] - 1)) { dropped.programs += e.data[0] >= PROGRAMS[engineIndex]; continue; }
          events.push({ ...S.event(at, KIND.PROGRAM, e.data[0], 0, 0), order: 0 });
        }
      }
      for (const [pitch, starts] of open) for (const on of starts) { /* never let go: held to the end */
        dropped.clamped++;
        events.push({ ...S.event(on.at, KIND.NOTE, pitch, on.velocity, Math.max(1, Math.min(65535, end - on.at))), order: 1 });
      }
      events.sort((a, b) => S.clockOf(a) - S.clockOf(b) || a.order - b.order);
      song.tracks.push(events.map(({ at, a, b, length }) => ({ at, a, b, length })));
    }
    song.bars = 1;
    while (S.barClock(song, song.bars) < end && song.bars < 10000) song.bars++;
    song.loopStart = 0; song.loopEnd = S.barClock(song, Math.min(song.bars, 4)); song.loopOn = 0;
    const title = midi.tracks[0].find(e => e.meta === M.META.NAME), titled = title && String.fromCharCode(...title.data).trim();
    if (titled || name) song.name = S.nameField(titled || name); /* the song's own name first, then the file's */
    const counts = { events: S.eventCount(song), tempos: song.tempos.length, meters: song.meters.length,
      markers: song.markers.length, bars: song.bars,
      clock: Math.max(end, ...song.tracks.flat().map(S.clockOf), ...song.markers.map(m => m.clock), ...song.tempos.map(t => t.clock)) };
    const limits = { events: LIMITS.events, tempos: LIMITS.tempos, meters: LIMITS.meters, markers: LIMITS.markers,
      bars: LIMITS.bars, clock: LIMITS.clock };
    for (const key of Object.keys(limits))
      if (counts[key] > limits[key]) problems.push(key + ': ' + counts[key] + ' of ' + limits[key]);
    const ok = Object.keys(limits).every(key => counts[key] <= limits[key]);
    const tracks = song.tracks.map((events, t) => ({ track: t + 1, source: mapping[t] ?? null, events: events.length,
      notes: events.filter(e => S.kindOf(e) === KIND.NOTE).length }));
    return { song: ok ? song : null, report: { ok, counts, limits, problems, dropped, tracks } };
  }
  /* Parts on channels 1 to 8, one each, go to the track of that number, as
   * an exported song's do. Otherwise the first 8 parts with notes, in order. */
  const defaultMapping = list => {
    const channels = new Set(list.map(s => s.channel));
    if (channels.size === list.length && list.every(s => s.channel < 8))
      return Array.from({ length: 8 }, (_, t) => { const s = list.find(s => s.channel === t); return s ? s.id : null; });
    const playable = list.filter(s => s.notes);
    return Array.from({ length: 8 }, (_, t) => (playable[t] ? playable[t].id : null));
  };

  ns.songMidi = { CONTROLLERS, PROGRAMS, exportMidi, importMidi, sources, defaultMapping };
  if (typeof module === 'object' && module.exports) module.exports = ns.songMidi;
})(typeof globalThis !== 'undefined' ? globalThis : this);
