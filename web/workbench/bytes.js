/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Song workbench (phase 5b): little-endian reading and writing, CRC-32 and
 * text helpers shared by the file codecs. Plain script, no dependencies:
 * the page inlines it and Node tests require it. */
(function (root) {
  'use strict';
  const ns = root.FM1SongWorkbench = root.FM1SongWorkbench || {};
  const fail = message => { throw Error(message); };
  const check = (ok, message) => { if (!ok) fail(message); };

  const table = new Uint32Array(256);
  for (let n = 0; n < 256; n++) {
    let c = n;
    for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
    table[n] = c >>> 0;
  }
  /* CRC-32 (IEEE), the device's pool_crc with its start and final inversion. */
  function crc32(bytes, previous = 0) {
    let c = ~previous >>> 0;
    for (let i = 0; i < bytes.length; i++) c = table[(c ^ bytes[i]) & 255] ^ (c >>> 8);
    return ~c >>> 0;
  }

  class Reader {
    constructor(bytes, what = 'file') { this.bytes = bytes; this.at = 0; this.what = what; }
    get left() { return this.bytes.length - this.at; }
    take(n) {
      check(n <= this.left, this.what + ' is cut short');
      const out = this.bytes.subarray(this.at, this.at + n);
      this.at += n;
      return out;
    }
    u8() { return this.take(1)[0]; }
    u16() { const b = this.take(2); return b[0] | b[1] << 8; }
    u32() { const b = this.take(4); return (b[0] | b[1] << 8 | b[2] << 16 | b[3] << 24) >>> 0; }
  }
  class Writer {
    constructor(size = 4096) { this.bytes = new Uint8Array(size); this.at = 0; }
    room(n) {
      if (this.at + n <= this.bytes.length) return;
      const next = new Uint8Array(Math.max(this.bytes.length * 2, this.at + n));
      next.set(this.bytes.subarray(0, this.at));
      this.bytes = next;
    }
    put(data) { this.room(data.length); this.bytes.set(data, this.at); this.at += data.length; return this; }
    u8(v) { this.room(1); this.bytes[this.at++] = v & 255; return this; }
    u16(v) { return this.u8(v).u8(v >>> 8); }
    u32(v) { return this.u16(v & 0xffff).u16(v >>> 16); }
    done() { return this.bytes.slice(0, this.at); }
  }
  const le32 = (bytes, at) => (bytes[at] | bytes[at + 1] << 8 | bytes[at + 2] << 16 | bytes[at + 3] << 24) >>> 0;
  const le16 = (bytes, at) => bytes[at] | bytes[at + 1] << 8;

  /* Fixed-width ASCII fields: text up to the first zero, trailing spaces kept
   * off on reading. Writing pads with `pad` (0 or 32). */
  function text(bytes) {
    let out = '';
    for (const b of bytes) { if (!b) break; out += String.fromCharCode(b); }
    return out.replace(/ +$/, '');
  }
  function field(value, width, pad = 0, upper = false) {
    const out = new Uint8Array(width).fill(pad);
    let clean = String(value).replace(/[^\x20-\x7e]/g, '_');
    if (upper) clean = clean.toUpperCase();
    for (let i = 0; i < Math.min(width, clean.length); i++) out[i] = clean.charCodeAt(i);
    return out;
  }
  const same = (a, b) => a.length === b.length && a.every((v, i) => v === b[i]);

  ns.bytes = { fail, check, crc32, Reader, Writer, le16, le32, text, field, same };
  if (typeof module === 'object' && module.exports) module.exports = ns.bytes;
})(typeof globalThis !== 'undefined' ? globalThis : this);
