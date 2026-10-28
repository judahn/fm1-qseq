/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Kits (phase 5b, docs/file-formats.md). A kit travels to the device as a
 * stock sample bank: a 512-byte header, then 8 pads of SEA5 audio. A .fm1kit
 * file is an 80-byte header (name, pad settings, CRC) around that bank. */
(function (root) {
  'use strict';
  const ns = root.FM1SongWorkbench = root.FM1SongWorkbench || {};
  const { check, crc32, Writer, le32, text, field } = ns.bytes;

  const BANK_MAGIC = 0x53314d46, KIT_MAGIC = 0x544b4d46; /* "FM1S", "FMKT" */
  const HEADER = 512, RATE = 32000, CHUNK_FRAMES = 320, CHUNK_BYTES = 228, PADS = 8, PARAMS = 6, FILE_HEADER = 80;
  const SECTOR = 4096, POOL_HEADER = 64;
  const chunkBytes = frames => Math.ceil(frames / CHUNK_FRAMES) * CHUNK_BYTES;
  /* Pool sectors a sample of `bytes` takes on the device: its own extent. */
  const sectors = bytes => Math.ceil((POOL_HEADER + bytes) / SECTOR);

  /* The stock bank CRC leaves out its own crc and commit words. */
  function bankCrc(bank) {
    const zero = new Uint8Array(8);
    return crc32(bank.subarray(20), crc32(zero, crc32(bank.subarray(0, 12))));
  }
  function readBank(bank) {
    check(bank instanceof Uint8Array && bank.length >= HEADER, 'Sample bank is cut short');
    check(le32(bank, 0) === BANK_MAGIC && le32(bank, 4) === 1 && le32(bank, 8) === bank.length && le32(bank, 16) === 0 &&
      le32(bank, 20) === RATE && !le32(bank, 24) && !le32(bank, 28), 'Not a stock sample bank');
    check(le32(bank, 12) === bankCrc(bank), 'Sample bank is damaged (CRC)');
    const pads = [];
    for (let i = 0, next = HEADER; i < 16; i++) {
      const at = 32 + i * 28, offset = le32(bank, at), frames = le32(bank, at + 4), bytes = le32(bank, at + 8);
      const name = bank.subarray(at + 12, at + 28);
      check(offset === next && bytes === chunkBytes(frames) && bytes <= bank.length - next && !name[15] &&
        name.every(b => !b || (b >= 32 && b <= 126)) && (i < PADS || (!frames && !name[0])), 'Bad sample entry ' + (i + 1));
      for (let p = offset; p < offset + bytes; p += CHUNK_BYTES) check(le32(bank, p) === 0x5a144501, 'Bad SEA5 data on pad ' + (i + 1));
      if (i < PADS) pads.push({ name: text(name), frames, data: bank.slice(offset, offset + bytes) });
      next += bytes;
      if (i === 15) check(next === bank.length, 'Sample bank has extra bytes');
    }
    return pads;
  }
  function buildBank(pads) {
    check(pads.length === PADS, 'A kit has 8 pads');
    const size = HEADER + pads.reduce((n, p) => n + p.data.length, 0), bank = new Uint8Array(size);
    const w = new Writer(HEADER).u32(BANK_MAGIC).u32(1).u32(size).u32(0).u32(0).u32(RATE).u32(0).u32(0);
    let offset = HEADER;
    for (let i = 0; i < 16; i++) {
      const p = i < PADS ? pads[i] : { name: '', frames: 0, data: new Uint8Array(0) };
      check(p.data.length === chunkBytes(p.frames), 'Pad ' + (i + 1) + ' data does not match its length');
      w.u32(offset).u32(p.frames).u32(p.data.length).put(field(p.name, 15)).u8(0);
      bank.set(p.data, offset);
      offset += p.data.length;
    }
    bank.set(w.done(), 0);
    bank.set(new Writer(4).u32(bankCrc(bank)).done(), 12);
    readBank(bank);
    return bank;
  }

  /* "FMKT", version 1, flags (1: pad settings), name[16], settings[8][6],
   * bank length, CRC-32 of the header's first 76 bytes and then the bank. */
  function writeKit(kit) {
    const head = new Writer(FILE_HEADER).u32(KIT_MAGIC).u16(1).u16(kit.params ? 1 : 0).put(field(kit.name, 15))
      .u8(0).put(kit.params || new Uint8Array(PADS * PARAMS)).u32(kit.bank.length).done();
    const crc = crc32(kit.bank, crc32(head));
    return new Writer(FILE_HEADER + kit.bank.length).put(head).u32(crc).put(kit.bank).done();
  }
  function readKit(file) {
    check(file instanceof Uint8Array && file.length >= FILE_HEADER && le32(file, 0) === KIT_MAGIC, 'Not an FM-1 kit file');
    check(file[4] === 1 && file[5] === 0 && file[6] <= 1 && !file[7], 'This kit file needs a newer workbench');
    const length = le32(file, 72), bank = file.subarray(FILE_HEADER);
    check(bank.length === length && crc32(bank, crc32(file.subarray(0, 76))) === le32(file, 76), 'The kit file is damaged (CRC)');
    const kit = { name: text(file.subarray(8, 24)), params: file[6] ? file.slice(24, 72) : null, bank: bank.slice() };
    kit.pads = readBank(kit.bank);
    return kit;
  }

  ns.kitFile = { HEADER, RATE, CHUNK_FRAMES, CHUNK_BYTES, PADS, PARAMS, chunkBytes, sectors, bankCrc, readBank, buildBank,
    writeKit, readKit };
  if (typeof module === 'object' && module.exports) module.exports = ns.kitFile;
})(typeof globalThis !== 'undefined' ? globalThis : this);
