/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Standard MIDI Files, written from the published format: an MThd chunk,
 * then MTrk chunks of delta-timed events. The reader takes format 0 and 1,
 * running status, SysEx and any meta event. The writer makes format 1. */
(function (root) {
  'use strict';
  const ns = root.FM1SongWorkbench = root.FM1SongWorkbench || {};
  const { check, Writer } = ns.bytes;
  const META = { NAME: 3, MARKER: 6, END: 0x2f, TEMPO: 0x51, METER: 0x58 };

  function vlq(w, value) {
    const groups = [value & 127];
    while ((value >>>= 7)) groups.unshift((value & 127) | 128);
    for (const g of groups) w.u8(g);
  }
  const be32 = (w, v) => w.u8(v >>> 24).u8(v >>> 16).u8(v >>> 8).u8(v);
  const ascii = s => Uint8Array.from(String(s), c => c.charCodeAt(0) & 127);

  /* tracks: lists of {tick, data} (status and data bytes) or {tick, meta, data}.
   * Events are written in list order, which the caller sorts. */
  function write({ division = 480, tracks }) {
    const w = new Writer(65536);
    w.put(ascii('MThd'));
    be32(w, 6);
    w.u8(0).u8(1).u8(tracks.length >> 8).u8(tracks.length).u8(division >> 8).u8(division);
    for (const list of tracks) {
      const body = new Writer(4096);
      let last = 0;
      for (const e of list) {
        check(e.tick >= last, 'MIDI events out of order');
        vlq(body, e.tick - last);
        last = e.tick;
        if (e.meta !== undefined) { body.u8(255).u8(e.meta); vlq(body, e.data.length); body.put(e.data); }
        else body.put(e.data);
      }
      const end = list.length ? list[list.length - 1] : null;
      if (!end || end.meta !== META.END) { vlq(body, 0); body.u8(255).u8(META.END).u8(0); }
      const bytes = body.done();
      w.put(ascii('MTrk'));
      be32(w, bytes.length);
      w.put(bytes);
    }
    return w.done();
  }

  /* Every track's events with absolute ticks: {tick, status, channel, data}
   * for channel messages, {tick, meta, data} for meta events. */
  function read(bytes) {
    check(bytes instanceof Uint8Array && bytes.length >= 14, 'Not a MIDI file');
    let at = 0;
    const u8 = () => { check(at < bytes.length, 'MIDI file is cut short'); return bytes[at++]; };
    const u32 = () => ((u8() << 24) | (u8() << 16) | (u8() << 8) | u8()) >>> 0;
    const id = () => String.fromCharCode(u8(), u8(), u8(), u8());
    const number = () => { let v = 0, b; do { b = u8(); v = v * 128 + (b & 127); } while (b & 128); return v; };
    check(id() === 'MThd', 'Not a MIDI file');
    const size = u32(), start = at;
    check(size >= 6, 'Not a MIDI file');
    const format = (u8() << 8) | u8(), count = (u8() << 8) | u8(), division = (u8() << 8) | u8();
    at = start + size;
    check(format <= 1, 'MIDI format 2 files are not supported');
    check(!(division & 0x8000) && division > 0, 'SMPTE-timed MIDI files are not supported');
    const tracks = [];
    while (at + 8 <= bytes.length && tracks.length < count) {
      const kind = id(), length = u32(), end = at + length;
      check(end <= bytes.length, 'MIDI file is cut short');
      if (kind !== 'MTrk') { at = end; continue; }
      const events = [];
      let tick = 0, running = 0;
      while (at < end) {
        tick += number();
        let status = bytes[at];
        if (status & 128) at++; else { check(running, 'MIDI data without a status'); status = running; }
        if (status === 255) {
          const meta = u8(), n = number();
          check(at + n <= end, 'A MIDI track runs past its end');
          events.push({ tick, meta, data: bytes.slice(at, at + n) });
          at += n;
          running = 0;
          if (meta === META.END) break;
        } else if (status === 0xf0 || status === 0xf7) {
          at += number();
          running = 0;
        } else {
          check(status < 0xf0, 'A MIDI track holds a system message');
          running = status;
          const n = (status & 0xe0) === 0xc0 ? 1 : 2;
          const data = [];
          for (let i = 0; i < n; i++) data.push(u8() & 127);
          events.push({ tick, status: status & 0xf0, channel: status & 15, data });
        }
      }
      check(at <= end, 'A MIDI track runs past its end');
      at = end;
      tracks.push(events);
    }
    check(tracks.length, 'The MIDI file has no tracks');
    return { format, division, tracks };
  }

  /* Meta event payloads. */
  const tempo = us => Uint8Array.of(us >>> 16, us >>> 8, us);
  const meter = (beats, unit) => Uint8Array.of(beats, Math.log2(unit), 96 / unit, 8);
  ns.midiFile = { META, read, write, ascii, tempo, meter };
  if (typeof module === 'object' && module.exports) module.exports = ns.midiFile;
})(typeof globalThis !== 'undefined' ? globalThis : this);
