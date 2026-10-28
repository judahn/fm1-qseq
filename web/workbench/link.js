/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* The songs and kits protocol (app/song/song_transfer.c) from the browser:
 * the stock transfer framing, sent to the page's simulator or to an FM-1
 * over USB MIDI. Writes check for room first, and every read is checked
 * against the device's own CRC. */
(function (root) {
  'use strict';
  const ns = root.FM1SongWorkbench = root.FM1SongWorkbench || {};
  const { check, crc32, Writer, le16, le32, text, field } = ns.bytes;
  const K = () => ns.kitFile;
  const ERRORS = { 1: 'Nothing there', 67: 'The device is busy with another transfer', 71: 'The device refused the data as invalid',
    72: 'The device is out of memory', 84: 'Not enough room on the device', 85: 'Stop playback on the device first',
    86: 'The device is busy taking in a kit' };
  const u16 = v => [v & 255, v >> 8 & 255];
  const u32 = v => [v & 255, v >>> 8 & 255, v >>> 16 & 255, v >>> 24];

  function pack7(bytes) {
    const out = [];
    let value = 0, bits = 0;
    for (const b of bytes) { value |= b << bits; bits += 8; while (bits >= 7) { out.push(value & 127); value >>= 7; bits -= 7; } }
    if (bits) out.push(value & 127);
    return out;
  }
  function frame(command, token, payload) {
    const data = [240, 125, 70, 77, 2, command, token & 127, token >> 7, ...pack7(payload)];
    data.push(-data.slice(1).reduce((a, b) => a + b, 0) & 127, 247);
    return Uint8Array.from(data);
  }
  function parse(bytes) {
    if (bytes.length < 10 || bytes[0] !== 240 || bytes[1] !== 125 || bytes[2] !== 70 || bytes[3] !== 77 || bytes[4] !== 2) return null;
    if (bytes.slice(1, -1).reduce((a, b) => a + b, 0) % 128) return null;
    let value = 0, bits = 0;
    const out = [];
    for (const b of bytes.slice(8, -2)) { value |= b << bits; bits += 7; if (bits >= 8) { out.push(value & 255); value >>>= 8; bits -= 8; } }
    return { command: bytes[5], token: bytes[6] | bytes[7] << 7, status: out[0], data: Uint8Array.from(out.slice(1)) };
  }
  const failure = status => Object.assign(Error(ERRORS[status] || 'Device error ' + status), { status });
  /* A sample as the device tells samples apart: CRC, size and length. */
  const sampleKey = s => s.crc + ':' + s.bytes + ':' + s.frames;
  const padKey = p => sampleKey({ crc: crc32(p.data), bytes: p.data.length, frames: p.frames });

  /* send(requests) answers [{status, data}] for a list of [command, payload]. */
  class Link {
    constructor(send) { this.send = send; }
    async many(requests) {
      const replies = await this.send(requests);
      replies.forEach(r => { if (r.status && r.status !== 1) throw failure(r.status); });
      return replies;
    }
    async one(command, payload = []) { return (await this.many([[command, payload]]))[0]; }
    async info() {
      const d = (await this.one(32)).data;
      return { version: d[0], sectors: le16(d, 1), free: le16(d, 3), largest: le16(d, 5), songs: d[7], kits: d[8], samples: d[9],
        song: le16(d, 10), kit: le16(d, 12), playing: !!(d[14] & 1), busy: !!(d[14] & 2), songSectors: le16(d, 15),
        sampleSectors: le16(d, 17), maxKits: d[19], maxEvents: le16(d, 20), maxSong: le32(d, 22) };
    }
    async list(command, read) {
      const out = [];
      for (let index = 0; index < 255; index++) {
        const r = await this.one(command, [index]);
        if (r.status === 1 || !r.data.length) break;
        out.push(read(r.data));
      }
      return out;
    }
    songs() { return this.list(33, d => ({ id: le16(d, 0), bytes: le32(d, 2), crc: le32(d, 6), name: text(d.subarray(10, 26)) })); }
    kits() {
      return this.list(41, d => ({ id: le16(d, 0), name: text(d.subarray(2, 18)), pads: d[18], hasParams: !!d[19], bytes: le32(d, 20) }));
    }
    samples() {
      return this.list(48, d => ({ id: le16(d, 0), bytes: le32(d, 2), frames: le32(d, 6), crc: le32(d, 10), refs: d[14], name: text(d.subarray(15, 31)) }));
    }
    /* A song (34) or a kit as a stock bank (42), in 64-byte pieces. */
    async read(command, id, progress = () => {}) {
      const head = await this.one(command, u16(id));
      if (head.status === 1) throw failure(1);
      const d = head.data, bytes = le32(d, 0), crc = le32(d, 4), out = new Uint8Array(bytes);
      const item = { crc, name: text(d.subarray(8, 24)), hasParams: !!d[24], samples: Array.from({ length: 8 }, (_, i) => le16(d, 25 + i * 2)) };
      try {
        for (let at = 0; at < bytes; at += 64 * 16) {
          const batch = [];
          for (let p = at; p < Math.min(bytes, at + 64 * 16); p += 64) batch.push([35, [...u32(p), Math.min(64, bytes - p)]]);
          (await this.many(batch)).forEach((r, i) => out.set(r.data, at + i * 64));
          progress(Math.min(1, (at + 1024) / bytes));
        }
      } finally { await this.one(49).catch(() => {}); }
      const sum = command === 42 ? K().bankCrc(out) : crc32(out);
      check(sum === crc, 'The read back does not match the device CRC');
      return { ...item, bytes: out };
    }
    readSong(id, progress) { return this.read(34, id, progress); }
    async readKit(id, progress) {
      const kit = await this.read(42, id, progress);
      const params = kit.hasParams ? (await this.one(43, u16(id))).data.slice() : null;
      return { name: kit.name, params, bank: kit.bytes, samples: kit.samples };
    }
    /* Into slot `id`, or a new one for 0. Answers the id it went to. */
    async writeSong(id, payload, progress = () => {}) {
      const info = await this.info(), need = K().sectors(payload.length);
      check(!info.playing, ERRORS[85]);
      check(info.free >= need, 'Not enough room on the device: this song needs ' + need * 4 + ' KB, ' + info.free * 4 + ' KB is free');
      await this.one(36, [...u16(id), ...u32(payload.length)]);
      try {
        await this.chunks(37, payload, progress);
        return le16((await this.one(38, u32(crc32(payload)))).data, 0);
      } finally { await this.one(49).catch(() => {}); }
    }
    async chunks(command, bytes, progress) {
      for (let at = 0; at < bytes.length; at += 128 * 16) {
        const batch = [];
        for (let p = at; p < Math.min(bytes.length, at + 128 * 16); p += 128) batch.push([command, [...u32(p), ...bytes.subarray(p, p + 128)]]);
        await this.many(batch);
        progress(Math.min(1, (at + 2048) / bytes.length));
      }
    }
    /* What a kit would add: its samples not on the device yet, by CRC, size
     * and length, as the device itself tells them apart. The kit table is
     * kept twice, a sector each: rewriting it takes one free sector, and the
     * first kit two. */
    async kitCost(pads, kits = 1) {
      const have = await this.samples(), seen = new Set(have.map(sampleKey));
      let sectors = kits ? 1 : 2, fresh = 0;
      for (const p of pads) {
        const key = padKey(p);
        if (!p.frames || seen.has(key)) continue;
        seen.add(key); fresh++;
        sectors += K().sectors(p.data.length);
      }
      return { sectors, fresh };
    }
    async writeKit(kit, progress = () => {}) {
      const info = await this.info(), cost = await this.kitCost(K().readBank(kit.bank), info.kits);
      check(!info.playing, ERRORS[85]);
      check(info.kits < info.maxKits, 'The device holds ' + info.maxKits + ' kits at most');
      check(info.free >= cost.sectors, 'Not enough room on the device: this kit needs ' + cost.sectors * 4 + ' KB, ' + info.free * 4 + ' KB is free');
      await this.one(12, u32(kit.bank.length));
      try {
        await this.chunks(13, kit.bank, progress);
        const settings = kit.params ? [1, ...kit.params] : [0, ...new Uint8Array(48)];
        const r = await this.one(47, [...field(kit.name, 16), ...settings]);
        return { id: le16(r.data, 0), fresh: cost.fresh };
      } catch (error) { await this.one(16).catch(() => {}); throw error; }
    }
    /* A kit on the device with these samples, pad for pad, or null. Only
     * when every sample is there already are the kits' pads read. */
    async findKit(pads) {
      const keys = pads.map(p => (p.frames ? padKey(p) : '')), byId = new Map((await this.samples()).map(s => [s.id, sampleKey(s)]));
      const there = new Set(byId.values());
      if (!keys.every(key => !key || there.has(key))) return null;
      for (const kit of await this.kits()) {
        let head;
        try { head = await this.one(42, u16(kit.id)); } finally { await this.one(49).catch(() => {}); }
        const ids = Array.from({ length: 8 }, (_, i) => le16(head.data, 25 + i * 2));
        if (head.status !== 1 && ids.every((id, i) => (id ? byId.get(id) || '?' : '') === keys[i])) return kit;
      }
      return null;
    }
    setKit(id, name, params) { return this.one(44, [...u16(id), ...field(name, 16), ...(params ? [1, ...params] : [0, ...new Uint8Array(48)])]); }
    deleteSong(id) { return this.one(39, u16(id)); }
    loadSong(id) { return this.one(40, u16(id)); }
    async saveWorking(asNew) { return le16((await this.one(50, [asNew ? 1 : 0])).data, 0); }
    deleteKit(id) { return this.one(45, u16(id)); }
    useKit(id) { return this.one(46, u16(id)); }
  }

  /* Transports. The simulator takes a batch in one call, the device one
   * request at a time through the page's library session. */
  let token = 1;
  function simulator(protocol) {
    return new Link(async requests => {
      const frames = requests.map(([command, payload]) => frame(command, token = (token + 1) & 16383, payload));
      const replies = await protocol(frames);
      return frames.map((f, i) => {
        const want = f[6] | f[7] << 7, r = (replies[i] || []).map(parse).find(p => p && p.token === want && p.command === (f[5] | 64));
        check(r, 'The simulator did not answer');
        return { status: r.status, data: r.data };
      });
    });
  }
  function device(library) {
    return new Link(async requests => {
      const out = [];
      for (const [command, payload] of requests) {
        try { out.push({ status: 0, data: await library.query(command, payload, true) }); }
        catch (error) { if (!error.deviceCode) throw error; out.push({ status: error.deviceCode, data: new Uint8Array(0) }); }
      }
      return out;
    });
  }
  ns.link = { Link, ERRORS, frame, parse, pack7, simulator, device };
  if (typeof module === 'object' && module.exports) module.exports = ns.link;
})(typeof globalThis !== 'undefined' ? globalThis : this);
