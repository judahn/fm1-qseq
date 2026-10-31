// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
// Takes, ALGO and the start point in 0.5.0, in the preview check. A take
// into one track shows the roll: from the song view, a grid cell and the
// clip editor, then each page comes back as it ends. A performance stays on
// the grid and the automation view keeps its own. ALGO zooms the song view,
// the clip editor and the automation view, the roll during a take, and in
// the automation view K2 sets tempo in whole BPM (0.5.2, SHIFT+ALGO before).
// PLAY starts from the start point and every stop comes back to it: PRESETS
// sets it (0.5.2, K1 before), a marker jump, SHIFT+PLAY and leaving step
// entry too, and the grid's stop and the song running out come back there.
// A small mark on the ruler shows it while playing elsewhere. Saves
// docs/take-roll.png.
import { simPage, savePng, checker } from './preview_lib.mjs';

export async function phaseStart(browser, root) {
  const page = await simPage(browser, root);
  const out = await page.evaluate(() => {
    const r = previewRig(), { B } = r, o = {};
    const { tap, key, held, turn, render, info } = r, pageNow = r.page;
    const shown = () => info(58, 0, 2), start = () => info(59), clock = () => info(8), recording = () => info(29, 0, 0);
    const zoom = () => info(58, 0, 6);              /* the song view's zoom, 0 for 16 bars (0.5.2: the footer no longer shows it) */
    const counting = () => { let n = 0; while (info(28) === 0xffffffff && n++ < 3000) render(1); };
    const at = (x, y) => r.pixels()[y * 240 + x];
    render(32);
    tap(B.SEQ);

    // 1. ALGO zooms the song view.
    const z = o.zoom = {};
    z.song = [zoom(), (turn(1, 1), zoom()), (turn(1, -1), zoom())];

    // 2. The start point: PRESETS, PLAY, and the stop comes back. The ruler
    // marks it while the playhead is elsewhere.
    const s = o.start = {};
    turn(6, 1);                                     // bar 2
    s.k1 = [clock(), start()];
    tap(B.PLAY); render(300);
    render(4);
    s.marked = [clock() > 1920, at(35, 44)];
    tap(B.PLAY); render(4);
    s.stopped = [clock(), at(35, 44)];
    turn(6, 3);                                     // bar 5
    held(B.FX, () => key(16));                      // SHIFT+A4: marker 3, BREAK, bar 7
    s.marker = [clock(), start()];
    tap(B.PLAY); render(200); tap(B.PLAY);
    s.markerBack = clock();
    held(B.FX, () => tap(B.PLAY));                  // SHIFT+PLAY, the loop off: the top
    s.top = [clock(), start()];
    tap(B.ENV);                                     // the loop on
    held(B.FX, () => tap(B.PLAY));                  // SHIFT+PLAY: the loop's start
    s.loop = [clock() === info(14), start() === info(14)];
    tap(B.ENV);
    turn(6, 2);                                     // PRESETS two bars on, then step entry
    r.step();
    turn(6, 3);
    const pointer = info(17);
    r.step();                                       // SHIFT+REC out: the playhead and the start at the pointer
    s.left = [pageNow(), clock() - pointer, start() - pointer];
    tap(B.SEQ);                                     // the grid: PLAY, and the stop comes back too
    tap(B.PLAY); render(300);
    s.gridPlaying = clock() > pointer;
    tap(B.PLAY);
    s.grid = [pageNow(), clock() - pointer];
    tap(B.SEQ);
    turn(6, 6);                                     // bar 12: played out to the end, back there
    const last = start();
    tap(B.PLAY);
    let n = 0;
    while (info(7) && n++ < 3000) render(10);
    s.end = [info(7), clock() - last];

    // 3. A take from the song view shows the roll: the count-in there, ALGO
    // zooming it, and the song view back as REC ends it.
    const tk = o.take = {};
    tap(B.REC);
    tk.song = [pageNow(), recording(), shown()];
    counting();
    turn(1, -1);
    tk.zoomed = [info(58, 0, 0), r.knobs()[3]];
    render(40);
    o.takePng = r.png();
    tap(B.REC);
    tk.songEnded = [pageNow(), recording(), shown(), info(7), zoom()];
    tap(B.PLAY);
    turn(1, 0);

    // 4. A take into a grid cell: the roll from the count-in, then the grid.
    tap(B.SEQ);
    tk.cellArmed = (tap(B.REC), [pageNow(), shown()]);
    counting();
    tk.cell = [info(36) & 3, shown()];
    render(100);
    tap(B.REC);
    tk.cellEnded = [pageNow(), info(36) & 3, shown()];
    tap(B.PLAY);
    // A performance on the SONG row stays on the grid, armed and taking.
    turn(2, -1);
    tap(B.REC);
    tk.perfArmed = [info(47) & 255, shown()];
    tap(B.PLAY); render(100);
    tk.perf = [info(47) & 255, shown()];
    tap(B.REC); tap(B.PLAY);
    tk.perfEnded = [info(47) & 255, shown()];
    // The clip editor: ALGO zooms it, and a take shows the roll.
    turn(2, 1);
    tap(B.SCL);
    tk.clip = [pageNow(), info(42) & 255];
    z.clip = [zoom(), (turn(1, 1), zoom())];
    turn(1, -1);
    tap(B.REC);
    tk.clipTake = [recording(), shown()];
    counting();
    render(40);
    tap(B.REC);
    tk.clipEnded = [pageNow(), recording(), shown()];
    tap(B.PLAY);
    tap(B.SEQ); tap(B.SEQ);                         // the grid, then the song view

    // 5. The automation view: ALGO zooms, and leaves the tempo with SHIFT
    // too, and K2 sets it in whole BPM (0.5.2). A take there stays there.
    held(B.FX, () => tap(B.LFO));
    tk.auto = pageNow();
    z.auto = [zoom(), (turn(1, 1), zoom())];
    turn(1, -1);
    held(B.FX, () => tap(B.PLAY));                  // the top
    for (let i = 0; i < 8; i++) turn(0, -1);        // past track 1: the conductor
    const bpm = info(6, 0, 0);
    turn(1, 2);
    held(B.FX, () => turn(1, -2));
    z.plain = info(6, 0, 0) - bpm;
    turn(3, 2);
    z.shifted = info(6, 0, 0) - bpm;
    turn(0, 1);
    tap(B.REC);
    tk.autoTake = [pageNow(), recording(), shown()];
    counting();
    tap(B.REC); tap(B.PLAY);
    tk.autoEnded = [pageNow(), recording(), shown()];
    return o;
  });
  savePng(root, 'take-roll', out, 'takePng');
  await page.close();
  const t = checker('start'), q = out, z = q.zoom, s = q.start, k = q.take, TEXT = 0xef5d;
  t.eq('ALGO zooms the song view', z.song, [0, 1, 0]);
  t.eq('PRESETS moves the playhead and the start point', s.k1, [1920, 1920]);
  t.eq('playing elsewhere, the ruler marks the start point', s.marked, [true, TEXT]);
  t.eq('stopping comes back to it, and the mark goes', s.stopped[0], 1920);
  t.no('no mark with the playhead on it', s.stopped[1] === TEXT);
  t.eq('a marker jump sets it', s.marker, [11520, 11520]);
  t.eq('and the stop comes back there', s.markerBack, 11520);
  t.eq('SHIFT+PLAY sets it at the top', s.top, [0, 0]);
  t.eq('or at the loop\'s start', s.loop, [true, true]);
  t.eq('leaving step entry sets it at the pointer', s.left, [19, 0, 0]);
  t.yes('the grid plays', s.gridPlaying);
  t.eq('and its stop comes back to the start point', s.grid, [27, 0]);
  t.eq('played out at the song\'s end: back at the start point', s.end, [0, 0]);
  t.eq('REC in the song view: a take, the roll shown, the page still the song view', k.song, [19, 1, 28]);
  t.eq('ALGO zooms the roll during the take, K4 a recording knob in the footer', k.zoomed, [1, 'REVERB 0']);
  t.eq('REC ends it, the song view back, playing on, its zoom as it was', k.songEnded, [19, 0, 19, 1, 0]);
  t.eq('REC on a grid cell: the roll from the count-in, the page the grid', k.cellArmed, [27, 28]);
  t.eq('recording into the cell on the roll', k.cell, [2, 28]);
  t.eq('REC ends it: the grid back', k.cellEnded, [27, 0, 27]);
  t.eq('a performance armed stays on the grid', k.perfArmed, [1, 27]);
  t.eq('and taking', k.perf, [2, 27]);
  t.eq('and ended', k.perfEnded, [0, 27]);
  t.eq('SEL opens the clip in the editor', k.clip, [19, 8]);
  t.eq('ALGO zooms the clip editor', z.clip, [0, 1]);
  t.eq('REC there: a take on the roll', k.clipTake, [1, 28]);
  t.eq('REC ends it: the clip editor back', k.clipEnded, [19, 0, 19]);
  t.eq('the automation view', k.auto, 24);
  t.eq('ALGO zooms it', z.auto, [0, 1]);
  t.eq('ALGO on the conductor leaves the tempo, SHIFT or not', z.plain, 0);
  t.eq('K2 sets it in whole BPM', z.shifted, 200);
  t.eq('a take in the automation view stays there', k.autoTake, [24, 1, 24]);
  t.eq('and ends there', k.autoEnded, [24, 0, 24]);
  return { phase: 'start', checks: t.list, out };
}
