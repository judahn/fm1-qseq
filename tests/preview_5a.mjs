// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
// Phase 5a of the preview check: storage. Save, change, load and compare,
// through the song list (K2 the action, YES does it). A kit made through the
// transfer calls, switched to and back. A recovery copy after a stop, offered
// and restored after a reboot on the same flash. Saves docs/phase5a-*.png.
import { simPage, savePng, checker } from './preview_lib.mjs';

export async function phase5a(browser, root) {
  const page = await simPage(browser, root);
  const out = await page.evaluate(() => {
    const r = previewRig(), { B } = r, info = r.info;
    const store = i => info(38, 0, i);
    const pads = () => [8, 9, 10, 11, 12, 13, 14, 15].map(store);
    const dump = () => {
      const parts = [];
      for (let t = 0; t < 8; t++)
        for (let i = 0; i < info(0, t); i++) parts.push([t, info(30, t, i), info(1, t, i), info(2, t, i), info(18, t, i), info(3, t, i)].join(','));
      for (let i = 0; i < info(12); i++) parts.push('m' + info(13, 0, i));
      return parts.join(';') + '|' + info(14) + ',' + info(15) + ',' + info(11);
    };
    const o = {};
    r.render(32);
    r.tap(B.SEQ);
    const bootPads = pads();
    o.boot = { kits: store(2), kit: store(3), songs: store(0), padsUsed: bootPads.filter(p => p).length };

    // 1. SAVE in the song view: the test song goes to slot 1.
    const before = dump();
    r.tap(B.SAVE);
    const crc = store(16);                         /* as saved, now with its name and programs */
    o.saved = { songs: store(0), id: store(1) };
    // 2. Change it (delete bars 3 to 6), save that as a second song from the list.
    r.held(B.FX, () => r.key(11));
    o.changed = dump() !== before;
    r.held(B.FX, () => r.tap(B.SAVE));
    o.listPage = r.page();
    r.turn(3, 2);                                  // K2: SAVE AS
    r.render(2);
    o.saveAsLabel = r.knobs()[1];
    r.tap(B.YES);
    o.songsAfterSaveAs = store(0);
    // 3. Load song 1 back: K2 to LOAD, K1 to song 1, YES. Nothing unsaved, so no question.
    r.turn(3, -2);
    r.turn(2, -5);
    r.tap(B.YES);
    o.loaded = { id: store(1), sameEvents: dump() === before, sameBytes: store(16) === crc, page: r.page() };
    r.render(4);
    o.listPng = r.png();

    // 4. A kit through the transfer calls: the stock bank with its last pad
    // changed. Only that sample is stored, the rest are shared. Then K3 back
    // to the stock bank: the pads rebind and nothing is written.
    o.kitImport = r.c.sim_song_kit_copy();
    const newPads = pads();
    o.kits = { count: store(2), kit: store(3), samples: store(7),
      shared: newPads.filter((p, i) => p && p === bootPads[i]).length, changed: newPads.filter((p, i) => p !== bootPads[i]).length };
    const revision = r.c.sim_flash_revision();
    r.turn(4, -1);
    o.switched = { kit: store(3), padsBack: pads().join() === bootPads.join(), written: r.c.sim_flash_revision() !== revision };
    r.tap(B.HOME);
    o.backToSong = r.page();

    // 5. A change, play and stop: a recovery copy appears.
    const generation = store(6);
    r.tap(B.SCL);                                  // SEL removes INTRO, at bar 1
    r.tap(B.PLAY); r.render(200); r.tap(B.PLAY); r.render(8);
    o.recovery = { before: generation, after: store(6) };
    const changed = store(16);
    r.render(4);
    o.songPng = r.png();

    // 6. Reboot on the same flash: SEQ offers the copy, YES restores it.
    const bytes = 536576, image = new Uint8Array(r.c.memory.buffer, r.c.sim_flash(), bytes).slice();
    r.boot();
    new Uint8Array(r.c.memory.buffer, r.c.sim_flash(), bytes).set(image);
    r.c.sim_boot(0);
    r.render(32);
    r.tap(B.SEQ);
    o.reboot = { page: r.page(), waiting: store(4), songs: store(0) };
    r.render(4);
    o.restorePng = r.png();
    r.render(2);
    o.restoreLabel = r.knobs()[1];
    r.tap(B.YES);
    o.restored = { sameBytes: store(16) === changed, id: store(1), waiting: store(4) };
    return o;
  });
  savePng(root, 'phase5a-song-list', out, 'listPng');
  savePng(root, 'phase5a-song-view', out, 'songPng');
  savePng(root, 'phase5a-restore', out, 'restorePng');
  await page.close();
  const t = checker('5a'), q = out;
  t.eq('at boot: one kit, the stock bank', q.boot.kits, 1);
  t.eq('at boot: that kit in use', q.boot.kit, 1);
  t.over('at boot: pads bound', q.boot.padsUsed, 0);
  t.eq('SAVE: one song', q.saved.songs, 1);
  t.eq('SAVE: it is song 1', q.saved.id, 1);
  t.yes('deleting bars changed the song', q.changed);
  t.eq('SHIFT+SAVE opens the song list', q.listPage, 25);
  t.eq('K2 picks SAVE AS', q.saveAsLabel, 'ACTION SAVE AS');
  t.eq('YES saves as a new song: two songs', q.songsAfterSaveAs, 2);
  t.eq('YES loads song 1, nothing unsaved, so at once', q.loaded.id, 1);
  t.eq('loading stays on the list', q.loaded.page, 25);
  t.yes('the loaded song has the same events', q.loaded.sameEvents);
  t.yes('the loaded song has the same bytes', q.loaded.sameBytes);
  t.eq('a kit through the transfer calls', q.kitImport, 0);
  t.eq('two kits now', q.kits.count, 2);
  t.eq('the new kit in use', q.kits.kit, 2);
  t.eq('one new sample stored', q.kits.samples, 1);
  t.eq('one pad changed', q.kits.changed, 1);
  t.eq('the other pads share the stock samples', q.kits.shared, q.boot.padsUsed - 1);
  t.eq('K3 back to the stock kit', q.switched.kit, 1);
  t.yes('the pads rebind to the stock samples', q.switched.padsBack);
  t.no('switching kits writes nothing', q.switched.written);
  t.eq('HOME goes back to the song view', q.backToSong, 19);
  t.yes('a recovery copy is written after the stop', q.recovery.after, q.recovery);
  t.yes('the recovery copy is a new one', q.recovery.after !== q.recovery.before, q.recovery);
  t.eq('after a reboot SEQ offers the copy in the song list', q.reboot.page, 25);
  t.eq('after a reboot the copy is waiting', q.reboot.waiting, 1);
  t.eq('after a reboot both songs are there', q.reboot.songs, 2);
  t.eq('the offer comes with K2 on LOAD', q.restoreLabel, 'ACTION LOAD');
  t.yes('restored, the song is what was played', q.restored.sameBytes);
  t.eq('restored, nothing waits any more', q.restored.waiting, 0);
  return { phase: '5a', checks: t.list, out };
}
