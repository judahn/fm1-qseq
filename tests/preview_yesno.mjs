// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
// YES and NO in the preview check (OCT+ and OCT-). The edit jobs: YES asks
// with the events the job takes on, NO and a setting change cancel, SAVE
// saves the song instead of running the job, YES twice runs it. The song
// list: K2 picks the action and YES does it, the questions (a box over the
// page) and what cancels them, SAVE ignored while one is open, renaming kept
// with YES and dropped with NO, LOAD and NEW asking only over unsaved
// changes, the note keys doing nothing, NO going back. Saves docs/yesno-*.png.
import { simPage, savePng, checker } from './preview_lib.mjs';

export async function phaseYesNo(browser, root) {
  const page = await simPage(browser, root);
  const out = await page.evaluate(() => {
    const r = previewRig(), { B } = r, info = r.info, store = i => info(38, 0, i);
    const written = () => r.c.sim_flash_revision();
    /* A stored song's first four letters, by its place in the list. */
    const name = row => { const v = info(52, 0, row); return String.fromCharCode(v & 255, v >> 8 & 255, v >> 16 & 255, v >> 24 & 255).replace(/\0+$/, ''); };
    /* A question is open: its box's corners, in accent or amber. */
    const asking = () => {
      r.render(8);
      const p = r.pixels(), a = p[90 * 240 + 4], b = p[153 * 240 + 235];
      return a === b && (a === 0x07ff || a === 0xfd20);
    };
    const knob = k => { r.render(2); return r.knobs()[k]; };
    const offGrid = () => Array.from({ length: info(0, 2) }, (_, i) => info(1, 2, i)).filter((at, i) => !info(30, 2, i) && at % 240).length;
    const o = {};
    r.render(32);
    r.tap(B.SEQ);

    // 1. The edit jobs: quantize the PSG (track 3) to 8ths over the whole song.
    r.turn(0, 2);
    r.held(B.FX, () => r.tap(B.EDIT));
    o.jobPage = r.page();
    const before = offGrid();
    r.turn(3, 1); r.turn(4, 2);                    // whole song, 1/16 to 1/8
    r.tap(B.YES);
    o.jobAsks = { asking: asking(), offGrid: offGrid(), before };
    o.jobAskPng = r.png();
    r.tap(B.NO);
    o.jobNo = { asking: asking(), offGrid: offGrid(), page: r.page() };
    r.tap(B.YES); r.turn(5, -1);                   // a setting turned: the question goes
    o.settingCancels = asking();
    r.turn(5, 1);
    const saves = written();
    r.tap(B.SAVE);                                 // SAVE saves the song, the job doesn't run
    o.jobSave = { songs: store(0), written: written() !== saves, offGrid: offGrid(), asking: asking(), page: r.page() };
    r.tap(B.YES); r.tap(B.YES);
    o.jobRan = { offGrid: offGrid(), asking: asking() };
    r.tap(B.NO);                                   // nothing open: back to the song view
    o.jobNoBack = r.page();

    // 2. The song list, with the song saved as it is now.
    r.tap(B.SAVE);
    r.held(B.FX, () => r.tap(B.SAVE));
    o.listPage = r.page();
    o.actions = [knob(1)];
    for (let i = 0; i < 7; i++) { r.turn(3, 1); o.actions.push(knob(1)); }
    r.turn(3, -10);
    o.newFrom = knob(3);
    const keys = written();
    for (let k = 0; k < 27; k++) r.key(k);         // the note keys do nothing here
    o.keys = { page: r.page(), songs: store(0), written: written() !== keys, asking: asking(), action: knob(1) };
    r.turn(3, 2); r.tap(B.YES);                    // SAVE AS: song 2, now the one worked on
    o.saveAs = { songs: store(0), id: store(1) };
    r.turn(3, 3); r.turn(2, -5); r.tap(B.YES);     // DUPLICATE song 1: no question
    o.duplicate = { songs: store(0), asking: asking(), name: name(2), of: name(0) };
    r.render(600);                                 // the notice runs out
    r.turn(3, 1); r.turn(2, 2);                    // DELETE, song 3
    r.render(4);
    o.listPng = r.png();
    r.tap(B.YES);
    o.deleteAsks = { asking: asking(), songs: store(0) };
    o.askPng = r.png();
    const quiet = written();
    r.tap(B.SAVE);                                 // ignored while a question is open
    o.saveIgnored = { asking: asking(), written: written() !== quiet };
    r.tap(B.NO);
    o.deleteNo = { asking: asking(), songs: store(0), page: r.page() };
    r.tap(B.YES); r.turn(2, -1);                   // another song: the question goes
    o.movedCancels = asking();
    r.turn(2, 1); r.tap(B.YES); r.turn(3, -1);     // turning K2 too
    o.turnedCancels = asking();
    r.turn(3, 1); r.tap(B.YES); r.tap(B.YES);
    o.deleted = { songs: store(0), asking: asking() };

    // 3. RENAME song 1: K1 the letter's place, K2 the letter, YES keeps the name. Again with NO: it stays.
    r.turn(2, -5); r.turn(3, -2);
    const old = name(0);
    r.tap(B.YES);
    o.naming = [knob(0), knob(1)];
    r.turn(3, 1);
    r.tap(B.SAVE);                                 // ignored while naming
    o.namingSave = knob(0);
    r.tap(B.YES);
    o.renamed = { old, now: name(0), knob: knob(1) };
    r.tap(B.YES); r.turn(3, 1); r.turn(2, 1); r.turn(3, 1); r.tap(B.NO);
    o.cancelled = { now: name(0), knob: knob(1) };

    // 4. LOAD song 1 at once, nothing unsaved. Then a change, and LOAD asks: NO keeps the song, YES loads.
    r.turn(3, -4);
    r.tap(B.YES);
    o.loadClean = { id: store(1), asking: asking(), unsaved: store(17) };
    r.tap(B.NO);                                   // nothing open: back to the song view
    o.noBack = r.page();
    r.tap(B.SCL);                                  // SEL: a marker at the playhead, a change
    o.changed = store(17);
    r.held(B.FX, () => r.tap(B.SAVE));
    r.turn(2, 1);
    r.tap(B.YES);
    o.loadAsks = { asking: asking(), id: store(1) };
    o.loadAskPng = r.png();
    r.tap(B.NO);
    o.loadNo = { asking: asking(), id: store(1) };
    r.tap(B.YES); r.tap(B.YES);
    o.loaded = { id: store(1), asking: asking(), unsaved: store(17) };

    // 5. NEW at once from blank, nothing unsaved. Again: the new song isn't saved, so it asks.
    r.turn(3, 3);
    r.tap(B.YES);
    o.newClean = { asking: asking(), events: [0, 1, 2, 3, 4, 5, 6, 7].reduce((n, t) => n + info(0, t), 0) };
    r.tap(B.YES);
    o.newAsks = asking();
    r.tap(B.NO);
    o.newNo = asking();
    r.tap(B.SAVE);                                 // nothing open: SAVE saves it
    o.saveKey = { songs: store(0), unsaved: store(17) };
    return o;
  });
  for (const [name, key] of [['list', 'listPng'], ['ask', 'askPng'], ['load-ask', 'loadAskPng'], ['jobs-ask', 'jobAskPng']])
    savePng(root, 'yesno-' + name, out, key);
  await page.close();
  const t = checker('yes/no'), q = out;
  t.eq('jobs: SHIFT+EDIT opens them', q.jobPage, 26);
  t.over('jobs: the PSG has notes off the 8th grid', q.jobAsks.before, 0);
  t.yes('jobs: YES asks', q.jobAsks.asking);
  t.eq('jobs: asking runs nothing', q.jobAsks.offGrid, q.jobAsks.before);
  t.no('jobs: NO cancels the question', q.jobNo.asking);
  t.eq('jobs: NO runs nothing', q.jobNo.offGrid, q.jobAsks.before);
  t.eq('jobs: NO with a question open stays on the page', q.jobNo.page, 26);
  t.no('jobs: turning a setting cancels the question', q.settingCancels);
  t.eq('jobs: SAVE saves the song', q.jobSave.songs, 1);
  t.yes('jobs: SAVE writes flash', q.jobSave.written);
  t.eq('jobs: SAVE doesn\'t run the job', q.jobSave.offGrid, q.jobAsks.before);
  t.no('jobs: SAVE doesn\'t ask', q.jobSave.asking);
  t.eq('jobs: YES, YES runs it', q.jobRan.offGrid, 0);
  t.no('jobs: no question after it ran', q.jobRan.asking);
  t.eq('jobs: NO with nothing open goes back to the song view', q.jobNoBack, 19);
  t.eq('list: SHIFT+SAVE opens it', q.listPage, 25);
  t.eq('list: K2 picks the action, LOAD first, DELETE last', q.actions, ['ACTION LOAD', 'ACTION SAVE', 'ACTION SAVE AS', 'ACTION NEW',
    'ACTION RENAME', 'ACTION DUPLICATE', 'ACTION DELETE', 'ACTION DELETE']);
  t.eq('list: K4 is NEW FROM', q.newFrom, 'NEW FROM BLANK');
  t.eq('list: the note keys do nothing', q.keys, { page: 25, songs: 1, written: false, asking: false, action: 'ACTION LOAD' });
  t.eq('list: YES on SAVE AS makes song 2', q.saveAs, { songs: 2, id: 2 });
  t.eq('list: YES on DUPLICATE copies song 1 without asking', [q.duplicate.songs, q.duplicate.asking], [3, false]);
  t.eq('list: the copy has song 1\'s name', q.duplicate.name, q.duplicate.of);
  t.yes('list: DELETE asks', q.deleteAsks.asking);
  t.eq('list: asking deletes nothing', q.deleteAsks.songs, 3);
  t.eq('list: SAVE is ignored while a question is open', q.saveIgnored, { asking: true, written: false });
  t.eq('list: NO cancels and keeps the song', q.deleteNo, { asking: false, songs: 3, page: 25 });
  t.no('list: moving to another song cancels the question', q.movedCancels);
  t.no('list: turning K2 cancels the question', q.turnedCancels);
  t.eq('list: YES, YES deletes', q.deleted, { songs: 2, asking: false });
  t.eq('list: RENAME opens the naming on K1 and K2', q.naming, ['CURSOR < >', 'LETTER A-Z']);
  t.eq('list: SAVE is ignored while naming', q.namingSave, 'CURSOR < >');
  t.yes('list: YES keeps the new name', q.renamed.now !== q.renamed.old && q.renamed.now.slice(1) === q.renamed.old.slice(1), q.renamed);
  t.eq('list: naming ends with YES', q.renamed.knob, 'ACTION RENAME');
  t.eq('list: NO cancels the naming, the name stays', q.cancelled, { now: q.renamed.now, knob: 'ACTION RENAME' });
  t.eq('list: YES loads at once with nothing unsaved', q.loadClean, { id: 1, asking: false, unsaved: 0 });
  t.eq('list: NO with nothing open goes back to the song view', q.noBack, 19);
  t.eq('list: a marker added is an unsaved change', q.changed, 1);
  t.eq('list: with unsaved changes YES asks before loading', q.loadAsks, { asking: true, id: 1 });
  t.eq('list: NO keeps the song', q.loadNo, { asking: false, id: 1 });
  t.eq('list: YES, YES loads', q.loaded, { id: 2, asking: false, unsaved: 0 });
  t.eq('list: NEW at once with nothing unsaved, from blank', q.newClean, { asking: false, events: 0 });
  t.yes('list: NEW asks over a new song not saved yet', q.newAsks);
  t.no('list: NO cancels it', q.newNo);
  t.eq('list: SAVE with nothing open saves', q.saveKey, { songs: 3, unsaved: 0 });
  return { phase: 'yes/no', checks: t.list, out };
}
