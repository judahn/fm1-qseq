// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
// Buttons across the song pages (0.3.0) in the preview check. SAVE opens the
// song list, and SAVE there saves. HOME+SAVE undoes and SHIFT+HOME+SAVE
// redoes with either one first, landing on the page they started from,
// and the list never shows on the way. A tap of FX opens the stock effects
// page from the song view, the grid, the automation view and the settings,
// FX there comes back and SEQ goes to the song view. Held, FX is still SHIFT,
// and step entry keeps it. The engine's tempo follows the song's: the
// settings' tempo, rounded, and the conductor's change at bar 9 as playing
// crosses it. A tempo set on the effects page's delay goes into the song.
// Saves docs/buttons-effects.png.
import { simPage, savePng, checker } from './preview_lib.mjs';

export async function phaseButtons(browser, root) {
  const page = await simPage(browser, root);
  const out = await page.evaluate(() => {
    const r = previewRig(), { B, c } = r, info = r.info, render = n => r.render(n);
    const engine = () => info(54) & 0xffff, markers = () => info(12);
    /* Buttons down and up in this order, with the page after each step. */
    const steps = list => list.map(([b, down]) => { c.sim_control(0, b, down); return r.page(); });
    const o = {};
    render(32);
    r.tap(B.SEQ);

    // 1. SAVE.
    r.tap(B.SAVE);
    o.saveOpens = r.page();
    const songs = info(38, 0, 0);
    r.tap(B.SAVE);
    o.saved = info(38, 0, 0) - songs;
    r.tap(B.HOME);
    o.back = r.page();
    // The combos, after an edit (SEL: the marker at the playhead goes).
    const combo = {}, base = markers();
    r.tap(B.SCL);
    combo.edited = markers() - base;
    combo.homeFirst = steps([[B.HOME, 1], [B.SAVE, 1], [B.SAVE, 0], [B.HOME, 0]]);
    combo.afterUndo = markers() - base;
    combo.redoSaveFirst = steps([[B.FX, 1], [B.SAVE, 1], [B.HOME, 1], [B.HOME, 0], [B.SAVE, 0], [B.FX, 0]]);
    combo.afterRedo = markers() - base;
    combo.undoSaveFirst = steps([[B.SAVE, 1], [B.HOME, 1], [B.SAVE, 0], [B.HOME, 0]]);
    combo.afterUndo2 = markers() - base;
    combo.redoHomeFirst = steps([[B.FX, 1], [B.HOME, 1], [B.SAVE, 1], [B.SAVE, 0], [B.HOME, 0], [B.FX, 0]]);
    combo.afterRedo2 = markers() - base;
    r.tap(B.SEQ);                                   // the grid: SAVE first lands back there too
    combo.grid = steps([[B.SAVE, 1], [B.HOME, 1], [B.HOME, 0], [B.SAVE, 0]]);
    combo.afterGrid = markers() - base;
    r.tap(B.SEQ);
    o.combo = combo;

    // 2. FX.
    const fx = o.fx = {};
    r.tap(B.FX);
    fx.songView = r.page();
    render(4);
    o.effectsPng = r.png();
    r.tap(B.FX);
    fx.back = r.page();
    r.tap(B.SEQ);                                   // the grid
    r.tap(B.FX);
    fx.grid = r.page();
    r.tap(B.SEQ);
    fx.seq = r.page();                              // SEQ on the effects page: the song view
    r.held(B.FX, () => r.tap(B.LFO));               // SHIFT+LFO: the automation view
    fx.auto = r.page();
    r.tap(B.FX);
    fx.autoFx = r.page();
    r.tap(B.FX);
    fx.autoBack = r.page();
    r.held(B.FX, () => r.tap(B.LFO));
    r.tap(B.GLO);                                   // the settings
    r.tap(B.FX);
    fx.setup = r.page();
    r.tap(B.FX);
    fx.setupBack = r.page();
    r.tap(B.GLO);
    const clip = info(29, 0, 11);
    r.held(B.FX, () => r.key(0));                   // held: SHIFT+F3 copies, no effects page
    fx.shiftCopy = [r.page(), info(29, 0, 11) !== clip || clip > 0];
    r.held(B.FX, () => render(250));                // held alone past 600 ms: no effects page
    fx.longHold = r.page();
    r.tap(B.REC); r.tap(B.REC);                     // step entry (the chooser starts on STEP)
    r.tap(B.FX);
    fx.step = r.page();
    r.tap(B.REC);

    // 3. The engine's tempo.
    const tp = o.tempo = {};
    tp.start = [engine(), info(6, 0, 0)];
    r.tap(B.GLO);
    r.turn(2, 5);                                   // K1: 125 BPM
    tp.settings = [info(6, 0, 0), engine()];
    r.held(B.FX, () => r.turn(2, 50));              // SHIFT+K1: 125.50
    tp.hundredths = [info(6, 0, 0), engine(), r.page()];
    r.tap(B.GLO);
    while (info(8) > 13440) r.turn(2, -1);          // K1: to bar 8
    while (info(8) < 13440) r.turn(2, 1);
    r.tap(B.PLAY);
    render(200);
    tp.beforeChange = [info(8) < 15360, engine()];
    let guard = 0;
    while (info(8) < 15360 + 240 && guard++ < 2000) render(1);
    tp.afterChange = [info(8) >= 15360, engine(), info(6, 0, 1)];
    r.tap(B.PLAY);
    r.held(B.FX, () => r.tap(B.PLAY));              // back before bar 9
    tp.stoppedBack = engine();
    r.tap(B.FX);                                    // the effects page's delay: its tempo knob
    r.turn(6, 2);
    r.turn(4, 2);
    tp.stock = engine();
    r.tap(B.FX);
    tp.adopted = [info(6, 0, 0), engine(), r.page()];
    return o;
  });
  savePng(root, 'buttons-effects', out, 'effectsPng');
  await page.close();
  const t = checker('buttons'), cb = out.combo, fx = out.fx, tp = out.tempo;
  t.eq('SAVE in the song view opens the song list', out.saveOpens, 25);
  t.eq('SAVE there saves a song', out.saved, 1);
  t.eq('HOME: back to the song view', out.back, 19);
  t.yes('HOME+SAVE, HOME first: the list never shows, back on the song view', !cb.homeFirst.includes(25) && cb.homeFirst.at(-1) === 19, cb.homeFirst);
  t.eq('and the marker is undone', cb.afterUndo, 0);
  t.yes('SHIFT+HOME+SAVE, SAVE first: no list, the song view', !cb.redoSaveFirst.includes(25) && cb.redoSaveFirst.at(-1) === 19, cb.redoSaveFirst);
  t.eq('and it is redone', cb.afterRedo, cb.edited);
  t.yes('HOME+SAVE, SAVE first: no list, the song view', !cb.undoSaveFirst.includes(25) && cb.undoSaveFirst.at(-1) === 19, cb.undoSaveFirst);
  t.eq('undone again', cb.afterUndo2, 0);
  t.yes('SHIFT+HOME+SAVE, HOME first: no list, the song view', !cb.redoHomeFirst.includes(25) && cb.redoHomeFirst.at(-1) === 19, cb.redoHomeFirst);
  t.eq('redone again', cb.afterRedo2, cb.edited);
  t.eq('the edit undone and redone: SEL removed the marker at the top', cb.edited, -1);
  t.yes('on the grid, SAVE first: no list, back on the grid', !cb.grid.includes(25) && cb.grid.at(-1) === 27, cb.grid);
  t.eq('and undone', cb.afterGrid, 0);
  t.eq('FX tapped in the song view: the effects page', fx.songView, 10);
  t.eq('FX there: back to the song view', fx.back, 19);
  t.eq('FX tapped in the grid: the effects page', fx.grid, 10);
  t.eq('SEQ there: the song view', fx.seq, 19);
  t.eq('the automation view', fx.auto, 24);
  t.eq('FX tapped there: the effects page, and FX back', [fx.autoFx, fx.autoBack], [10, 24]);
  t.eq('FX tapped in the settings: the effects page, and FX back', [fx.setup, fx.setupBack], [10, 21]);
  t.eq('FX held with a key is SHIFT: SHIFT+F3 copies, the song view stays', fx.shiftCopy, [19, true]);
  t.eq('FX held alone past 600 ms: no effects page', fx.longHold, 19);
  t.eq('step entry keeps FX to itself', fx.step, 20);
  t.eq('the engine\'s tempo starts at the song\'s', tp.start, [120, 12000]);
  t.eq('settings K1 to 125: the engine follows', tp.settings, [12500, 125]);
  t.eq('125.50: the engine rounds to 126, FX held as SHIFT', tp.hundredths, [12550, 126, 21]);
  t.eq('playing from bar 8: still 126 before bar 9', tp.beforeChange, [true, 126]);
  t.eq('across the change at bar 9: 133', tp.afterChange, [true, 133, 13300]);
  t.eq('stopped back before bar 9: 126 again', tp.stoppedBack, 126);
  t.eq('the delay page\'s tempo knob: the engine at 128', tp.stock, 128);
  t.eq('and the song\'s starting tempo follows it, back on the song view', tp.adopted, [12800, 128, 19]);
  return { phase: 'buttons', checks: t.list, out };
}
