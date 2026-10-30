// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
// REC and playback in 0.4.0, in the preview check. A tap of REC in the song
// view starts a take in the song's live mode (OVERDUB in a new song), and
// SHIFT+REC is step entry at the playhead. A held REC opens the record
// settings, listing REPLACE, OVERDUB, PUNCH, LOOP and STEP, where STEP is
// never kept, and HOME goes back to the view it came from. SHIFT+SEL
// switches REPLACE and OVERDUB (from PUNCH or LOOP, OVERDUB), the mode shows
// at the right of the bottom line, and it survives a save and a load. In the
// grid SHIFT+REC opens a cell's clip in step entry at its start, a new clip
// REC LEN long (4 bars for FREE), and SHIFT+REC there goes back to the
// grid. PLAY plays the clip there and stays, and SEQ goes back with it
// launched (0.5.0). Playback follows the view: the grid plays
// only launched clips and the song view only the song. A clip block in the
// song view is drawn in its track's full color with its clip's notes, and
// the track's own notes are hidden under it. Saves docs/rec-settings.png and
// docs/rec-block.png.
import { simPage, savePng, checker } from './preview_lib.mjs';

export async function phaseRec(browser, root) {
  const page = await simPage(browser, root);
  const out = await page.evaluate(() => {
    const r = previewRig(), { B, c } = r;
    const { tap, key, held, turn, render, info, png } = r, pageNow = r.page;
    const mode = () => info(29, 0, 3), recording = () => info(29, 0, 0);
    const from = t => info(41, t) & 255, waiting = t => info(41, t) >> 8 & 255, all = f => [0, 1, 2, 3, 4, 5, 6, 7].map(f);
    const pixel = (x, y) => r.pixels()[y * 240 + x];
    const box = (lcd, x0, y0, x1, y1, color) => { /* where `color` is drawn in the rectangle */
      const b = { left: 240, right: -1 };
      for (let y = y0; y < y1; y++)
        for (let x = x0; x < x1; x++) if (lcd[y * 240 + x] === color) { b.left = Math.min(b.left, x); b.right = Math.max(b.right, x); }
      return b;
    };
    const RED = 0xf800, PANEL = 0x18e3, DIM = 0x738e;
    const modeShown = () => box(r.pixels(), 6, 231, 240, 238, RED);  /* the REC mode at the bottom line's right end */
    const o = {};
    render(32);
    tap(B.SEQ);
    render(4);

    // 1. The song view, stopped: REC tapped starts a take at once, in the
    // song's mode, with its count-in. REC again calls it off. Playing, a
    // tap only says to stop first.
    const v = o.view = {};
    v.page = pageNow();
    v.mode = mode();
    v.shown = modeShown();
    tap(B.REC);
    v.take = { recording: recording(), playing: info(7), page: pageNow(), quiet: info(29, 0, 14) };
    tap(B.REC);
    v.calledOff = { recording: recording(), playing: info(7) };
    tap(B.PLAY);
    render(50);
    tap(B.REC);
    v.whilePlaying = { recording: recording(), playing: info(7), page: pageNow() };
    held(B.FX, () => tap(B.REC));
    v.stepWhilePlaying = pageNow();
    tap(B.PLAY);
    // SHIFT+REC stopped: step entry at the playhead. SHIFT+REC leaves it (0.5.0).
    turn(2, 3);
    const at = info(8);
    r.step();
    v.step = { page: pageNow(), pointer: info(17) - at };
    r.step();
    v.stepLeft = pageNow();

    // 2. A held REC: the record settings. K1 goes through the list in its
    // order, a live mode becoming the song's and STEP never.
    const s = o.settings = {};
    r.long(B.REC);
    s.page = pageNow();
    const row = () => { /* the highlighted row, and the row whose name is red (the song's mode) */
      const lcd = r.pixels(), rows = [0, 1, 2, 3, 4];
      return [rows.find(i => lcd[(37 + i * 20) * 240 + 8] === PANEL), rows.find(i => box(lcd, 12, 36 + i * 20, 80, 52 + i * 20, RED).right >= 0)];
    };
    render(2);
    s.start = [r.knobs()[0], mode(), ...row()];
    turn(2, -1);
    s.list = [0, 1, 2, 3, 4].map(i => { if (i) turn(2, 1); render(2); return [r.knobs()[0], mode(), ...row()]; });
    render(2);
    s.png = png();
    tap(B.HOME);
    s.home = [pageNow(), mode()];
    held(B.FX, () => tap(B.SCL));                   // SHIFT+SEL from LOOP: OVERDUB
    s.fromLoop = mode();

    // 3. HOME goes back where the settings came from: the grid, and the
    // automation view.
    tap(B.SEQ);
    r.long(B.REC);
    s.fromGrid = pageNow();
    tap(B.HOME);
    s.backToGrid = pageNow();
    tap(B.SEQ);
    held(B.FX, () => tap(B.LFO));
    s.auto = pageNow();
    render(4);
    s.autoShown = modeShown().right;                // the automation view keeps its line
    r.long(B.REC);
    s.fromAuto = pageNow();
    tap(B.HOME);
    s.backToAuto = pageNow();
    r.step();                                       // step entry from the automation view, and SHIFT+REC goes back there
    s.stepFromAuto = [pageNow(), (r.step(), pageNow())];
    held(B.FX, () => tap(B.LFO));

    // 4. SHIFT+SEL: REPLACE, shown in the song view and the grid, and it
    // survives a save and a load.
    const m = o.mode = {};
    held(B.FX, () => tap(B.SCL));
    render(560);                                    // past the notice
    m.replace = [mode(), modeShown()];
    tap(B.SEQ);
    render(4);
    m.grid = modeShown();
    tap(B.SEQ);
    tap(B.SAVE); tap(B.SAVE); tap(B.HOME);          // saved
    held(B.FX, () => tap(B.SCL));
    m.changed = mode();
    tap(B.SAVE); tap(B.YES); tap(B.YES);            // loaded back: YES asks first, the change being unsaved, then the song view (0.5.1)
    m.loaded = [mode(), pageNow()];
    held(B.FX, () => tap(B.SCL));
    m.back = mode();

    // 5. The grid's SHIFT+REC: step entry in the cell's clip at its start, a
    // new one REC LEN long (4 bars), or 4 bars on FREE. On the SONG row, or
    // playing, it stays in the grid.
    const g = o.grid = {};
    tap(B.SEQ);
    turn(0, -7);                                    // track 1, row 1
    r.step();
    g.empty = { page: pageNow(), open: info(42) & 255, length: info(39, 0, 0), pointer: info(17) };
    r.step();                                       // SHIFT+REC: back in the grid
    g.closed = [pageNow(), info(42) & 255];
    r.step();
    g.again = { page: pageNow(), open: info(42) & 255, length: info(39, 0, 0) };
    tap(B.PLAY); render(20);                        // still in step entry, the clip playing (0.5.0)
    g.played = [pageNow(), info(42) & 255, info(7)];
    tap(B.SEQ); render(20);                         // the grid again, the clip launched and playing on
    g.seqPlaying = [pageNow(), info(42) & 255, info(7), from(0)];
    tap(B.PLAY);
    turn(3, 3);                                     // REC LEN FREE
    turn(2, 1);                                     // row 2
    r.step();
    g.free = { page: pageNow(), open: info(42) & 255, length: info(39, 0, 1) };
    r.step();
    turn(3, -3);
    turn(2, -2);                                    // the SONG row
    r.step();
    g.songRow = [pageNow(), info(42) & 255];
    turn(2, 1);
    tap(B.PLAY);
    r.step();
    g.playing = [pageNow(), info(42) & 255];
    tap(B.PLAY);

    // 6. Playback follows the view. Stopped, the change is at once. Playing,
    // every track goes at the next bar line and the transport runs on.
    const f = o.follow = {};
    f.gridStopped = all(from);
    tap(B.SEQ);
    f.songStopped = all(from);
    c.sim_song_capture(1);
    tap(B.PLAY);
    render(100);
    tap(B.SEQ);                                     // the grid
    f.gridWaiting = all(waiting);
    const stopAt = info(45, 0) - (info(43) - info(8));
    let n = 0;
    while (waiting(0) !== 0xfd && n++ < 2000) render(1);
    f.gridAfter = all(from);
    const heardFrom = r.heard().length;
    render(800);
    f.inGrid = { notes: r.heard().slice(heardFrom).filter(h => !h.kind && h.velocity).length, playing: info(7), bar: stopAt % 1920 };
    tap(B.SEQ);                                     // the song view
    f.songWaiting = all(waiting);
    n = 0;
    while (waiting(0) !== 0xfd && n++ < 2000) render(1);
    f.songAfter = all(from);
    const backFrom = r.heard().length;
    render(800);
    f.inSong = new Set(r.heard().slice(backFrom).filter(h => !h.kind && h.velocity).map(h => h.track)).size;
    tap(B.PLAY);
    c.sim_song_capture(0);

    // 7. A block in the song view: track 8's clip (a quarter at its start)
    // copied in the grid and pasted over bar 1, where the drums have notes of
    // their own, 4 bars across, track 8 muted on the mix page so its own
    // notes show dim. The block is in the track's full color, the label's,
    // with the clip's note and number on it in black, and none of the
    // track's own notes, which show after it.
    const k = o.block = {};
    tap(B.SEQ);
    turn(0, 7);                                     // track 8, row 1
    tap(B.SCL); r.step();
    held(B.FX, () => key(4));                       // quarters
    key(0);
    tap(B.SEQ);
    held(B.FX, () => key(0));                       // copy the cell
    tap(B.HOME); tap(B.GLO); render(8);             // the mix page: mute track 8
    key(12); render(8);
    k.muted = r.state()[8 + 7 * 20 + 4];
    tap(B.GLO); render(8);
    tap(B.SEQ);
    k.page = pageNow();
    held(B.FX, () => tap(B.PLAY));                  // the top
    held(B.FX, () => key(6));                       // paste over: a block at bar 1
    turn(5, 2);                                     // 4 bars across
    turn(2, 8);                                     // the playhead to bar 3, out of the way
    render(4);
    const lcd = r.pixels(), y0 = 48 + 7 * 18, x1 = 22 + 52;  /* the block's bar, less the column before the next bar line */
    k.label = lcd[(y0 + 1) * 240 + 4];
    k.inside = lcd[(y0 + 12) * 240 + 60];
    const other = new Set();
    let black = 0, dim = 0;
    for (let y = y0; y < y0 + 16; y++)
      for (let x = 23; x < x1; x++) { const p = lcd[y * 240 + x]; if (p === 0) black += x >= 29 && x < 35; else if (p !== k.label) other.add(p); }
    for (let y = y0; y < y0 + 16; y++) for (let x = 77; x < 128; x++) dim += lcd[y * 240 + x] === DIM; /* bar 2 */
    k.other = [...other];
    k.black = black;
    k.dimAfter = dim;
    k.png = png();
    return o;
  });
  savePng(root, 'rec-settings', out.settings, 'png');
  savePng(root, 'rec-block', out.block, 'png');
  await page.close();
  const t = checker('rec'), q = out, v = q.view, s = q.settings, m = q.mode, g = q.grid, f = q.follow, k = q.block;
  t.eq('the song view', v.page, 19);
  t.eq('a new song records in OVERDUB', v.mode, 2);
  t.eq('the song view\'s bottom line ends with OVERDUB in REC\'s red', v.shown, { left: 234 - 6 * 7, right: 232 });
  t.eq('REC tapped, stopped: a take at once, in the count-in, still the song view, nothing kept quiet', v.take,
    { recording: 1, playing: 1, page: 19, quiet: 255 });
  t.eq('REC in the count-in calls it off and stops', v.calledOff, { recording: 0, playing: 0 });
  t.eq('REC tapped while playing: no take, playing on', v.whilePlaying, { recording: 0, playing: 1, page: 19 });
  t.eq('SHIFT+REC while playing: no step entry', v.stepWhilePlaying, 19);
  t.eq('SHIFT+REC stopped: step entry at the playhead', v.step, { page: 20, pointer: 0 });
  t.eq('SHIFT+REC leaves step entry', v.stepLeft, 19);
  t.eq('REC held: the record settings', s.page, 23);
  t.eq('they open on the song\'s mode, OVERDUB, highlighted and red', s.start, ['MODE OVERDUB', 2, 1, 1]);
  t.eq('K1: REPLACE, OVERDUB, PUNCH, LOOP, then STEP, which leaves LOOP the song\'s mode', s.list, [
    ['MODE REPLACE', 1, 0, 0], ['MODE OVERDUB', 2, 1, 1], ['MODE PUNCH', 3, 2, 2], ['MODE LOOP', 4, 3, 3], ['MODE STEP', 4, 4, 3]]);
  t.eq('HOME goes back to the song view, LOOP kept', s.home, [19, 4]);
  t.eq('SHIFT+SEL from LOOP: OVERDUB', s.fromLoop, 2);
  t.eq('REC held in the grid: the record settings', s.fromGrid, 23);
  t.eq('HOME goes back to the grid', s.backToGrid, 27);
  t.eq('the automation view', s.auto, 24);
  t.eq('its bottom line has no REC mode', s.autoShown, -1);
  t.eq('REC held there: the record settings', s.fromAuto, 23);
  t.eq('HOME goes back to the automation view', s.backToAuto, 24);
  t.eq('SHIFT+REC there: step entry, and SHIFT+REC goes back to the automation view', s.stepFromAuto, [20, 24]);
  t.eq('SHIFT+SEL: REPLACE, shown in red', m.replace, [1, { left: 234 - 6 * 7, right: 232 }]);
  t.eq('the grid\'s bottom line shows it too', m.grid, { left: 234 - 6 * 7, right: 232 });
  t.eq('SHIFT+SEL again after saving: OVERDUB', m.changed, 2);
  t.eq('loaded back: REPLACE, as saved', m.loaded, [1, 19]);
  t.eq('and SHIFT+SEL: OVERDUB again', m.back, 2);
  t.eq('the grid\'s SHIFT+REC on an empty cell: step entry at the start of a new 4-bar clip', g.empty,
    { page: 20, open: 8, length: 7680, pointer: 0 });
  t.eq('SHIFT+REC leaves step entry for the grid it came from, the clip closed', g.closed, [27, 0]);
  t.eq('on the clip now there: step entry in it, its length kept', g.again, { page: 20, open: 8, length: 7680 });
  t.eq('PLAY plays the clip and stays in step entry (0.5.0)', g.played, [20, 8, 1]);
  t.eq('SEQ goes back to the grid, the clip launched and still playing', g.seqPlaying, [27, 0, 1, 0]);
  t.eq('REC LEN FREE: a new clip 4 bars long', g.free, { page: 20, open: 9, length: 7680 });
  t.eq('on the SONG row: no step entry', g.songRow, [27, 0]);
  t.eq('playing: no step entry', g.playing, [27, 0]);
  t.eq('stopped in the grid, every track stopped', f.gridStopped, Array(8).fill(0xfe));
  t.eq('stopped in the song view, every track on the song', f.songStopped, Array(8).fill(0xff));
  t.eq('playing, SEQ to the grid: every track waits to stop', f.gridWaiting, Array(8).fill(0xfe));
  t.eq('at the bar line every track is stopped', f.gridAfter, Array(8).fill(0xfe));
  t.eq('in the grid, nothing launched: silence, the transport running on, from a bar line', f.inGrid,
    { notes: 0, playing: 1, bar: 0 });
  t.eq('SEQ to the song view: every track waits to go back to the song', f.songWaiting, Array(8).fill(0xff));
  t.eq('at the bar line every track plays the song', f.songAfter, Array(8).fill(0xff));
  t.atLeast('the song heard again, on most tracks', f.inSong, 6);
  t.eq('the song view, track 8 muted', [k.page, k.muted], [19, 1]);
  t.eq('the block is in the track\'s full color, as its label', k.inside, k.label);
  t.eq('the track\'s color is track 8\'s', k.label, 0xfba5);
  t.eq('on the block only its color and black: no dim notes of the track\'s own', k.other, []);
  t.atLeast('black on it right of its number: the clip\'s note', k.black, 6);
  t.over('the track\'s own notes show dim after it', k.dimAfter, 10);
  return { phase: 'rec', checks: t.list, out };
}
