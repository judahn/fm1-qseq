/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* "New song from a factory sound set" as song files (phase 5b). A private
 * copy of the simulator builds each one (sim_song_template: a new song from
 * that set, saved), then reads it back through the transfer protocol. The
 * device then needs no factory data of its own to start a song from one.
 * A template names kit 0, so the device keeps whichever kit is in use. */
(function (root) {
  'use strict';
  /* The page declares the simulator's bytes with const, so not on globalThis. */
  const simulator = () => (typeof FM1_SIMULATOR !== 'undefined' ? FM1_SIMULATOR : root.FM1_SIMULATOR);
  const ns = root.FM1SongWorkbench = root.FM1SongWorkbench || {};
  let core = null;
  async function instance() {
    if (core) return core;
    const wasm = await root.FM1Web.assetBytes(simulator(), 2097152, 'The simulator failed its check');
    core = new root.FM1SimulatorCore(wasm, null, null);
    return core;
  }
  async function names() { return (await instance()).command({ type: 'factories' }); }
  /* The song file payload for factory sound set `index`. */
  async function build(index) {
    const c = await instance(), S = ns.songFile;
    const link = ns.link.simulator(async frames => c.command({ type: 'protocol', frames }));
    const id = c.c.sim_song_template(index);
    if (id <= 0) throw Error('Factory sound set ' + (index + 1) + ' could not be built');
    const read = await link.readSong(id);
    await link.deleteSong(id);
    const song = S.decode(read.bytes);
    song.kit = 0;
    return S.encode(song);
  }
  ns.templates = { names, build };
  if (typeof module === 'object' && module.exports) module.exports = ns.templates;
})(typeof globalThis !== 'undefined' ? globalThis : this);
