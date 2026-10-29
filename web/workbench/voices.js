/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* The DX7 library (0.3.0): the device's 4 banks of 32 voices, read and
 * written through the stock transfer commands as kind 4 (app/transfer.c,
 * app/fm_library.c), 32 voices of 128 VMEM bytes a bank. The full build
 * leaves the factory voices out, so a bank nothing was saved to plays one
 * blank voice, and reads as zeros. The factory bank sent here is M-VAVE's
 * V15 bank (app/fm_factory.inc, Apache-2.0, THIRD_PARTY.md), taken from the
 * page's simulator, which is built with it: a fresh copy has nothing saved,
 * so its banks read as those voices. */
(function (root) {
  'use strict';
  const ns = root.FM1SongWorkbench = root.FM1SongWorkbench || {};
  const { check, crc32 } = ns.bytes;
  const BANKS = 4, BYTES = 4096, KIND = 4;
  const same = (a, b) => a.length === b.length && a.every((v, i) => v === b[i]);

  async function readBank(link, index) {
    const head = await link.one(1, [KIND, index]);
    check((head.data[0] | head.data[1] << 8) === BYTES, 'The device sent a DX7 bank of the wrong size');
    const out = new Uint8Array(BYTES), batch = [];
    try {
      for (let at = 0; at < BYTES; at += 64) batch.push([2, [at & 255, at >> 8, 64]]);
      (await link.many(batch)).forEach((r, i) => { check(r.data.length === 64, 'A piece of the DX7 bank went missing'); out.set(r.data, i * 64); });
    } finally { await link.one(6).catch(() => {}); }
    return out;
  }
  async function writeBank(link, index, raw) {
    const batch = [], crc = crc32(raw);
    await link.one(3, [KIND, index, BYTES & 255, BYTES >> 8]);
    try {
      for (let at = 0; at < BYTES; at += 64) batch.push([4, [at & 255, at >> 8, ...raw.subarray(at, at + 64)]]);
      await link.many(batch);
      await link.one(5, [crc & 255, crc >>> 8 & 255, crc >>> 16 & 255, crc >>> 24]);
    } finally { await link.one(6).catch(() => {}); }
  }
  /* The factory bank, from a fresh copy of the simulator. */
  async function factory(simulator) {
    const wasm = await root.FM1Web.assetBytes(simulator, 2097152, 'The simulator failed its check');
    const c = (await WebAssembly.instantiate(wasm, {})).instance.exports;
    c._initialize();
    return Array.from({ length: BANKS }, (_, i) => {
      check(!c.sim_library_read(i), 'The factory voices failed to load');
      return new Uint8Array(c.memory.buffer, c.sim_file(), BYTES).slice();
    });
  }
  /* Every bank gets the factory voices, after `ask(n)` agrees when n banks
   * hold saved voices of their own (neither blank nor the factory's), and
   * each is read back. The device leaves a bank it already holds as it is. */
  async function sendFactory(link, banks, ask, progress = () => {}) {
    const before = [];
    for (let i = 0; i < BANKS; i++) before.push(await readBank(link, i));
    const saved = before.filter((raw, i) => raw.some(v => v) && !same(raw, banks[i])).length;
    if (saved && !ask(saved)) return { cancelled: true, saved };
    for (let i = 0; i < BANKS; i++) { await writeBank(link, i, banks[i]); progress((i + 1) / (2 * BANKS)); }
    const wrong = [];
    for (let i = 0; i < BANKS; i++) { if (!same(await readBank(link, i), banks[i])) wrong.push(i + 1); progress(0.5 + (i + 1) / (2 * BANKS)); }
    check(!wrong.length, 'The read back does not match the factory voices in bank ' + wrong.join(', '));
    return { saved, fresh: before.filter(raw => !raw.some(v => v)).length };
  }
  ns.voices = { BANKS, BYTES, readBank, writeBank, factory, sendFactory };
})(typeof globalThis !== 'undefined' ? globalThis : this);
